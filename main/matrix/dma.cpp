#include "dma.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_private/gdma.h>
#include <esp_private/gpio.h>
#include <esp_private/periph_ctrl.h>
#include <esp_rom_gpio.h>
#include <esp_rom_sys.h>
#include <hal/gdma_ll.h>
#include <hal/gpio_hal.h>
#include <soc/gpio_sig_map.h>
#include <soc/lcd_cam_struct.h>
#if HUB75_EXTERNAL_FRAMEBUFFERS
#include <esp_cache.h>
#endif

static const char *const TAG = "HUBDMA";

// HUB75 16-bit word layout for LCD_CAM peripheral
// Bit layout: [--|--|OE|LAT|ADDR(5-bit)|R2|G2|B2|R1|G1|B1]
enum HUB75WordBits : uint16_t
{
    // RGB data bits
    R1_BIT = 0, // Upper half red
    G1_BIT = 1, // Upper half green
    B1_BIT = 2, // Upper half blue
    R2_BIT = 3, // Lower half red
    G2_BIT = 4, // Lower half green
    B2_BIT = 5, // Lower half blue
    // Bits 6-10: Row address (5-bit field, shifted << 6)
    LAT_BIT = 11, // Latch signal
    OE_BIT = 12,  // Output Enable (active low)
                  // Bits 13-15: Unused
};

// Address field (not individual bits)
constexpr int ADDR_SHIFT = 6;
constexpr uint16_t ADDR_MASK = 0x1F; // 5-bit address (0-31)
constexpr uint16_t SM5368_CLK_MASK = 1u << (ADDR_SHIFT + 0);
constexpr uint16_t SM5368_BK_MASK = 1u << (ADDR_SHIFT + 1);
constexpr uint16_t SM5368_DATA_MASK = 1u << (ADDR_SHIFT + 2);

// Combined RGB masks
constexpr uint16_t RGB_UPPER_MASK = (1 << R1_BIT) | (1 << G1_BIT) | (1 << B1_BIT);
constexpr uint16_t RGB_LOWER_MASK = (1 << R2_BIT) | (1 << G2_BIT) | (1 << B2_BIT);
constexpr uint16_t RGB_MASK = RGB_UPPER_MASK | RGB_LOWER_MASK; // 0x003F

// Bit clear masks
constexpr uint16_t OE_CLEAR_MASK = ~(1 << OE_BIT);

GdmaDma::GdmaDma(const Hub75Config &config)
    : dma_chan_(nullptr), bit_depth_(HUB75_BIT_DEPTH), lsbMsbTransitionBit_(0),
      actual_clock_hz_(resolve_actual_clock_speed(config.output_clock_speed)), panel_width_(config.panel_width), panel_height_(config.panel_height),
      layout_rows_(config.layout_rows), layout_cols_(config.layout_cols), virtual_width_(config.panel_width * config.layout_cols),
      virtual_height_(config.panel_height * config.layout_rows),
      // Use helper function to compute DMA width (doubles for four-scan panels)
      dma_width_(get_effective_dma_width(config.scan_wiring, config.panel_width, config.layout_rows, config.layout_cols)),
      scan_wiring_(config.scan_wiring), layout_(config.layout), needs_scan_remap_(config.scan_wiring != Hub75ScanWiring::STANDARD_TWO_SCAN),
      needs_layout_remap_(config.layout != Hub75PanelLayout::HORIZONTAL), rotation_(config.rotation),
      // Use helper function to compute num_rows (halves for four-scan panels)
      num_rows_(get_effective_num_rows(config.scan_wiring, config.panel_height)), dma_buffers_{nullptr, nullptr}, row_buffers_{nullptr, nullptr},
      descriptors_{nullptr, nullptr}, front_idx_(0), active_idx_(0), descriptor_count_(0),
      basis_brightness_(config.brightness), // Use config value (default: 128)
      intensity_(1.0f)
{
    // Zero-copy architecture: DMA buffers ARE the display memory
    // Note: For four-scan panels, dma_width_ is doubled and num_rows_ is halved
    // to match the physical shift register layout
}

GdmaDma::~GdmaDma()
{
    GdmaDma::shutdown();
}

