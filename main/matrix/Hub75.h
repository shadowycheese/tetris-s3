#ifndef Hub75_h_
#define Hub75_h_

#include <driver/gpio.h>
#include <string.h>

#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32
#define SECTION_HEIGHT 16

class Hub75
{
    public:
        Hub75();
        void drawPixel(int16_t x, int16_t y, uint8_t col);

        inline int16_t width()
        {
            return PANEL_WIDTH;
        }
        inline int16_t height()
        {
            return PANEL_HEIGHT;
        }

        uint8_t *commitFrame();

    private:
        void clearScreen(int8_t id)
        {
            memset(&_screenBuffer[id], 0, sizeof(FrameBuffer));
        }

        void render();
        void renderTest();

        inline uint8_t r_bits565(uint16_t rgb565)
        {
            return (uint8_t)((rgb565 >> 11) & 0x1F) > 0 ? 0x4 : 0x0;
        }
        inline uint8_t g_bits565(uint16_t rgb565)
        {
            return (uint8_t)((rgb565 >> 5) & 0x3F) > 0 ? 0x2 : 0x0;
        }
        inline uint8_t b_bits565(uint16_t rgb565)
        {
            return (uint8_t)((rgb565) & 0x1F) > 0 ? 0x1 : 0x0;
        }

        inline uint8_t r_set(uint8_t rgb)
        {
            return (rgb & 0x4) > 0 ? 1 : 0;
        }
        inline uint8_t g_set(uint8_t rgb)
        {
            return (rgb & 0x1) > 0 ? 1 : 0;
        }
        inline uint8_t b_set(uint8_t rgb)
        {
            return (rgb & 0x2) > 0 ? 1 : 0;
        }

        void drawPixel(int8_t frame, int16_t x, int16_t y, uint16_t rgb565);

        typedef uint8_t FrameBuffer[PANEL_HEIGHT * (PANEL_WIDTH + 1)];

        FrameBuffer _screenBuffer[4];

        uint16_t _outputRow;
        int8_t _outputFrame;
        int8_t _pendingFrame;
        int8_t _drawingFrame;
        int8_t _idleFrame;

        volatile bool _overlayVisible;

        const int8_t _overlayFrame = 3;

        uint32_t _lastRender;

        friend void refreshDisplay();
};

#endif