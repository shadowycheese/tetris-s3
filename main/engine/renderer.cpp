#include <inttypes.h>
#include <stdio.h>

#include "renderer.h"

int Renderer::piece_color(shape_type_t t)
{
    switch (t)
    {
    case O:
        return COL_YELLOW;
    case L:
        return COL_BLUE;
    case I:
        return COL_CYAN;
    case T:
        return COL_RED;
    case S:
        return COL_GREEN;
    case J:
        return COL_BLUE;
    case Z:
        return COL_RED;
    default:
        return COL_WHITE;
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

/* Main Grid utilities */

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
        // Here the assumption is that the shape is valid and entirely within
        // the main grid.
        uint8_t gc = shape.pos.x + shape.offsets[i].x;
        uint8_t gr = shape.pos.y + shape.offsets[i].y;
        _ui_main_grid[gr][gc].type = shape.type;
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
            _hub75->fill_rect(
                X_OFFSET + c * BLOCK_SIZE,
                Y_OFFSET + r * BLOCK_SIZE,
                BLOCK_SIZE,
                BLOCK_SIZE,
                COL_BLACK);

            // Draw the cell if it isnt empty or has an active animation.
            if (_ui_main_grid[r][c].type != N || _ui_main_grid[r][c].anim.type != ANIM_NONE)
            {
                _hub75->fill_rect(
                    X_OFFSET + c * BLOCK_SIZE,
                    Y_OFFSET + r * BLOCK_SIZE,
                    BLOCK_SIZE,
                    BLOCK_SIZE,
                    piece_color(_ui_main_grid[r][c].type));
                anim_update(&_ui_main_grid[r][c].anim, dt);
            }

            still_animating = still_animating || (_ui_main_grid[r][c].anim.type != ANIM_NONE);
        }
    }
    _ui_main_grid_animating = still_animating;
}

void Renderer::ui_shape_draw(ui_shape_t *ui_shape, int col_ind,
                             uint64_t origin_x, uint64_t origin_y,
                             uint64_t cell_size, float dt)
{
    if (!ui_shape || ui_shape->shape.type == N)
    {
        return;
    }
    for (uint8_t i = 0; i < OFFSETS_COUNT; i++)
    {
        uint8_t gc = ui_shape->shape.pos.x + ui_shape->shape.offsets[i].x;
        uint8_t gr = ui_shape->shape.pos.y + ui_shape->shape.offsets[i].y;
        _hub75->fill_rect(
            origin_x + gc * cell_size,
            origin_y + gr * cell_size,
            cell_size,
            cell_size,
            col_ind);
    }
    anim_update(&ui_shape->anim, dt);
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

    // Center next shape inside next grid.
    shape_t *s = &_player_next_shape.shape;
    int8_t min_x = s->offsets[0].x, max_x = s->offsets[0].x;
    int8_t min_y = s->offsets[0].y, max_y = s->offsets[0].y;
    for (uint8_t i = 1; i < OFFSETS_COUNT; i++)
    {
        if (s->offsets[i].x < min_x)
        {
            min_x = s->offsets[i].x;
        }
        if (s->offsets[i].x > max_x)
        {
            max_x = s->offsets[i].x;
        }
        if (s->offsets[i].y < min_y)
        {
            min_y = s->offsets[i].y;
        }
        if (s->offsets[i].y > max_y)
        {
            max_y = s->offsets[i].y;
        }
    }
    s->pos.x = (NEXT_GRID_SIZE - (max_x - min_x + 1)) / 2 - min_x;
    s->pos.y = (NEXT_GRID_SIZE - (max_y - min_y + 1)) / 2 - min_y;

    // Set shape fade in animations.
    anim_set_lerp(&_player_active_shape.anim, SHAPE_FADE_IN_SPEED, COL_BLACK,
                  piece_color(_player_active_shape.shape.type));
    anim_set_lerp(&_player_shadow_shape.anim, SHAPE_FADE_IN_SPEED, COL_BLACK,
                  COL_WHITE);
    anim_set_lerp(&_player_next_shape.anim, SHAPE_FADE_IN_SPEED, COL_BLACK,
                  piece_color(_player_next_shape.shape.type));

    return true;
}

void Renderer::event_soft_drop_handle(tetris_event_t ev)
{
    _player_active_shape.shape = ev.shape;
    uint32_t delta = ev.score - _score;
    _score = ev.score;
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

    _player_active_shape.shape = ev.shape;

    return false;
}

bool Renderer::event_line_clear_handle(tetris_event_t ev)
{
    uint32_t score_delta = ev.score - _score;
    _score = ev.score;
    _level = ev.level;
    _lines = ev.lines;

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

    // Store indices of rows that has to move after line clear and to where.
    // How many cleared lines a row has below it is exactly the number of rows
    // it should move down.
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

// Handle the given engine event. returns false if the engine should be
// blocked.
// Main event handler.
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

// Initialize state variables, used on when a new game starts.
void Renderer::state_init(void)
{
    ctetris_init();
    ui_main_grid_init();

    anim_set_none(&_trail_anim);

    _player_active_shape.shape.type = N;
    _player_shadow_shape.shape.type = N;
    _player_next_shape.shape.type = N;

    _paused = false;
    _ui_main_grid_write_pending = false;
    _block_engine = false;
    _line_move_pending = false;
    _game_over = false;
}

// Initialize the renderer.
void Renderer::init()
{
    state_init(); // Initialize the engine and game state variables.
}

bool Renderer::input()
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

    if (_paused || _block_engine)
    {
        return true;
    }

    // Push input to engine.
    input_state_t input_state = {};
    /*
        .shift_left_pressed = IsKeyPressed(KEY_LEFT),
        .shift_right_pressed = IsKeyPressed(KEY_RIGHT),
        .shift_left_held = IsKeyDown(KEY_LEFT),
        .shift_right_held = IsKeyDown(KEY_RIGHT),
        .soft_drop_held = IsKeyDown(KEY_DOWN),
        .rotate_right_pressed = IsKeyPressed(KEY_UP),
        .rotate_left_pressed = IsKeyPressed(KEY_Z),
        .hard_drop_pressed = IsKeyPressed(KEY_SPACE),
    };*/
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

    // The engine is only stepped forward when it isnt blocked and
    // all buffered events are processed.
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
                  X_OFFSET,
                  Y_OFFSET,
                  BLOCK_SIZE,
                  time);

    _hub75->commit_frame();

    // hard_drop_trail_draw(0);

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
