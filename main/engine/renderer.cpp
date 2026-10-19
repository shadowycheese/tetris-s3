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
        printf("Anim complete %d progress => %0.3f\n", anim->type, anim->progress);
        anim->progress = 1.0f;
        anim->type = ANIM_NONE;
    }
    else
    {
        printf("Anim %d progress => %0.3f\n", anim->type, anim->progress);
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
            printf("Color animation: %02X", col);
            col = color_lerp(col, COL_WHITE, 1.0f - anim->progress);
            printf("==>%02X==>%02X\n", col, COL_WHITE);
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

void Renderer::ui_shadow_shape_draw(ui_shape_t *ui_shape, int col_ind, float dt)
{
    if (!ui_shape || ui_shape->shape.type == N)
    {
        return;
    }

    col_ind = color_lerp(0, col_ind, 0.3);

    for (uint8_t i = 0; i < OFFSETS_COUNT; i++)
    {
        uint8_t gc = ui_shape->shape.pos.x + ui_shape->shape.offsets[i].x;
        uint8_t gr = ui_shape->shape.pos.y + ui_shape->shape.offsets[i].y;

        for (int x = 0; x < BLOCK_SIZE; x++)
        {
            for (int y = 0; y < BLOCK_SIZE; y++)
            {
                if (((x) % 2) == ((y) % 2))
                {
                    _hub75->draw_pixel(X_OFFSET + (gc * BLOCK_SIZE) + x,
                                       Y_OFFSET + (gr * BLOCK_SIZE) + y,
                                       col_ind);
                }
            }
        }
    }
}

void Renderer::soft_drop_trail_set(shape_t shape, coord_t from)
{
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

    for (int j = 0; j < 5; j++)
    {
        int x = min_x + (random() % (max_x - min_x));
        int y = Y_OFFSET + (from.y * BLOCK_SIZE) + (random() % 3);
        int y2 = y - (random() % 4);

        if (y2 < 0)
        {
            y2 = 0;
        }

        double start = y * 0.001;
        double fade_adj = ((double)(random() % 256)) / 1000.0;

        particle_t p = {
            .direction = Lerp,
            .fade_start = 0.1 + start,
            .fade_end = 0.150 + start + start + fade_adj,
            .move_start = 0.0 + start,
            .move_end = 0.150 + start,
            .move_from = {x, y},
            .move_to = {x, y2},
            .col_start = piece_color(shape.type),
            .col_end = 0,
        };

        _particles.start_effect(&p);
    }
}

void Renderer::shift_trail_set(shape_t shape, bool left, coord_t from)
{
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

    for (int j = 0; j < 5; j++)
    {
        int x = (left ? max_x : (min_x - 3)) + (random() % 3);
        int y = Y_OFFSET + (from.y * BLOCK_SIZE) + (random() % 3);

        int r = random() % 4;

        int x2 = x + (left ? r : -r);

        double start = y * 0.001;
        double fade_adj = ((double)(random() % 256)) / 1000.0;

        particle_t p = {
            .direction = Lerp,
            .fade_start = 0.1 + start,
            .fade_end = 0.150 + start + start + fade_adj,
            .move_start = 0.0 + start,
            .move_end = 0.150 + start,
            .move_from = {x, y},
            .move_to = {x2, y},
            .col_start = piece_color(shape.type),
            .col_end = 0,
        };

        _particles.start_effect(&p);
    }
}

void Renderer::hard_drop_trail_set(shape_t shape, coord_t from, coord_t to)
{
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
            double fade_adj = ((double)(random() % 256)) / 1000.0;

            particle_t p = {
                .direction = Lerp,
                .fade_start = 0.1 + start,
                .fade_end = 0.250 + start + fade_adj,
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

        edt_job_t job = {
            .event_type = EVENT_GAME_STATE,
            .shape_type = _player_next_shape.shape.type,
            .game_state = GAME_STATE_GAME_OVER,
            .cleared_rows = 0,
            .total_rows = _lines,
            .level = _level,
            .score = _score,
        };
        edt_post(job);
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

bool Renderer::event_soft_drop_handle(tetris_event_t ev)
{
    _player_active_shape.shape = ev.shape;
    _score = ev.score;

    soft_drop_trail_set(_player_active_shape.shape,
                        _player_active_shape.shape.pos);

    return true;
}

bool Renderer::event_shift_left_handle(tetris_event_t ev)
{
    _player_active_shape.shape = ev.shape;
    _player_shadow_shape.shape = ctetris_shape_proj_get();

    shift_trail_set(_player_active_shape.shape,
                    true,
                    _player_active_shape.shape.pos);

    return true;
}

bool Renderer::event_shift_right_handle(tetris_event_t ev)
{
    _player_active_shape.shape = ev.shape;
    _player_shadow_shape.shape = ctetris_shape_proj_get();

    shift_trail_set(_player_active_shape.shape,
                    false,
                    _player_active_shape.shape.pos);

    return true;
}

bool Renderer::event_rotate_handle(tetris_event_t ev)
{
    _player_active_shape.shape = ev.shape;
    _player_shadow_shape.shape = ctetris_shape_proj_get();

    edt_job_t job = {
        .event_type = EVENT_ROTATE,
        .shape_type = N,
        .game_state = _paused | _block_engine ? GAME_STATE_PAUSED : GAME_STATE_ACTIVE,
        .cleared_rows = (uint8_t)ev.lines_cleared_count,
        .total_rows = _lines,
        .level = _level,
        .score = ev.score,
    };
    edt_post(job);
    return true;
}

bool Renderer::event_hard_drop_handle(tetris_event_t ev)
{
    _score = ev.score;

    hard_drop_trail_set(_player_active_shape.shape,
                        _player_active_shape.shape.pos,
                        _player_shadow_shape.shape.pos);

    edt_job_t job = {
        .event_type = EVENT_HARD_DROP,
        .shape_type = N,
        .game_state = _paused | _block_engine ? GAME_STATE_PAUSED : GAME_STATE_ACTIVE,
        .cleared_rows = (uint8_t)ev.lines_cleared_count,
        .total_rows = _lines,
        .level = _level,
        .score = ev.score,
    };
    edt_post(job);

    _player_active_shape.shape = ev.shape;

    return false;
}

bool Renderer::event_lock_handle(tetris_event_t ev)
{
    edt_job_t job = {
        .event_type = EVENT_LOCK,
        .shape_type = N,
        .game_state = _paused | _block_engine ? GAME_STATE_PAUSED : GAME_STATE_ACTIVE,
        .cleared_rows = (uint8_t)ev.lines_cleared_count,
        .total_rows = _lines,
        .level = _level,
        .score = _score,
    };
    edt_post(job);

    anim_set_flash(&_player_active_shape.anim, SHAPE_FLASH_SPEED);
    _ui_main_grid_write_pending = true;

    return false;
}

bool Renderer::event_line_clear_handle(tetris_event_t ev)
{
    _score = ev.score;
    _level = ev.level;
    _lines = ev.lines;

    edt_job_t job = {
        .event_type = EVENT_ROW_CLEARED,
        .shape_type = N,
        .game_state = _paused | _block_engine ? GAME_STATE_PAUSED : GAME_STATE_ACTIVE,
        .cleared_rows = (uint8_t)ev.lines_cleared_count,
        .total_rows = _lines,
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
        return event_soft_drop_handle(ev);

    case CTETRIS_EVENT_SHIFT_LEFT:
        return event_shift_left_handle(ev);

    case CTETRIS_EVENT_SHIFT_RIGHT:
        return event_shift_right_handle(ev);

    case CTETRIS_EVENT_ROTATE:
        return event_rotate_handle(ev);

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
        return event_lock_handle(ev);

    case CTETRIS_EVENT_LINE_CLEAR:
        return event_line_clear_handle(ev);

    default:
        break;
    }

    return true;
}

void Renderer::reset()
{
    ui_main_grid_init();

    ctetris_init();

    _player_active_shape.shape.type = N;
    _player_shadow_shape.shape.type = N;
    _player_next_shape.shape.type = N;

    _paused = false;
    _ui_main_grid_write_pending = false;
    _block_engine = false;
    _line_move_pending = false;
    _game_over = false;
}

void Renderer::init()
{
    ctetris_init();
    ui_main_grid_init();

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

bool Renderer::input(input_state_t *input_state)
{
    if (_paused || _block_engine)
    {
        return true;
    }

    ctetris_input_push(input_state);

    return true;
}

// Update the game, step the engine forward and deal with events.
// Returns false when game over.
bool Renderer::update(double now)
{
    if (_last_time == -1)
    {
        _last_time = now;
    }

    double delta = now - _last_time;

    _last_time = now;
    _last_delta = delta;

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
            (_player_active_shape.anim.type != ANIM_NONE))
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

    int i = 0;

    tetris_event_t ev;
    while ((ev = ctetris_event_pop()).type != CTETRIS_EVENT_NONE)
    {
        if ((i++ % 100) == 0)
        {
            printf("...");
        }
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

    ui_main_grid_draw(_last_delta);

    _particles.update(time);

    int col = piece_color(_player_active_shape.shape.type);
    ui_shape_draw(&_player_active_shape,
                  col,
                  time);
    ui_shadow_shape_draw(&_player_shadow_shape,
                         col,
                         time);

    _hub75->commit_frame();
}