bool GdmaDma::init()
{
    ESP_LOGI(TAG, "Initializing LCD_CAM peripheral with GDMA...");

    // Enable and reset LCD_CAM peripheral
    periph_module_enable(PERIPH_LCD_CAM_MODULE);
    periph_module_reset(PERIPH_LCD_CAM_MODULE);

    // Reset LCD bus
    LCD_CAM.lcd_user.lcd_reset = 1;
    esp_rom_delay_us(1000);

    // Configure LCD clock
    configure_lcd_clock();

    // Configure LCD mode (i8080 16-bit parallel)
    configure_lcd_mode();

    // Configure GPIO routing
    configure_gpio();

    // Allocate GDMA channel
    gdma_channel_alloc_config_t dma_alloc_config = {};
    dma_alloc_config.direction = GDMA_CHANNEL_DIRECTION_TX;
    esp_err_t err = gdma_new_ahb_channel(&dma_alloc_config, &dma_chan_);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to allocate GDMA channel: %s", esp_err_to_name(err));
        return false;
    }

    // Connect GDMA to LCD peripheral
    gdma_connect(dma_chan_, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));

    // Configure GDMA strategy
    // owner_check = false: Static descriptors, no dynamic ownership handshaking needed
    // auto_update_desc = false: No descriptor writeback - prevents corruption with infinite ring
    gdma_strategy_config_t strategy_config = {};
    gdma_apply_strategy(dma_chan_, &strategy_config);

    ESP_LOGI(TAG, "GDMA strategy configured: owner_check=false, auto_update_desc=false");

    // Use the largest PSRAM burst that divides each bitplane (64 bytes for common panels).
    // Keep descriptor starts and lengths aligned without forcing small bursts on every panel.
    const size_t psram_alignment = dma_width_ % 32 == 0 ? 64 : (dma_width_ % 16 == 0 ? 32 : 16);

    // Configure external access explicitly, including encrypted PSRAM on S3.
    gdma_transfer_config_t transfer_config = {.max_data_burst_size = HUB75_EXTERNAL_FRAMEBUFFERS ? psram_alignment : 32u,
                                              .access_ext_mem = HUB75_EXTERNAL_FRAMEBUFFERS != 0};
    err = gdma_config_transfer(dma_chan_, &transfer_config);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure GDMA transfer: %s", esp_err_to_name(err));
        return false;
    }

    // Wait for any pending LCD operations
    while (LCD_CAM.lcd_user.lcd_start)
        ;

    // Post-init cleanup for clean state
    gdma_reset(dma_chan_);
    esp_rom_delay_us(1000);
    LCD_CAM.lcd_user.lcd_dout = 1;        // Enable data out
    LCD_CAM.lcd_user.lcd_update = 1;      // Update registers
    LCD_CAM.lcd_misc.lcd_afifo_reset = 1; // Reset LCD TX FIFO

    // Note: No EOF callback needed with descriptor-chain approach
    // The descriptor chain encodes all timing via repetition counts

    ESP_LOGI(TAG, "GDMA EOF callback registered successfully");
    ESP_LOGI(TAG, "Panel config: %dx%d pixels, %dx%d layout, virtual: %dx%d", panel_width_, panel_height_, layout_cols_, layout_rows_, virtual_width_,
             virtual_height_);
    ESP_LOGI(TAG, "DMA config: %dx%d (width x rows), four-scan: %s", dma_width_, num_rows_, is_four_scan_wiring(scan_wiring_) ? "yes" : "no");
    ESP_LOGI(TAG, "Row decoder: %s", config_.row_decoder == Hub75RowDecoder::SM5368 ? "SM5368" : "binary");

    ESP_LOGI(TAG, "LCD_CAM + GDMA initialized successfully");
    ESP_LOGI(TAG, "Clock: %.2f MHz (requested %u MHz)", actual_clock_hz_ / 1000000.0f,
             (unsigned int)(static_cast<uint32_t>(config_.output_clock_speed) / 1000000));

    // Allocate per-row bit-plane buffers
    if (!allocate_row_buffers())
    {
        return false;
    }

    // Initialize buffers with blank pixels (control bits only, RGB=0)
    initialize_blank_buffers();

    // Initialize brightness remapping coefficients (quadratic curve)
    init_brightness_coeffs(dma_width_, config_.latch_blanking);

    // Set OE bits for BCM control and brightness
    set_brightness_oe();

    // Build descriptor chain (one descriptor per bit plane)
    if (!build_descriptor_chain())
    {
        return false;
    }

    ESP_LOGI(TAG, "Descriptor-chain DMA setup complete");
    return true;
}

