#include "driver/gpio.h"
#include "esp/espio.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <matrix/Hub75.h>
#include <matrix/hub75_driver.h>
#include <stdio.h>

#define HIGH 1
#define LOW 0
#define PANEL_WIDTH 64

void test_hub(int row)
{
    gpio_set_level(PIN_OE, HIGH);

    for (int x = 0; x < 64; x++)
    {
        if (x >= 32)
        {
            gpio_set_level(PIN_R1, HIGH);
            gpio_set_level(PIN_G1, LOW);
            gpio_set_level(PIN_B1, LOW);

            gpio_set_level(PIN_R2, LOW);
            gpio_set_level(PIN_G2, HIGH);
            gpio_set_level(PIN_B2, LOW);
        }
        else
        {
            gpio_set_level(PIN_R1, LOW);
            gpio_set_level(PIN_G1, LOW);
            gpio_set_level(PIN_B1, HIGH);

            gpio_set_level(PIN_R2, HIGH);
            gpio_set_level(PIN_G2, HIGH);
            gpio_set_level(PIN_B2, LOW);
        }

        esp_rom_delay_us(5);
        gpio_set_level(PIN_CLK, HIGH);
        esp_rom_delay_us(5);
        gpio_set_level(PIN_CLK, LOW);
    }

    gpio_set_level(PIN_A, ((row & 0x01) > 0) ? HIGH : LOW);
    gpio_set_level(PIN_B, ((row & 0x02) > 0) ? HIGH : LOW);
    gpio_set_level(PIN_C, ((row & 0x04) > 0) ? HIGH : LOW);
    gpio_set_level(PIN_D, ((row & 0x08) > 0) ? HIGH : LOW);

    esp_rom_delay_us(5);
    gpio_set_level(PIN_LAT, LOW);
    esp_rom_delay_us(5);
    gpio_set_level(PIN_OE, LOW);
    esp_rom_delay_us(100);
}

Hub75Driver m1;
Hub75 h1;

void drawRect(int x, int y, int w, int h, int col)
{
    for (int i = 0; i < w; i++)
    {
        for (int j = 0; j < h; j++)
        {
            h1.drawPixel(x + i, y + j, col);
        }
    }
}

extern "C" void app_main(void)
{

    Hub75Driver::Config cfg;
    cfg.clock_div_num = 10; // 16 MHz CLK
    cfg.clk_invert = false; // flip if the panel needs inverted CLK
    cfg.swap_byte_order = false;
    cfg.oe_start = 0; // output enable window: columns 8..55
    cfg.oe_end = 63;
    m1.init(cfg);

    drawRect(3, 3, 4, 5, 1);
    drawRect(8, 14, 12, 15, 2);
    drawRect(18, 11, 5, 20, 3);
    drawRect(25, 17, 7, 3, 4);
    drawRect(48, 0, 20, 5, 5);
    drawRect(37, 25, 11, 4, 6);

    m1.set_frame_buffer(h1.commitFrame());

    /*// Force control lines to known states
    gpio_set_level(PIN_CLK, LOW);
    gpio_set_level(PIN_LAT, LOW);

    gpio_set_level(PIN_A, LOW);
    gpio_set_level(PIN_B, LOW);
    gpio_set_level(PIN_C, LOW);
    gpio_set_level(PIN_D, LOW);

    // Now configure them as outputs
    gpio_num_t pins[] = {PIN_R1, PIN_G1, PIN_B1, PIN_R2, PIN_G2, PIN_B2, PIN_CLK, PIN_LAT, PIN_OE, PIN_A, PIN_B, PIN_C, PIN_D};

    gpio_set_level(PIN_LAT, LOW);
    esp_rom_delay_us(100);
    gpio_set_level(PIN_LAT, HIGH);
    esp_rom_delay_us(100);
    gpio_set_level(PIN_LAT, LOW);
    esp_rom_delay_us(100);

    for (int i = 0; i < 13; i++)
    {
        configure_output_pin(pins[i]);
    }*/

    const TickType_t delay_period = pdMS_TO_TICKS(10);

    TickType_t last_wake_time = xTaskGetTickCount();

    for (;;)
    {
        for (int i = 0; i < 16; i++)
        {
            vTaskDelayUntil(&last_wake_time, delay_period);
            // test_hub(i);
        }
    }
}
