#include <inttypes.h>
#include <stdio.h>

#include "events/edt.h"
#include "engine/renderer.h"
#include "engine/particles.h"

uint8_t Renderer::piece_color(shape_type_t t)
{
    switch (t)
    {
    case O:
        return COL_YELLOW;
    case L:
        return COL_ORANGE;
    case I:
        return COL_CYAN;
    case T:
        return COL_MAGENTA;
    case S:
        return COL_GREEN;
    case J:
        return COL_BLUE;
    case Z:
        return COL_RED;
    default:
        return 0x15;
    }
}

void Renderer::anim_set_none(animation_t *anim)
{
    anim->type = ANIM_NONE;
    anim->progress = 0.0f;
    anim->speed = 0.0f;
}

void Renderer::anim_set_lerp(animation_t *anim, float speed, int from, int to)
{
    anim->type = ANIM_LERP;
    anim->progress = 0.0f;
    anim->speed = speed;
    anim->lerp_from = from;
    anim->lerp_to = to;
}

void Renderer::anim_set_translate(animation_t *anim, float speed, point_t from, point_t to)
{
    anim->type = ANIM_TRANSLATE;
    anim->progress = 0.0f;
    anim->speed = speed;
    anim->move_from = from;
    anim->move_to = to;
}

void Renderer::anim_set_flash(animation_t *anim, float speed)
{
    anim->type = ANIM_FLASH;
    anim->progress = 0.0f;
    anim->speed = speed;
}

void Renderer::anim_update(animation_t *anim, float dt)
{
    if (anim->type == ANIM_NONE)
    {
        return;
    }

    anim->progress += dt * anim->speed;
    if (anim->progress >= 1.0f)
    {
        anim->progress = 1.0f;
        anim->type = ANIM_NONE;
    }
}

void Renderer::ui_main_grid_init(void)
{
    for (uint8_t r = 0; r < ROWS; r++)
    {
        for (uint8_t c = 0; c < COLS; c++)
        {
            _ui_main_grid[r][c].type = N;

            anim_set_none(&_ui_main_grid[r][c].anim);
        }
    }
    _ui_main_grid_animating = false;
}

void Renderer::ui_main_grid_shape_write(shape_t shape)
{
    for (uint8_t i = 0; i < OFFSETS_COUNT; i++)
    {
        uint8_t gc = shape.pos.x + shape.offsets[i].x;
        uint8_t gr = shape.pos.y + shape.offsets[i].y;

        _ui_main_grid[gr][gc].type = shape.type;
    }
}

void Renderer::ui_rect_draw(rectangle_t rect, uint8_t col, bool outline, const animation_t *anim)
{
    rectangle_t drawn_rect = rect;

    if (anim && anim->type != ANIM_NONE)
    {
        if (anim->type == ANIM_TRANSLATE)
        {
            point_t anim_pos = point_lerp(anim->move_from, anim->move_to, anim->progress);
            drawn_rect.x = anim_pos.x;
            drawn_rect.y = anim_pos.y;
        }
        else if (anim->type == ANIM_LERP)
        {
            col = color_lerp(anim->lerp_from, anim->lerp_to, anim->progress);
        }
        else if (anim->type == ANIM_FLASH)
        {
            col = color_lerp(col, COL_WHITE, 1.0f - anim->progress);
        }
    }

    if (outline)
    {
        _hub75->draw_rect(drawn_rect.x, drawn_rect.y, drawn_rect.w, drawn_rect.h, col);
    }
    else
    {
        _hub75->fill_rect(drawn_rect.x, drawn_rect.y, drawn_rect.w, drawn_rect.h, col);
    }
}

void Renderer::ui_main_grid_draw(float dt)
{
    _hub75->draw_rect(0, 0, W, H, COL_BLUE);

    bool still_animating = false;

    for (uint8_t r = 0; r < ROWS; r++)
    {
        for (uint8_t c = 0; c < COLS; c++)
        {
            rectangle_t cell_rect = {
                X_OFFSET + c * BLOCK_SIZE,
                Y_OFFSET + r * BLOCK_SIZE,
                BLOCK_SIZE,
                BLOCK_SIZE,
            };

            ui_rect_draw(cell_rect, COL_BLACK, false, NULL);

            // Draw the cell if it isnt empty or has an active animation.
            if (_ui_main_grid[r][c].type != N || _ui_main_grid[r][c].anim.type != ANIM_NONE)
            {
                ui_rect_draw(cell_rect, piece_color(_ui_main_grid[r][c].type), false, &_ui_main_grid[r][c].anim);

                anim_update(&_ui_main_grid[r][c].anim, dt);
            }

            still_animating = still_animating || (_ui_main_grid[r][c].anim.type != ANIM_NONE);
        }
    }

    _ui_main_grid_animating = still_animating;
}