HUB75_CONST uint32_t GdmaDma::resolve_actual_clock_speed(Hub75ClockSpeed clock_speed) const
{
    // ESP32-S3 LCD_CAM clock derivation:
    //   Output = PLL_F160M / lcd_clkm_div_num
    //   Constraint: lcd_clkm_div_num >= 2
    //
    // We use integer dividers only - no fractional dividers. Fractional dividers
    // cause clock jitter because the hardware alternates between two integer
    // dividers to approximate the fractional value. With pure integer division
    // from the stable 160 MHz PLL, every clock cycle is identical.
    //
    // The resulting frequencies may not be round numbers (e.g., 160/7 = 22.86 MHz),
    // but this is fine - what matters for signal integrity is that each clock
    // period is exactly the same, not that the frequency is a nice decimal.
    //
    // Available speeds: 32 MHz (div=5), 26.67 MHz (div=6), 22.86 MHz (div=7),
    //                   20 MHz (div=8), 17.78 MHz (div=9), 16 MHz (div=10), ...
    uint32_t requested_hz = static_cast<uint32_t>(clock_speed);
    uint32_t divider = (160000000 + requested_hz / 2) / requested_hz; // Round to nearest
    return 160000000 / std::max(divider, uint32_t{2});
}

void GdmaDma::configure_lcd_clock()
{
    // Configure LCD clock from PLL_F160M (160 MHz)
    // actual_clock_hz_ already resolved in constructor
    uint32_t requested_hz = static_cast<uint32_t>(config_.output_clock_speed);
    uint32_t div_num = 160000000 / actual_clock_hz_;

    if (actual_clock_hz_ != requested_hz)
    {
        ESP_LOGI(TAG, "Clock speed %u Hz rounded to %u Hz (160MHz / %u)", (unsigned int)requested_hz, (unsigned int)actual_clock_hz_,
                 (unsigned int)div_num);
    }

    LCD_CAM.lcd_clock.lcd_clk_sel = 3;     // PLL_F160M_CLK (value 3, not 2!)
    LCD_CAM.lcd_clock.lcd_ck_out_edge = 0; // PCLK low in 1st half cycle
    LCD_CAM.lcd_clock.lcd_ck_idle_edge = config_.clk_phase_inverted ? 1 : 0;
    LCD_CAM.lcd_clock.lcd_clkcnt_n = 1;       // Should never be zero
    LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 1; // PCLK = CLK / 1 (simple divisor)
    LCD_CAM.lcd_clock.lcd_clkm_div_num = div_num;
    LCD_CAM.lcd_clock.lcd_clkm_div_a = 1; // Fractional divider (0/1)
    LCD_CAM.lcd_clock.lcd_clkm_div_b = 0;

    ESP_LOGI(TAG, "LCD clock: PLL_F160M / %u = %.2f MHz", (unsigned int)div_num, actual_clock_hz_ / 1000000.0f);
}

