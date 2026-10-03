#ifndef TFT_H
#define TFT_H

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "tft/adafruit/Adafruit_GFX.h"
#include "engine/ctetris.h"

#define LCD_HOST SPI2_HOST
#define PIN_NUM_SCLK GPIO_NUM_21
#define PIN_NUM_MOSI GPIO_NUM_47
#define PIN_NUM_LCD_RST GPIO_NUM_48
#define PIN_NUM_LCD_DC GPIO_NUM_45
#define LCD_H_RES 240
#define LCD_V_RES 240
#define LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000) // 20 MHz

class TFT : public GFXcanvas16
{
public:
    TFT() : GFXcanvas16(LCD_H_RES, LCD_V_RES)
    {
    }

    void init()
    {
        buffer = (uint16_t *)heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_DMA);

        const char *TAG = "ST7789_EXAMPLE";

        ESP_LOGI(TAG, "Initializing SPI bus...");
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = PIN_NUM_SCLK;
        buscfg.mosi_io_num = PIN_NUM_MOSI;
        buscfg.miso_io_num = -1;
        buscfg.quadwp_io_num = -1;
        buscfg.quadhd_io_num = -1;
        buscfg.max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t);

        ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

        ESP_LOGI(TAG, "Attaching panel IO to SPI bus...");
        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.dc_gpio_num = PIN_NUM_LCD_DC;
        io_config.cs_gpio_num = -1;
        io_config.pclk_hz = LCD_PIXEL_CLOCK_HZ;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        io_config.spi_mode = 3;
        io_config.trans_queue_depth = 10;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

        ESP_LOGI(TAG, "Installing ST7789 driver...");
        _panel_handle = NULL;
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = PIN_NUM_LCD_RST;
        panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;
        panel_config.bits_per_pixel = 16;

        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &_panel_handle));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(_panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(_panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(_panel_handle, true));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(_panel_handle, true));

        fillRect(0, 0, 240, 240, 0x0000);
        commit();
    }

    virtual void drawPixel(int16_t x, int16_t y, uint16_t color)
    {
        buffer[y * LCD_H_RES + x] = color;
    }

    void commit()
    {
        esp_lcd_panel_draw_bitmap(_panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, buffer);
    }

    void commit(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
    {
        esp_lcd_panel_draw_bitmap(_panel_handle, x1, y1, x2, y2, buffer);
    }

private:
    esp_lcd_panel_handle_t _panel_handle;
};

#endif