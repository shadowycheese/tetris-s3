#include "hub75_driver.h"
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_log.h>
#include <esp_private/gdma.h>
#include <esp_private/periph_ctrl.h>
#include <esp_rom_gpio.h>
#include <esp_rom_sys.h>
#include <hal/gdma_ll.h>
#include <hal/gpio_hal.h>
#include <soc/gpio_sig_map.h>
#include <soc/lcd_cam_struct.h>

#include <esp_private/gpio.h>

static const char *TAG = "HUB75";

Hub75Driver::Hub75Driver() = default;

Hub75Driver::~Hub75Driver()
{
    deinit();
}

bool Hub75Driver::init(const Hub75DriverConfig &cfg)
{
    _driver_cfg = cfg;

    ESP_LOGI(TAG, "Initializing LCD_CAM (I8080 16-bit) + GDMA for HUB75 64x32");

    periph_module_enable(PERIPH_LCD_CAM_MODULE);
    periph_module_reset(PERIPH_LCD_CAM_MODULE);

    LCD_CAM.lcd_user.lcd_reset = 1;

    esp_rom_delay_us(1000);

    configure_lcd_clock();
    configure_lcd_mode();
    configure_gpio();

    gdma_channel_alloc_config_t alloc_cfg = {};
    gdma_channel_handle_t chan = NULL;

    esp_err_t err = gdma_new_ahb_channel(&alloc_cfg, &chan);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "gdma_new_channel: %s", esp_err_to_name(err));
        return false;
    }
    _dma_chan = chan;

    ESP_ERROR_CHECK(gdma_connect(chan, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0)));

    gdma_strategy_config_t strategy = {};
    gdma_apply_strategy(chan, &strategy);

    gdma_transfer_config_t transfer = {
        .max_data_burst_size = 32,
        .access_ext_mem = 0,
    };
    err = gdma_config_transfer(chan, &transfer);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "gdma transfer config: %s", esp_err_to_name(err));
        return false;
    }

    if (!allocate_dma())
    {
        return false;
    }

    build_descriptors();
    encode_frame(nullptr); // blank

    start();
    ESP_LOGI(TAG, "Running: %lu Hz clock, OE window [%d,%d)",
             (unsigned long)(160000000 / _driver_cfg.clock_div_num),
             _driver_cfg.oe_start,
             _driver_cfg.oe_end);

    return true;
}

void Hub75Driver::deinit()
{
    stop();
    if (_dma_chan)
    {
        gdma_disconnect((gdma_channel_handle_t)_dma_chan);
        gdma_del_channel((gdma_channel_handle_t)_dma_chan);
        _dma_chan = nullptr;
    }
    if (_descriptors)
    {
        heap_caps_free(_descriptors);
        _descriptors = nullptr;
    }
    if (_dma_buf)
    {
        heap_caps_free(_dma_buf);
        _dma_buf = nullptr;
    }
    periph_module_disable(PERIPH_LCD_CAM_MODULE);
}

void Hub75Driver::configure_lcd_clock()
{
    LCD_CAM.lcd_clock.lcd_clk_sel = 3;     // PLL_F160M source
    LCD_CAM.lcd_clock.lcd_ck_out_edge = 0; // PCLK low in first half of cycle
    LCD_CAM.lcd_clock.lcd_ck_idle_edge = _driver_cfg.clk_invert ? 1 : 0;
    LCD_CAM.lcd_clock.lcd_clkcnt_n = 1;
    LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 1; // integer divider only
    LCD_CAM.lcd_clock.lcd_clkm_div_num = _driver_cfg.clock_div_num;
    LCD_CAM.lcd_clock.lcd_clkm_div_a = 1;
    LCD_CAM.lcd_clock.lcd_clkm_div_b = 0;
}

void Hub75Driver::configure_lcd_mode()
{
    LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 0; // I8080 mode, not RGB/DPI
    LCD_CAM.lcd_rgb_yuv.lcd_conv_bypass = 0;
    LCD_CAM.lcd_misc.lcd_next_frame_en = 0; // no auto-reframe
    LCD_CAM.lcd_misc.lcd_bk_en = 1;
    LCD_CAM.lcd_misc.lcd_vfk_cyclelen = 0;
    LCD_CAM.lcd_misc.lcd_vbk_cyclelen = 0;
    LCD_CAM.lcd_data_dout_mode.val = 0;

    LCD_CAM.lcd_user.lcd_always_out_en = 1; // arbitrary-length continuous mode
    LCD_CAM.lcd_user.lcd_8bits_order = _driver_cfg.swap_byte_order ? 1 : 0;
    LCD_CAM.lcd_user.lcd_bit_order = 0;
    LCD_CAM.lcd_user.lcd_2byte_en = 1; // 16-bit bus
    LCD_CAM.lcd_user.lcd_dout = 1;
    LCD_CAM.lcd_user.lcd_dummy = 1; // dummy phase so DMA triggers reliably
    LCD_CAM.lcd_user.lcd_dummy_cyclelen = 1;
    LCD_CAM.lcd_user.lcd_cmd = 0;
    LCD_CAM.lcd_user.lcd_start = 0;
    LCD_CAM.lcd_misc.lcd_afifo_reset = 1; // clear stale TX FIFO
}