void GdmaDma::configure_lcd_mode()
{
    // Configure LCD in i8080 mode, 16-bit parallel, continuous output
    LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 0;    // i8080 mode (not RGB)
    LCD_CAM.lcd_rgb_yuv.lcd_conv_bypass = 0; // Disable RGB/YUV converter
    LCD_CAM.lcd_misc.lcd_next_frame_en = 0;  // Do NOT auto-frame
    LCD_CAM.lcd_misc.lcd_bk_en = 1;          // Enable blanking
    LCD_CAM.lcd_misc.lcd_vfk_cyclelen = 0;
    LCD_CAM.lcd_misc.lcd_vbk_cyclelen = 0;

    LCD_CAM.lcd_data_dout_mode.val = 0;     // No data delays
    LCD_CAM.lcd_user.lcd_always_out_en = 1; // Enable 'always out' mode for arbitrary-length transfers
    LCD_CAM.lcd_user.lcd_8bits_order = 0;   // Do not swap bytes
    LCD_CAM.lcd_user.lcd_bit_order = 0;     // Do not reverse bit order
    LCD_CAM.lcd_user.lcd_2byte_en = 1;      // 16-bit mode
    LCD_CAM.lcd_user.lcd_dout = 1;          // Enable data output

    // CRITICAL: Dummy phases required for DMA to trigger reliably
    LCD_CAM.lcd_user.lcd_dummy = 1;          // Dummy phase(s) @ LCD start
    LCD_CAM.lcd_user.lcd_dummy_cyclelen = 1; // 1+1 dummy phase
    LCD_CAM.lcd_user.lcd_cmd = 0;            // No command at LCD start

    // Disable start signal
    LCD_CAM.lcd_user.lcd_start = 0;
}

void GdmaDma::configure_gpio()
{
    // 16-bit data pins mapping
    int data_pins[16] = {
        PIN_R0,  // D0
        PIN_G0,  // D1
        PIN_B0,  // D2
        PIN_R1,  // D3
        PIN_R2,  // D4
        PIN_B2,  // D5
        PIN_A,   // D6
        PIN_B,   // D7
        PIN_C,   // D8
        PIN_D,   // D9
        PIN_CLK, // D10
        PIN_LAT, // D11
        PIN_OE,  // D12
        -1,      -1,
        -1 // D13-D15 unused
    };

    // Configure data pins
    for (int i = 0; i < 16; i++)
    {
        if (data_pins[i] >= 0)
        {
            esp_rom_gpio_connect_out_signal(data_pins[i], LCD_DATA_OUT0_IDX + i, false, false);
            gpio_func_sel((gpio_num_t)data_pins[i], PIN_FUNC_GPIO);
            gpio_set_drive_capability((gpio_num_t)data_pins[i], GPIO_DRIVE_CAP_3); // Max drive strength
        }
    }

    // Configure WR (clock) pin
    if (config_.pins.clk >= 0)
    {
        esp_rom_gpio_connect_out_signal(config_.pins.clk, LCD_PCLK_IDX, config_.clk_phase_inverted, false);
        gpio_func_sel((gpio_num_t)config_.pins.clk, PIN_FUNC_GPIO);
        gpio_set_drive_capability((gpio_num_t)config_.pins.clk, GPIO_DRIVE_CAP_3); // Max drive strength
    }

    ESP_LOGD(TAG, "GPIO routing configured");
}

