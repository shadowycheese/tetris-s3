#ifndef RENDERER_H
#define RENDERER_H

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include "ctetris.h"
#include "lerp.h"
#include "matrix/hub75.h"
#include "engine/particles.h"

// Speed for animations.
#define SHAPE_FADE_IN_SPEED 5.0f
#define SHAPE_LOCK_FADE_OUT_SPEED (1.0f / SHAPE_LOCK_DELAY)
#define SHAPE_FLASH_SPEED 10.0f

#define LINE_FADE_OUT_SPEED 4.0f
#define LINE_MOVE_SPEED 10.0f

#define TRAIL_FADE_OUT_SPEED 10.0f

#define POPUP_FADE_IN_SPEED 15.0f
#define POPUP_FADE_OUT_SPEED 1.0f

#define BADGE_STATE_LERP_SPEED 10.0f
#define THEME_LERP_SPEED 5.0f
#define GAMEOVER_LERP_SPEED 5.0f

/* [ ENUMS AND STRUCTS ] */
typedef enum
{
    ANIM_NONE,      // No animations.
    ANIM_LERP,      // Linear interpolation bw colors.
    ANIM_FLASH,     // Pretty much "lerp to WHITE".
    ANIM_TRANSLATE, // Animate translation (movement).
} animation_type_t;

// Represents a animation.
typedef struct
{
    animation_type_t type;
    float progress; // (0.0f - 1.0f)
    float speed;    // duration = 1/speed
    point_t move_from;
    point_t move_to;
    int lerp_from;
    int lerp_to;
} animation_t;

// All keybindings.
typedef enum
{
    KB_MOVE_LEFT,
    KB_MOVE_RIGHT,
    KB_ROTATE_RIGHT,
    KB_ROTATE_LEFT,
    KB_SOFT_DROP,
    KB_HARD_DROP,
    KB_PAUSE,
    KB_RESTART,
    KB_MUTE,
    KB_THEME,
    KB_COUNT,
} key_bind_t;

typedef struct
{
    animation_t anim;
    shape_type_t type;
} ui_grid_cell_t;

typedef struct
{
    shape_t shape;
    animation_t anim;
} ui_shape_t;

class Renderer
{
public:
    Renderer(Hub75 *hub75) : _particles(hub75)
    {
        _hub75 = hub75;
    }

    void init();
    void reset();
    bool input(input_state_t *input_state); // Returns false signaling game exit.
    bool update(double now);                // Returns false signaling game over.
    void render(double now);

private:
    Hub75 *_hub75;
    Particles _particles;

    float _last_time = -1.0;
    float _last_delta = -1.0;

    const int X_OFFSET = 1;
    const int Y_OFFSET = 0;
    const int BLOCK_SIZE = 3;
    const int NEXT_GRID_SIZE = 3;
    const int W = 32;
    const int H = 64;

    ui_grid_cell_t _ui_main_grid[ROWS][COLS];
    bool _ui_main_grid_animating; // Is the main grid being animated?
    bool _ui_main_grid_write_pending;

    ui_shape_t _player_active_shape;
    ui_shape_t _player_shadow_shape;
    ui_shape_t _player_next_shape;

    coord_t _line_moves[ROWS];
    uint8_t _line_move_count;
    bool _line_move_pending;

    // Game stats.
    uint32_t _score, _high_score;
    uint8_t _level;
    uint16_t _lines;

    bool _paused = false;
    bool _audio_muted = false;
    bool _block_engine = false;
    bool _game_over = false;

    void anim_set_none(animation_t *anim);

    void anim_set_lerp(animation_t *anim, float speed, int from, int to);
    void anim_set_translate(animation_t *anim, float speed, point_t from, point_t to);
    void anim_set_flash(animation_t *anim, float speed);
    void anim_update(animation_t *anim, float dt);

    void ui_main_grid_init(void);

    void ui_main_grid_shape_write(shape_t shape);
    void ui_main_grid_draw(float dt);

    void ui_shape_draw(ui_shape_t *ui_shape, int col_ind, float dt);
    void ui_shadow_shape_draw(ui_shape_t *ui_shape, int col_ind, float dt);
    void ui_rect_draw(rectangle_t rect, uint8_t base_color, bool outline, const animation_t *anim);

    void shift_trail_set(shape_t shape, bool left, coord_t from);
    void soft_drop_trail_set(shape_t shape, coord_t from);
    void hard_drop_trail_set(shape_t shape, coord_t from, coord_t to);

    bool event_new_shape_handle(tetris_event_t ev);
    bool event_soft_drop_handle(tetris_event_t ev);
    bool event_rotate_handle(tetris_event_t ev);
    bool event_shift_left_handle(tetris_event_t ev);
    bool event_shift_right_handle(tetris_event_t ev);
    bool event_hard_drop_handle(tetris_event_t ev);
    bool event_line_clear_handle(tetris_event_t ev);
    bool event_lock_handle(tetris_event_t ev);

    bool event_handle(tetris_event_t ev);

    static uint8_t piece_color(shape_type_t t);
};

#endif
