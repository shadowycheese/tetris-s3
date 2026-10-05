#ifndef UI_H
#define UI_H

#include "tft/tft.h"
#include "events/edt.h"
#include "assets/assets.h"
#include "audio/audio.h"
#include "tft/adafruit/Adafruit_GFX.h"

class UI
{
public:
    UI(TFT *tft, Audio *audio)
    {
        _tft = tft;
        _audio = audio;
    }

    void init()
    {
        edt_add_game_event_handler(EVENT_HARD_DROP, [this](edt_job_t event)
                                   {
                                       update_score(&event);

                                       commit();

                                       //_audio->play_440hz_tone(); //
                                       _audio->play(piece_landed_wav, piece_landed_wav_len); //
                                   });

        edt_add_game_event_handler(EVENT_SOFT_DROP, [this](edt_job_t event)
                                   {
                                       update_score(&event);

                                       commit();

                                       //_audio->play_440hz_tone(); //
                                       //_audio->play(piece_landed_wav, piece_landed_wav_len); //
                                   });

        edt_add_game_event_handler(EVENT_ROTATE, [this](edt_job_t event)
                                   {
                                       update_score(&event);

                                       _audio->play(rotate_piece_wav, rotate_piece_wav_len); //
                                   });

        edt_add_game_event_handler(EVENT_GAME_OVER, [this](edt_job_t event)
                                   {
                                       update_score(&event);

                                       commit();

                                       _audio->play(game_over_wav, game_over_wav_len); //
                                   });

        edt_add_game_event_handler(EVENT_SCORE_UPDATE, [this](edt_job_t event)
                                   {
                                       update_score(&event);

                                       commit(); //
                                   });

        edt_add_game_event_handler(EVENT_ROW_CLEARED, [this](edt_job_t event)
                                   {
                                       update_score(&event);

                                       commit();

                                       if (event.cleared_rows == 4)
                                       {
                                           _audio->play(line_clear_wav, line_clear_wav_len); //
                                       }
                                       else
                                       {
                                           _audio->play(tetris_4_lines_wav, tetris_4_lines_wav_len); //
                                       } //
                                   });

        edt_add_game_event_handler(EVENT_NEXT_SHAPE, [this](edt_job_t event)
                                   {
                                       update_score(&event);

                                       printf("Next shape: %d\n", event.shape_type);
                                       _tft->fillRect(0, 80, 240, 160, 0x0000);

                                       if (event.shape_type < N)
                                       {
                                           shape_t *s = ctetris_shape(event.shape_type);

                                           for (uint8_t i = 0; i < OFFSETS_COUNT; i++)
                                           {
                                               uint16_t gc = 120 + 40 * s->offsets[i].x;
                                               uint16_t gr = 120 + 40 * s->offsets[i].y;

                                               printf("Shape element: %d,%d\n", gc, gr);

                                               _tft->fillRect(
                                                   gc,
                                                   gr,
                                                   40,
                                                   40,
                                                   0xF800);
                                           }
                                       }

                                       commit(); //
                                   });
    }

private:
    TFT *_tft;
    Audio *_audio;
    int _last_level = 0;

    void commit()
    {
        _tft->commit(0, 0, 240, 240);
    }

    void update_score(edt_job_t *job)
    {
        _tft->fillRect(0, 0, 240, 40, 0x000);
        _tft->setTextSize(3);
        _tft->setCursor(4, 4);

        _tft->setTextColor(0xFFFF);

        _tft->printf("SCORE: %d", job->score);

        if (_last_level != job->level)
        {
            if (_last_level > 0)
            {
                _audio->play(level_up_jingle_wav, level_up_jingle_wav_len);
            }

            _last_level = job->level;
        }
    }
};

#endif