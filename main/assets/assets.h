#ifndef ASSETS_H
#define ASSETS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    extern const uint8_t game_over_wav[];
    extern const uint8_t level_up_jingle_wav[];
    extern const uint8_t line_clear_wav[];
    extern const uint8_t piece_falling_after_line_clear_wav[];
    extern const uint8_t piece_landed_wav[];
    extern const uint8_t rotate_piece_wav[];
    extern const uint8_t tetris_4_lines_wav[];

    extern const uint32_t game_over_wav_len;
    extern const uint32_t level_up_jingle_wav_len;
    extern const uint32_t line_clear_wav_len;
    extern const uint32_t piece_falling_after_line_clear_wav_len;
    extern const uint32_t piece_landed_wav_len;
    extern const uint32_t rotate_piece_wav_len;
    extern const uint32_t tetris_4_lines_wav_len;

#ifdef __cplusplus
}
#endif

#endif
