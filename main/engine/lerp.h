#ifndef LERP_H
#define LERP_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int x;
    int y;
    int w;
    int h;
} rectangle_t;

typedef struct
{
    int x;
    int y;
} point_t;

#ifdef __cplusplus
extern "C"
{
#endif
    uint8_t color_lerp(uint8_t col1, uint8_t col2, float factor);
    point_t point_lerp(point_t p1, point_t p2, float factor);

#ifdef __cplusplus
}
#endif

#endif