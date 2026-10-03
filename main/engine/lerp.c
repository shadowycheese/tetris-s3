#include "engine/lerp.h"

uint8_t color_lerp(uint8_t col1, uint8_t col2, float factor)
{
    if (factor < 0.0f)
    {
        factor = 0.0f;
    }
    if (factor > 1.0f)
    {
        factor = 1.0f;
    }

    float r1 = (float)((col1 >> 4) & 0x3);
    float g1 = (float)((col1 >> 2) & 0x3);
    float b1 = (float)((col1) & 0x3);

    float r2 = (float)((col2 >> 4) & 0x3);
    float g2 = (float)((col2 >> 2) & 0x3);
    float b2 = (float)((col2) & 0x3);

    uint8_t r3 = (uint8_t)(r1 + factor * (r2 - r1) + 0.5f);
    uint8_t g3 = (uint8_t)(g1 + factor * (g2 - g1) + 0.5f);
    uint8_t b3 = (uint8_t)(b1 + factor * (b2 - b1) + 0.5f);

    return r3 << 4 | g3 << 2 | b3;
}

point_t point_lerp(point_t p1, point_t p2, float factor)
{
    if (factor < 0.0f)
    {
        factor = 0.0f;
    }
    if (factor > 1.0f)
    {
        factor = 1.0f;
    }

    float x1 = (float)p1.x;
    float y1 = (float)p1.y;

    float x2 = (float)p2.x;
    float y2 = (float)p2.y;

    int x = (int)(x1 + factor * (x2 - x1) + 0.5f);
    int y = (int)(y1 + factor * (y2 - y1) + 0.5f);

    point_t p = {x, y};

    return p;
}