void Renderer::ui_shape_draw(ui_shape_t *ui_shape, int col_ind, float dt)
{
    if (!ui_shape || ui_shape->shape.type == N)
    {
        return;
    }

    for (uint8_t i = 0; i < OFFSETS_COUNT; i++)
    {
        uint8_t gc = ui_shape->shape.pos.x + ui_shape->shape.offsets[i].x;
        uint8_t gr = ui_shape->shape.pos.y + ui_shape->shape.offsets[i].y;

        rectangle_t r = {
            X_OFFSET + gc * BLOCK_SIZE,
            Y_OFFSET + gr * BLOCK_SIZE,
            BLOCK_SIZE,
            BLOCK_SIZE,
        };

        ui_rect_draw(r, col_ind, false, &ui_shape->anim);
    }

    anim_update(&ui_shape->anim, dt);
}

// Sets up hard drop trail rectangles for the given shape and path.
void Renderer::hard_drop_trail_set(shape_t shape, coord_t from, coord_t to)
{
    _trail_rect_count = 0;

    uint8_t min_x = COLS * BLOCK_SIZE;
    uint8_t max_x = 0;

    for (uint8_t i = 0; i < OFFSETS_COUNT; i++)
    {
        uint8_t new_x = X_OFFSET + (shape.offsets[i].x + from.x) * BLOCK_SIZE;

        if (new_x < min_x)
        {
            min_x = new_x;
        }
        if ((new_x + BLOCK_SIZE) > max_x)
        {
            max_x = new_x + BLOCK_SIZE;
        }
    }

    for (int8_t i = from.y; i < to.y; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            int x = min_x + (random() % (max_x - min_x));
            int y = Y_OFFSET + (i * BLOCK_SIZE) + (random() % 3);
            int y2 = y - (random() % 10);

            if (y2 < 0)
            {
                y2 = 0;
            }

            double start = y * 0.001;

            particle_t p = {
                .direction = Lerp,
                .fade_start = 0.1 + start,
                .fade_end = 0.250 + start,
                .move_start = 0.0 + start,
                .move_end = 0.250 + start,
                .move_from = {x, y},
                .move_to = {x, y2},
                .col_start = piece_color(shape.type),
                .col_end = 0,
            };

            _particles.start_effect(&p);
        }
    }
}

bool Renderer::event_new_shape_handle(tetris_event_t ev)
{
    _player_active_shape.shape = ev.shape;
    _player_shadow_shape.shape = ctetris_shape_proj_get();
    _player_next_shape.shape = ctetris_shape_next_get();

    // If the engine went inactive with the new shape, meaning game ended:
    if (ev.engine_inactive)
    {
        _game_over = true;
        // game_over_t = 0.0f;
        return false;
    }

    edt_job_t job = {
        .event_type = EVENT_NEXT_SHAPE,
        .shape_type = _player_next_shape.shape.type,
        .game_state = _paused | _block_engine ? GAME_STATE_PAUSED : GAME_STATE_ACTIVE,
        .cleared_rows = 0,
        .total_rows = _lines,
        .level = _level,
        .score = _score,
    };
    edt_post(job);

    anim_set_lerp(&_player_active_shape.anim, SHAPE_FADE_IN_SPEED, COL_BLACK, piece_color(_player_active_shape.shape.type));
    anim_set_lerp(&_player_shadow_shape.anim, SHAPE_FADE_IN_SPEED, COL_BLACK, COL_WHITE);

    return true;
}

void Renderer::event_soft_drop_handle(tetris_event_t ev)
{
    _player_active_shape.shape = ev.shape;
    uint32_t delta = ev.score - _score;
    _score = ev.score;

    edt_job_t job = {
        .event_type = EVENT_SCORE_UPDATE,
        .game_state = _paused | _block_engine ? GAME_STATE_PAUSED : GAME_STATE_ACTIVE,
        .cleared_rows = (uint8_t)ev.lines_cleared_count,
        .total_rows = _level,
        .level = _level,
        .score = _score,
    };
    edt_post(job);
}

void Renderer::event_shift_rotate_handle(tetris_event_t ev)
{
    _player_active_shape.shape = ev.shape;
    _player_shadow_shape.shape = ctetris_shape_proj_get();
}

bool Renderer::event_hard_drop_handle(tetris_event_t ev)
{
    uint32_t delta = ev.score - _score;
    _score = ev.score;

    hard_drop_trail_set(_player_active_shape.shape,
                        _player_active_shape.shape.pos,
                        _player_shadow_shape.shape.pos);

    edt_job_t job = {
        .event_type = EVENT_HARD_DROP,
        .game_state = _paused | _block_engine ? GAME_STATE_PAUSED : GAME_STATE_ACTIVE,
        .cleared_rows = (uint8_t)ev.lines_cleared_count,
        .total_rows = _level,
        .level = _level,
        .score = ev.score,
    };
    edt_post(job);

    _player_active_shape.shape = ev.shape;

    return false;
}

