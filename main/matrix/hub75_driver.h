#pragma once

#include "hal/dma_types.h"
#include <driver/gpio.h>
#include <stddef.h>
#include <stdint.h>

#define PIN_R1 GPIO_NUM_8
#define PIN_G1 GPIO_NUM_13
#define PIN_B1 GPIO_NUM_14
#define PIN_R2 GPIO_NUM_12
#define PIN_G2 GPIO_NUM_10
#define PIN_B2 GPIO_NUM_11
#define PIN_CLK GPIO_NUM_9
#define PIN_LAT GPIO_NUM_3
#define PIN_OE GPIO_NUM_5
#define PIN_A GPIO_NUM_4
#define PIN_B GPIO_NUM_15
#define PIN_C GPIO_NUM_7
#define PIN_D GPIO_NUM_6

class Hub75Driver
{
public:
    struct Hub75DriverConfig
    {
        uint32_t clock_div_num; // PCLK = PLL_F160M / clock_div_num (>= 2)
        bool clk_invert;        // true if the panel wants inverted CLK polarity
        bool swap_byte_order;   // set LCD_CAM lcd_8bits_order if bytes come out swapped
    };

    static constexpr int _width = 64;
    static constexpr int _height = 32;
    static constexpr int _rows = 16;
    static constexpr int _row_words = 3 * _width;
    static constexpr int _dma_words = _rows * _row_words;
    static constexpr int _dma_bytes = _dma_words * sizeof(uint16_t);

    static constexpr int _oe_start = 0;
    static constexpr int _oe_end = _width - 1;

    Hub75Driver();
    ~Hub75Driver();

    bool init();
    void deinit();

    void set_frame_buffer(const uint8_t *fb_64x32);

    void start();
    void stop();

private:
    enum : uint16_t
    {
        BitR1 = 0,
        BitG1 = 1,
        BitB1 = 2,
        BitR2 = 3,
        BitG2 = 4,
        BitB2 = 5,
        AddrShift = 6,
        BitLAT = 11,
        BitOE = 12,
    };

    void configure_lcd_clock();
    void configure_lcd_mode();
    void configure_gpio();
    bool allocate_dma();
    void build_descriptors();
    void encode_frame(const uint8_t *fb_64x32);

    Hub75DriverConfig _driver_cfg{};

    void *_dma_chan = nullptr;
    void *_descriptors = nullptr;
    uint16_t *_dma_buf = nullptr;
};