void Hub75Driver::configure_gpio()
{
    static const struct
    {
        gpio_num_t pin;
        int idx;
    } data_map[] = {
        {PIN_R1, BitR1},
        {PIN_G1, BitG1},
        {PIN_B1, BitB1},
        {PIN_R2, BitR2},
        {PIN_G2, BitG2},
        {PIN_B2, BitB2},
        {PIN_A, AddrShift},
        {PIN_B, AddrShift + 1},
        {PIN_C, AddrShift + 2},
        {PIN_D, AddrShift + 3},
        {PIN_LAT, BitLAT},
        {PIN_OE, BitOE},
    };

    for (auto &m : data_map)
    {
        esp_rom_gpio_connect_out_signal(m.pin, LCD_DATA_OUT0_IDX + m.idx, false, false);
        gpio_func_sel(m.pin, PIN_FUNC_GPIO);
        gpio_set_drive_capability(m.pin, GPIO_DRIVE_CAP_3);
    }

    esp_rom_gpio_connect_out_signal(PIN_CLK, LCD_PCLK_IDX, _driver_cfg.clk_invert, false);
    gpio_func_sel(PIN_CLK, PIN_FUNC_GPIO);
    gpio_set_drive_capability(PIN_CLK, GPIO_DRIVE_CAP_3);
}

bool Hub75Driver::allocate_dma()
{
    _dma_buf = (uint16_t *)heap_caps_aligned_alloc(4, _dma_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!_dma_buf)
    {
        ESP_LOGE(TAG, "no DMA buffer memory (%d bytes)", _dma_bytes);
        return false;
    }

    _descriptors = heap_caps_aligned_calloc(16, _row_groups, sizeof(dma_descriptor_t), MALLOC_CAP_DMA);
    if (!_descriptors)
    {
        ESP_LOGE(TAG, "no descriptor memory");
        return false;
    }
    return true;
}

void Hub75Driver::build_descriptors()
{
    dma_descriptor_t *desc = (dma_descriptor_t *)_descriptors;
    for (int g = 0; g < _row_groups; g++)
    {
        desc[g].dw0.size = _row_words * sizeof(uint16_t);
        desc[g].dw0.length = _row_words * sizeof(uint16_t);
        desc[g].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
        desc[g].dw0.suc_eof = (g == _row_groups - 1);
        desc[g].buffer = (uint8_t *)&_dma_buf[g * _row_words];
        desc[g].next = (g == _row_groups - 1) ? &desc[0] : &desc[g + 1];
    }
}

void Hub75Driver::encode_frame(const uint8_t *fb)
{
    constexpr uint8_t rgbMask = 0x07; // bit0=R bit1=G bit2=B
    const uint16_t blank = (1u << BitOE);

    for (int g = 0; g < _row_groups; g++)
    {
        const uint16_t base = ((uint16_t)g << AddrShift);
        for (int x = 0; x < _row_words; x++)
        {
            uint16_t w = base | blank;
            if (x >= _driver_cfg.oe_start && x < _driver_cfg.oe_end)
            {
                w &= ~(1u << BitOE); // outputs enabled during this window
            }

            if (fb)
            {
                const uint8_t top = g; // (fb[(size_t)g * kWidth + x] & kRgbMask);                // rows 0..15
                const uint8_t bot = x; //(fb[(size_t)(g + kRowGroups) * kWidth + x] & kRgbMask); // rows 16..31
                w |= (top & 1) ? (1 << BitR1) : 0;
                w |= (top & 2) ? (1 << BitG1) : 0;
                w |= (top & 4) ? (1 << BitB1) : 0;
                w |= (bot & 1) ? (1 << BitR2) : 0;
                w |= (bot & 2) ? (1 << BitG2) : 0;
                w |= (bot & 4) ? (1 << BitB2) : 0;
            }

            if (x == _row_words - 1)
            {
                w |= (1u << BitLAT); // latch at end of shift
            }
            _dma_buf[g * _row_words + x] = w;
        }
    }
}

void Hub75Driver::set_frame_buffer(const uint8_t *fb_64x32)
{
    encode_frame(fb_64x32);
}

void Hub75Driver::start()
{
    LCD_CAM.lcd_user.lcd_update = 1;
    esp_rom_delay_us(10);
    gdma_start((gdma_channel_handle_t)_dma_chan, (intptr_t)_descriptors);
    esp_rom_delay_us(100);
    LCD_CAM.lcd_user.lcd_start = 1;
}

void Hub75Driver::stop()
{
    if (!_dma_chan)
    {
        return;
    }
    LCD_CAM.lcd_user.lcd_start = 0;
    LCD_CAM.lcd_user.lcd_update = 1;
    gdma_stop((gdma_channel_handle_t)_dma_chan);
}