bool Renderer::event_line_clear_handle(tetris_event_t ev)
{
    uint32_t score_delta = ev.score - _score;
    _score = ev.score;
    _level = ev.level;
    _lines = ev.lines;

    edt_job_t job = {
        .event_type = EVENT_SCORE_UPDATE,
        .game_state = _paused | _block_engine ? GAME_STATE_PAUSED : GAME_STATE_ACTIVE,
        .cleared_rows = (uint8_t)ev.lines_cleared_count,
        .total_rows = _level,
        .level = _level,
        .score = _score,
    };
    edt_post(job);

    // Fade out cleared lines
    for (uint8_t i = 0; i < ev.lines_cleared_count; i++)
    {
        uint8_t row = ev.lines_cleared_indices[i];

        for (uint8_t c = 0; c < COLS; c++)
        {
            anim_set_lerp(&_ui_main_grid[row][c].anim, LINE_FADE_OUT_SPEED,
                          piece_color(_ui_main_grid[row][c].type), COL_BLACK);
            _ui_main_grid[row][c].type = N;
        }
    }

    _line_move_count = 0;

    for (int8_t src = ROWS - 1; src >= 0; src--)
    {
        uint8_t drop_distance = 0;
        for (uint8_t i = 0; i < ev.lines_cleared_count; i++)
        {
            if (ev.lines_cleared_indices[i] > src)
            {
                drop_distance++;
            }
        }

        if (drop_distance > 0)
        {
            bool empty = true;
            for (uint8_t c = 0; c < COLS; c++)
            {
                if (_ui_main_grid[src][c].type != N)
                {
                    empty = false;
                    break;
                }
            }
            if (!empty)
            {
                int8_t dst = src + drop_distance;

                _line_moves[_line_move_count++] = (coord_t){src, dst};
            }
        }
    }

    _line_move_pending = true;

    return false;
}

bool Renderer::event_handle(tetris_event_t ev)
{
    switch (ev.type)
    {
    case CTETRIS_EVENT_NEW_SHAPE:
        return event_new_shape_handle(ev);

    case CTETRIS_EVENT_DROP:
        _player_active_shape.shape = ev.shape;
        break;

    case CTETRIS_EVENT_SOFT_DROP:
        event_soft_drop_handle(ev);
        break;

    case CTETRIS_EVENT_SHIFT:
    case CTETRIS_EVENT_ROTATE:
        event_shift_rotate_handle(ev);
        break;

    case CTETRIS_EVENT_HARD_DROP:
        return event_hard_drop_handle(ev);

    case CTETRIS_EVENT_LOCK_START:
    case CTETRIS_EVENT_LOCK_RESET:
        anim_set_lerp(&_player_active_shape.anim, SHAPE_LOCK_FADE_OUT_SPEED,
                      piece_color(_player_active_shape.shape.type), COL_BLACK);
        break;

    case CTETRIS_EVENT_LOCK_CANCEL:
        anim_set_none(&_player_active_shape.anim);
        break;

    case CTETRIS_EVENT_LOCK_DONE:
        //        if (!audio_muted)
        //          PlaySound(sfx_click);
        anim_set_flash(&_player_active_shape.anim, SHAPE_FLASH_SPEED);
        _ui_main_grid_write_pending = true;
        return false;

    case CTETRIS_EVENT_LINE_CLEAR:
        return event_line_clear_handle(ev);

    default:
        break;
    }

    return true;
}

void Renderer::init()
{
    ctetris_init();
    ui_main_grid_init();

    anim_set_none(&_trail_anim);

    _particles.init();

    _player_active_shape.shape.type = N;
    _player_shadow_shape.shape.type = N;
    _player_next_shape.shape.type = N;

    _paused = false;
    _ui_main_grid_write_pending = false;
    _block_engine = false;
    _line_move_pending = false;
    _game_over = false;
}

