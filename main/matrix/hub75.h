#ifndef Hub75_h_
#define Hub75_h_

#include <driver/gpio.h>
#include <string.h>
#include "matrix/hub75_driver.h"

#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32

#define COL_BLACK 0x00
#define COL_RED 0x30
#define COL_GREEN 0x0C
#define COL_BLUE 0x03
#define COL_CYAN (COL_GREEN | COL_BLUE)
#define COL_MAGENTA (COL_RED | COL_BLUE)
#define COL_YELLOW (COL_RED | COL_GREEN)
#define COL_WHITE (COL_RED | COL_BLUE | COL_GREEN)
#define COL_ORANGE 0x34
#define COL_LIGHT_GREY 0x2A
#define COL_DARK_GREY 0x15

class Hub75
{
public:
    Hub75(Hub75Driver *driver)
    {
        _driver = driver;

        clear_screen();
    }

    void draw_pixel(int16_t x, int16_t y, uint8_t col)
    {
        if (_invert)
        {
            x = x & 0x1F;
            y = y & 0x3F;

            _frame_buffer[(x * PANEL_WIDTH) + y] = col;
        }
        else
        {
            x = x & 0x3F;
            y = y & 0x1F;

            _frame_buffer[(y * PANEL_WIDTH) + x] = col;
        }
    }

    void commit_frame()
    {
        _driver->set_frame_buffer(&_frame_buffer[0]);
    }

    void draw_rect(int x, int y, int w, int h, int col)
    {
        for (int i = 0; i < w; i++)
        {
            for (int j = 0; j < h; j++)
            {
                draw_pixel(x + i, y + j, col);
            }
        }
    }

    void fill_rect(int x, int y, int w, int h, int col)
    {
        for (int i = 0; i < w; i++)
        {
            for (int j = 0; j < h; j++)
            {
                draw_pixel(x + i, y + j, col);
            }
        }
    }

    inline int16_t width()
    {
        return PANEL_WIDTH;
    }
    inline int16_t height()
    {
        return PANEL_HEIGHT;
    }

    void clear_screen()
    {
        memset(&_frame_buffer[0], 0, PANEL_HEIGHT * PANEL_WIDTH);
    }

private:
    uint8_t _frame_buffer[PANEL_HEIGHT * PANEL_WIDTH * 2];
    bool _invert = true;
    Hub75Driver *_driver;
};

#endif