bool GdmaDma::allocate_row_buffers()
{
    size_t pixels_per_bitplane = dma_width_; // DMA buffer width (all panels chained horizontally)
    // Each bitplane uses one descriptor, whose length and size fields are 12 bits.
    if (pixels_per_bitplane * sizeof(uint16_t) > DMA_DESCRIPTOR_BUFFER_MAX_SIZE)
    {
        ESP_LOGE(TAG, "DMA width exceeds the single-descriptor bitplane limit");
        return false;
    }
    size_t buffer_size_per_row = pixels_per_bitplane * bit_depth_ * 2; // uint16_t = 2 bytes
    size_t total_buffer_size = num_rows_ * buffer_size_per_row;
#if HUB75_EXTERNAL_FRAMEBUFFERS
    // Each descriptor's data must satisfy the 16-byte PSRAM transfer alignment.
    if ((pixels_per_bitplane * sizeof(uint16_t)) % 16 != 0)
    {
        ESP_LOGE(TAG, "PSRAM requires DMA width to be a multiple of 8 pixels");
        return false;
    }
    // 64 bytes covers all S3 data-cache line sizes, including legacy IDF.
    total_buffer_size = (total_buffer_size + 63) & ~size_t{63};
#endif
    total_buffer_bytes_ = total_buffer_size;

    // Always allocate first buffer (buffer A, index 0)
#if HUB75_EXTERNAL_FRAMEBUFFERS == 1
    // Older IDF heaps do not advertise MALLOC_CAP_DMA on PSRAM. Align manually.
    static constexpr uint32_t DMA_MEM_CAPS = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    static constexpr size_t DMA_ALIGNMENT = 64;
    ESP_LOGI(TAG, "Allocating buffer A: %zu bytes for %d rows (PSRAM)", total_buffer_size, num_rows_);
#else
    static constexpr uint32_t DMA_MEM_CAPS = MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL;
    static constexpr size_t DMA_ALIGNMENT = 4;
    ESP_LOGI(TAG, "Allocating buffer A: %zu bytes for %d rows (internal RAM)", total_buffer_size, num_rows_);
#endif
    dma_buffers_[0] = (uint8_t *)heap_caps_aligned_calloc(DMA_ALIGNMENT, 1, total_buffer_size, DMA_MEM_CAPS);
    if (!dma_buffers_[0])
    {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes for buffer A", total_buffer_size);
        return false;
    }

    // Allocate metadata array for buffer A
    row_buffers_[0] = new RowBitPlaneBuffer[num_rows_];

    // Point each row's metadata into the single allocation
    uint8_t *current_ptr = dma_buffers_[0];
    for (int row = 0; row < num_rows_; row++)
    {
        row_buffers_[0][row].buffer_size = buffer_size_per_row;
        row_buffers_[0][row].data = current_ptr;
        current_ptr += buffer_size_per_row;
    }

    // Set indices for single-buffer mode (both point to buffer 0)
    front_idx_ = 0;
    active_idx_ = 0;

    ESP_LOGI(TAG, "Buffer A allocated: %d rows × %zu bytes/row = %zu total", num_rows_, buffer_size_per_row, total_buffer_size);

    // Conditionally allocate second buffer (buffer B, index 1)
    if (config_.double_buffer)
    {
        ESP_LOGI(TAG, "Allocating buffer B: %zu bytes (double buffering enabled)", total_buffer_size);
        dma_buffers_[1] = (uint8_t *)heap_caps_aligned_calloc(DMA_ALIGNMENT, 1, total_buffer_size, DMA_MEM_CAPS);
        if (!dma_buffers_[1])
        {
            ESP_LOGE(TAG, "Failed to allocate %zu bytes for buffer B", total_buffer_size);
            // The descriptor setup requires both buffers when double_buffer is set.
            return false;
        }

        // Allocate metadata array for buffer B
        row_buffers_[1] = new RowBitPlaneBuffer[num_rows_];

        // Point each row's metadata into the single allocation
        current_ptr = dma_buffers_[1];
        for (int row = 0; row < num_rows_; row++)
        {
            row_buffers_[1][row].buffer_size = buffer_size_per_row;
            row_buffers_[1][row].data = current_ptr;
            current_ptr += buffer_size_per_row;
        }

        // Set indices for double-buffer mode (front=0, active=1)
        active_idx_ = 1;

        ESP_LOGI(TAG, "Buffer B allocated: %d rows × %zu bytes/row = %zu total (double buffer mode)", num_rows_, buffer_size_per_row,
                 total_buffer_size);
    }

    return true;
}

void GdmaDma::start_transfer()
{
    if (!dma_chan_ || !descriptors_[front_idx_])
    {
        ESP_LOGE(TAG, "DMA channel or descriptors not initialized");
        return;
    }

    ESP_LOGI(TAG, "Starting descriptor-chain DMA:");
    ESP_LOGI(TAG, "  Descriptor count: %zu", descriptor_count_);
    ESP_LOGI(TAG, "  Rows: %d, Bits: %d", num_rows_, bit_depth_);

    // Prime LCD registers
    LCD_CAM.lcd_user.lcd_update = 1;
    esp_rom_delay_us(10);

    // Start GDMA transfer from first descriptor in chain (front buffer)
    gdma_start(dma_chan_, (intptr_t)&descriptors_[front_idx_][0]);

    // Delay before starting LCD
    esp_rom_delay_us(100);

    // Start LCD engine (will run continuously via descriptor loop)
    LCD_CAM.lcd_user.lcd_start = 1;

    ESP_LOGI(TAG, "Descriptor-chain DMA transfer started - running continuously");
}