bool Renderer::input(uint32_t button_mask)
{ /*
     if (IsKeyPressed(KEY_P))
     {
         paused = !paused;
         ui_badge_update(&ui_key_info_list[KB_PAUSE].ui_badge, NULL, NULL, NULL,
                         &paused);
     }

     if (IsKeyPressed(KEY_R))
     {
         ui_badge_update(&ui_key_info_list[KB_RESTART].ui_badge, NULL, NULL,
                         NULL, &(bool){true});
         state_init();

         ui_badge_update(&ui_key_info_list[KB_PAUSE].ui_badge, NULL, NULL, NULL,
                         &paused);
         return true;
     }
     else
     {
         ui_badge_update(&ui_key_info_list[KB_RESTART].ui_badge, NULL, NULL,
                         NULL, &(bool){false});
     }

     if (IsKeyPressed(KEY_T))
     {
         prev_scheme = current_scheme;
         current_scheme = (current_scheme + 1) % SCHEME_COUNT;
         theme_switch_t = 0.0f;
         ui_badge_update(&ui_key_info_list[KB_THEME].ui_badge, NULL, NULL, NULL,
                         &(bool){true});
     }
     else
     {
         ui_badge_update(&ui_key_info_list[KB_THEME].ui_badge, NULL, NULL, NULL,
                         &(bool){false});
     }

     if (IsKeyPressed(KEY_M))
     {
         audio_muted = !audio_muted;
         ui_badge_update(&ui_key_info_list[KB_MUTE].ui_badge, NULL, NULL, NULL,
                         &audio_muted);
     }*/

    if ((button_mask & 0xAA) == 0xAA)
    {
        init();

        return true;
    }

    if (_paused || _block_engine)
    {
        return true;
    }

    // Push input to engine.
    input_state_t input_state = {
        .shift_left_pressed = (button_mask & 0x04) > 0,
        .shift_left_held = (button_mask & 0x08) > 0,
        .shift_right_pressed = (button_mask & 0x01) > 0,
        .shift_right_held = (button_mask & 0x02) > 0,
        .rotate_right_pressed = (button_mask & 0x10) > 0,
        .rotate_left_pressed = false,
        .soft_drop_held = false,
        .hard_drop_pressed = (button_mask & 0x40) > 0,
    };

    ctetris_input_push(input_state);

    return true;
}

// Update the game, step the engine forward and deal with events.
// Returns false when game over.
bool Renderer::update(double now)
{
    if (last_t == -1)
    {
        last_t = now;
    }
    double delta = now - last_t;
    last_t = now;

    if (_paused)
    {
        return true;
    }

    if (_game_over)
    {
        return false;
    }

    // In case the engine is blocked for playing animations / mutating main ui
    // grid then unblock once they are finished.
    if (_block_engine)
    {
        if (_ui_main_grid_animating ||
            (_player_active_shape.anim.type != ANIM_NONE) ||
            (_trail_anim.type != ANIM_NONE))
        {
            return true;
        }

        if (_line_move_pending)
        {
            for (uint8_t i = 0; i < _line_move_count; i++)
            {
                uint8_t src = _line_moves[i].x;
                uint8_t dst = _line_moves[i].y;
                for (uint8_t c = 0; c < COLS; c++)
                {
                    if (_ui_main_grid[src][c].type == N)
                    {
                        continue;
                    }
                    _ui_main_grid[dst][c] = _ui_main_grid[src][c];
                    int x = X_OFFSET + BLOCK_SIZE * c;
                    int from_y = Y_OFFSET + src * BLOCK_SIZE;
                    int to_y = Y_OFFSET + dst * BLOCK_SIZE;
                    anim_set_translate(&_ui_main_grid[dst][c].anim,
                                       LINE_MOVE_SPEED,
                                       (point_t){x, from_y},
                                       (point_t){x, to_y});
                    _ui_main_grid[src][c].type = N;
                }
            }
            _line_move_pending = false;
            return true;
        }

        if (_ui_main_grid_write_pending)
        {
            ui_main_grid_shape_write(_player_active_shape.shape);
            _ui_main_grid_write_pending = false;
            // Once the shape is locked then dont draw it and its shadow until a
            // new shape is spawned.
            _player_active_shape.shape.type = N;
            _player_shadow_shape.shape.type = N;
        }

        _block_engine = false;
    }

    tetris_event_t ev;
    while ((ev = ctetris_event_pop()).type != CTETRIS_EVENT_NONE)
    {
        if (!event_handle(ev))
        {
            _block_engine = true;
            return _game_over != true;
        }
    }

    ctetris_update(delta);
    return true;
}

// Render everything.
void Renderer::render(double time)
{
    _hub75->clear_screen();

    ui_main_grid_draw(time);

    ui_shape_draw(&_player_active_shape,
                  piece_color(_player_active_shape.shape.type),
                  time);

    _particles.update(time);

    _hub75->commit_frame();

    // Theme switch and game over lerp animation update.
    /*    if (game_over && game_over_t < 1.0f)
        {
            game_over_t += dt * GAMEOVER_LERP_SPEED;
            if (game_over_t > 1.0f)
            {
                game_over_t = 1.0f;
            }
        }*/
}
