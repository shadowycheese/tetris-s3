#ifndef UI_H
#define UI_H

#include "tft/tft.h"
#include "events/edt.h"
#include "assets/assets.h"
#include "audio/audio.h"
#include "tft/adafruit/Adafruit_GFX.h"
#include "engine/renderer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"

#define HS_DEBOUNCE (60LL * 1000LL * 1000LL)

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
        nvs_handle_t nvs_handle;

        const char *TAG = "UI";

        ESP_LOGI(TAG, "Reading current high scores...");

        nvs_open(_nvs_namespace, NVS_READONLY, &nvs_handle);

        if (nvs_get_u32(nvs_handle, _nvs_high_score, &_high_score) != ESP_OK)
        {
            _high_score = 0;
        }

        nvs_close(nvs_handle);

        ESP_LOGI(TAG, "Configuring event listeners...");

        edt_add_game_event_handler(EVENT_HARD_DROP, [this](edt_job_t event)
                                   {
                                       update_score(&event);

                                       commit(); //
                                   });

        edt_add_game_event_handler(EVENT_LOCK, [this](edt_job_t event)
                                   {
                                       update_score(&event);

                                       commit();

                                       _audio->play(piece_landed_wav, piece_landed_wav_len); //
                                   });

        edt_add_game_event_handler(EVENT_GAME_STATE, [this](edt_job_t event)
                                   {
                                       update_score(&event);

                                       commit(); });

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
                                           _audio->play(tetris_4_lines_wav, tetris_4_lines_wav_len);
                                       }
                                       else
                                       {
                                           _audio->play(line_clear_wav, line_clear_wav_len);
                                       } //
                                   });

        edt_add_game_event_handler(EVENT_NEXT_SHAPE, [this](edt_job_t event)
                                   {
                                       update_score(&event);

                                       _tft->fillRect(0, 100, 240, 140, 0x0000);

                                       if (event.shape_type < N)
                                       {
                                           shape_t *s = ctetris_shape(event.shape_type);

                                           uint16_t col = piece_color_rgb565(event.shape_type);

                                           int min_x = 40 * s->offsets[0].x;
                                           int max_x = min_x + 40;
                                           int min_y = 40 * s->offsets[0].y;
                                           int max_y = min_y + 40;

                                           for (uint8_t i = 1; i < OFFSETS_COUNT; i++)
                                           {
                                               int x = 40 * s->offsets[i].x;
                                               int y = 40 * s->offsets[i].y;

                                               if ((x + 40) > max_x)
                                               {
                                                   max_x = x + 40;
                                               }
                                               else if (x < min_x)
                                               {
                                                   min_x = x;
                                               }

                                               if ((y + 40) > max_y)
                                               {
                                                   max_y = y + 40;
                                               }
                                               else if (y < min_y)
                                               {
                                                   min_y = y;
                                               }
                                           }

                                           int xo = (240 - (max_x - min_x)) / 2;
                                           int yo = 100 + (140 - (max_y - min_y)) / 2;

                                           if (min_x < 0)
                                           {
                                               xo += -min_x;
                                           }
                                           if (min_y < 0)
                                           {
                                               yo += -min_y;
                                           }

                                           for (uint8_t i = 0; i < OFFSETS_COUNT; i++)
                                           {
                                               uint16_t gc = xo + 40 * s->offsets[i].x;
                                               uint16_t gr = yo + 40 * s->offsets[i].y;

                                               _tft->fillRect(
                                                   gc,
                                                   gr,
                                                   40,
                                                   40,
                                                   col);
                                           }
                                       }

                                       commit(); //
                                   });

        ESP_LOGI(TAG, "UI init complete");
    }

private:
    TFT *_tft;
    Audio *_audio;
    int _last_level = 0;
    uint32_t _high_score;
    bool _high_score_dirty;
    int64_t _last_hs_write;

    void commit()
    {
        _tft->commit(0, 0, 240, 240);
    }

    void update_score(edt_job_t *ev)
    {
        _tft->fillRect(0, 0, 240, 80, 0x000);
        _tft->setTextSize(3);
        _tft->setCursor(4, 4);

        if (ev->score > _high_score)
        {
            _high_score = ev->score;
            _high_score_dirty = true;
        }

        _tft->setTextColor(0xFFFF);
        _tft->printf("HIGHS:%07d", _high_score);
        _tft->setCursor(4, 30);
        _tft->printf("SCORE:%07d", ev->score);
        _tft->setCursor(4, 56);
        _tft->printf("LEVEL:%d", ev->level);

        if (_last_level != ev->level)
        {
            if (_last_level > 0)
            {
                _audio->play(level_up_jingle_wav, level_up_jingle_wav_len);
            }

            _last_level = ev->level;
        }

        if (_high_score_dirty)
        {
            int64_t now = esp_timer_get_time();

            if (ev->event_type == EVENT_GAME_OVER || ((now - _last_hs_write) > HS_DEBOUNCE))
            {
                save_high_score();
            }
        }

        if (ev->game_state == GAME_STATE_PAUSED)
        {
            _tft->fillRect(80, 100, 24, 100, 0xEEEE);
            _tft->fillRect(136, 100, 24, 100, 0xEEEE);
        }
    }

    void save_high_score()
    {
        nvs_handle_t nvs_handle;

        nvs_open(_nvs_namespace, NVS_READWRITE, &nvs_handle);

        nvs_set_u32(nvs_handle, _nvs_high_score, _high_score);

        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);

        _high_score_dirty = false;
    }

    uint16_t piece_color_rgb565(shape_type_t t)
    {
        switch (t)
        {
        case O:
            return 0xF81F;
        case L:
            return 0xF814;
        case I:
            return 0x07FF;
        case T:
            return 0xFFE0;
        case S:
            return 0x001F;
        case J:
            return 0x07E0;
        case Z:
            return 0xF800;
        default:
            return 0x15;
        }
    }

    const char *_nvs_namespace = "TETRIS";
    const char *_nvs_high_score = "HIGHS";
};

#endif