void GdmaDma::stop_transfer()
{
    if (!dma_chan_)
    {
        return;
    }

    // Disable LCD output
    LCD_CAM.lcd_user.lcd_start = 0;
    LCD_CAM.lcd_user.lcd_update = 1; // Apply the stop command

    gdma_stop(dma_chan_);

    ESP_LOGI(TAG, "DMA transfer stopped");
}

// No EOF callback needed - descriptor chain handles all timing

void GdmaDma::shutdown()
{
    GdmaDma::stop_transfer();

    if (dma_chan_)
    {
        gdma_disconnect(dma_chan_);
        gdma_del_channel(dma_chan_);
        dma_chan_ = nullptr;
    }

    // Free all allocated resources (using array structure)
    for (int i = 0; i < 2; i++)
    {
        // Free descriptor chains
        if (descriptors_[i])
        {
            heap_caps_free(descriptors_[i]);
            descriptors_[i] = nullptr;
        }

        // Free raw DMA buffers (single allocation per buffer)
        if (dma_buffers_[i])
        {
            heap_caps_free(dma_buffers_[i]);
            dma_buffers_[i] = nullptr;
        }

        // Free metadata arrays
        if (row_buffers_[i])
        {
            delete[] row_buffers_[i];
            row_buffers_[i] = nullptr;
        }
    }

    descriptor_count_ = 0;

    periph_module_disable(PERIPH_LCD_CAM_MODULE);

    ESP_LOGI(TAG, "Shutdown complete");
}

bool GdmaDma::build_descriptor_chain_internal(RowBitPlaneBuffer *buffers, dma_descriptor_t *descriptors)
{
    if (!buffers || !descriptors)
    {
        return false;
    }

    size_t pixels_per_bitplane = dma_width_;             // DMA buffer width per bit plane
    size_t bytes_per_bitplane = pixels_per_bitplane * 2; // uint16_t = 2 bytes

    // Link descriptors with BCM repetitions
    size_t desc_idx = 0;
    for (int row = 0; row < num_rows_; row++)
    {
        for (int bit = 0; bit < bit_depth_; bit++)
        {
            uint8_t *const bit_buffer = buffers[row].data + (bit * bytes_per_bitplane);

            // Calculate number of descriptor repetitions for this bit plane
            const int repetitions = (bit <= lsbMsbTransitionBit_) ? 1                                        // Base timing for LSBs
                                                                  : (1 << (bit - lsbMsbTransitionBit_ - 1)); // BCM weighting

            // Create 'repetitions' descriptors, all pointing to the SAME buffer
            // This achieves BCM timing via temporal repetition
            for (int rep = 0; rep < repetitions; rep++)
            {
                dma_descriptor_t *const desc = &descriptors[desc_idx];
                desc->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
                desc->dw0.suc_eof = 0; // EOF only on last descriptor
                desc->dw0.size = bytes_per_bitplane;
                desc->dw0.length = bytes_per_bitplane;
                desc->buffer = bit_buffer; // Same buffer for all repetitions

                // Link to next descriptor
                if (desc_idx < descriptor_count_ - 1)
                {
                    desc->next = &descriptors[desc_idx + 1];
                }

                desc_idx++;
            }
        }
    }

    // Last descriptor loops back to first (continuous refresh)
    descriptors[descriptor_count_ - 1].next = &descriptors[0];
    descriptors[descriptor_count_ - 1].dw0.suc_eof = 1; // Optional: EOF once per frame

    return true;
}

void GdmaDma::flush_cache_to_dma(int buffer_idx)
{
#if HUB75_EXTERNAL_FRAMEBUFFERS
    if (!dma_buffers_[buffer_idx])
    {
        return;
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    // C2M is the default direction, including IDF 5.1 which has no DIR_C2M flag.
    esp_err_t err = esp_cache_msync(dma_buffers_[buffer_idx], total_buffer_bytes_, ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Cache sync failed: %s", esp_err_to_name(err));
    }
#else
    // Legacy IDF: the PSRAM allocation and its extent are cache-line aligned.
    if (Cache_WriteBack_Addr((uint32_t)dma_buffers_[buffer_idx], total_buffer_bytes_) != 0)
    {
        ESP_LOGW(TAG, "PSRAM cache writeback failed");
    }
#endif
#endif
}
