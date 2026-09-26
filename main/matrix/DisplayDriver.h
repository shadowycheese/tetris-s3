#ifndef Display_Driver_h
#define Display_Driver_h

#include <stdio.h>

class DisplayDriver
{
    public:
        virtual void drawPixel(int16_t x, int16_t y, uint16_t rgb565);
        virtual int16_t width();
        virtual int16_t height();
        virtual uint8_t *commitFrame();
};

#endif