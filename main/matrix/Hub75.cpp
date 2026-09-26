#include "matrix/Hub75.h"

Hub75::Hub75()
{
    for (uint8_t i = 0; i < 3; i++)
    {
        clearScreen(i);
    }
}

void Hub75::drawPixel(int16_t x, int16_t y, uint8_t col)
{
    if (x < 0 || x >= PANEL_WIDTH || y < 0 || y >= PANEL_HEIGHT)
    {
        return;
    }
    _screenBuffer[_drawingFrame][y * PANEL_WIDTH + x] = col;
}

uint8_t *Hub75::commitFrame()
{
    if (_pendingFrame < 0)
    {
        // No current pending frame, make the current drawing frame the pending one
        // and set the drawing frame to the idle frame
        _pendingFrame = _drawingFrame;
        _drawingFrame = _idleFrame;
    }
    else
    {
        // Pending frame exists, but we have a new frame so make that pending and use the
        // previous pending frame for drawing
        int8_t pending = _pendingFrame;

        _pendingFrame = _drawingFrame;
        _drawingFrame = pending;
    }

    clearScreen(_drawingFrame);

    return &_screenBuffer[_pendingFrame][0];
}
