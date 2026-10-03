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
                                       //_audio->play(SFX_THUD_WAV, SFX_THUD_WAV_LEN); //

                                       _audio->play(SFX_CLACK_WAV, SFX_CLACK_WAV_LEN); //
                                   });

        edt_add_game_event_handler(EVENT_SCORE_UPDATE, [this](edt_job_t event)
                                   {
                                       update_score(&event);

                                       commit(); //
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
    }
};

#endif