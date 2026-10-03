/* [ CTETRIS_H ] */

#ifndef CTETRIS_H
#define CTETRIS_H

/**
 * CTETRIS ENGINE
 * --------------
 *
 * ABOUT:
 *
 * This is a simple and minimal Tetris engine implementing only the bare minimum
 * of what I think constitutes a clean, lightweight, and fun Tetris game.
 *
 * GAMEPLAY:
 *
 * - The grid is 20×10, shapes are spawned at the top of the grid and drops down
 *   at an interval given by the formula:
 *   pow(0.8 - ((level - 1) * 0.007), level - 1)
 * - The drop interval cannot become shorter than the soft-drop interval.
 *
 * Input mechanics:
 * - Shift (left/right): Individual moves or held input with delay.
 * - Rotate (left/right): Geometric rotation with wall/floor kick collision.
 * - Soft drop: Accelerated fall (sets the drop interval to `SOFT_DROP_DELAY`
 *   when active).
 * - Hard drop: Instant teleportation to where the shape is supposed to land,
 *   locks immediately.
 *
 * Landing and locking:
 * - After landing (when not hard dropped), a lock timer runs. When it
 *   expires, the shape auto-locks.
 * - The lock timer is reset by further shift or rotate moves.
 * - If a move after landing puts the shape airborne, lock timer will be
 *   cancelled and further moves will reset the drop timer, pausing the downward
 *   drop mechanism.
 * - Once landed, all moves, whether the shape is grounded or airborne, are
 *   counted against a move budget, which when exhausted, a forced hard
 *   drop occurs.
 * - The move count can be reset and the drop mechanism restored to its default
 *   behavior, by letting the shape fall to a row it has never reached before.
 *
 * Line clears:
 * - Completed rows are cleared; rows above compact downward.
 *
 * SCORING AND LEVELING:
 *
 * Score is earned on:
 * - Soft drop (1 point per row dropped)
 * - Hard drop (2 points per row dropped)
 * - Line clears (line clear bonus * level)
 *   The baseline line clear bonus is given by:
 *   - 1 line:  100
 *   - 2 lines: 300
 *   - 3 lines: 500
 *   - 4 lines: 800
 *   - Maintaining a combo / line-clear chain; Score gain for combo is given by
 *     the formula:
 *       level * COMBO_BONUS * combo.
 *   - Level is dependent on the lines cleared and is given by the formula:
 *     (lines / 10) + 1
 *
 * BASIC USAGE:
 *
 * An active engine session starts with a call to `ctetris_init` fn and ends
 * with a CTETRIS_EVENT_NEW_SHAPE event with `engine_inactive` set to true
 * (see `struct CTetrisEvent`).
 *
 * 1. Call ctetris_init() once to set up the game state.
 * 2. Each frame:
 *    - Call ctetris_input_push() to push input to the engine.
 *    - Call ctetris_update() to process the input and step the engine
 *      forward.
 *    - Poll ctetris_event_pop() in a loop until `CTETRIS_EVENT_NONE` to
 *      retrieve events happened in that update tick.
 *    - Query the engine using ctetris_shape_proj_get() and
 *      ctetris_shape_next_get().
 *    - Render based on events and queries.
 * 3. Once the engine session ends (game over), call ctetris_init() to start a
 *    new session (restart the game).
 *
 */

/* [ INCLUDES ] */

#include <stdbool.h>
#include <stdint.h>

/* [ DEFINES ] */

// Tetris grid dimensions.
#define ROWS 21
#define COLS 10

// Number of offsets used per shape to define them.
// All shapes are made of 4 blocks, meaning 4 offsets.
#define OFFSETS_COUNT 4

#define SOFT_DROP_DELAY 0.05f // Drop delay (seconds).

// Delay before shift auto-repeat kicks in (seconds).
#define SHIFT_DELAY_INITIAL 0.2f
// Delay for shift auto-repeat (seconds).
#define SHIFT_DELAY_REPEAT 0.1f

// Number of moves allowed after a shape has landed before it is automatically
// hard dropped and locked.
#define MOVES_BEFORE_LOCK 15

#define SHAPE_LOCK_DELAY 0.5f // Lock timer duration (seconds).

// Score bonus multiplier for maintaining a consecutive line clear chain.
#define COMBO_BONUS 50
// Combo reward formula macro.
#define CALC_COMBO_POINTS(level, combo) ((level) * COMBO_BONUS * (combo))

/* [ STRUCTS AND ENUMS ] */

// Coord struct is used to represent shape offsets and grid coordinates.
typedef struct
{
    int8_t x;
    int8_t y;
} coord_t;

typedef enum
{
    O,
    L,
    J,
    I,
    T,
    S,
    Z,
    N
} shape_type_t;

typedef struct
{
    shape_type_t type;
    coord_t offsets[OFFSETS_COUNT];
    coord_t pos;
} shape_t;

typedef struct
{
    bool shift_left_pressed;
    bool shift_left_held;
    bool shift_right_pressed;
    bool shift_right_held;
    bool rotate_right_pressed;
    bool rotate_left_pressed;
    bool soft_drop_held;
    bool hard_drop_pressed;
} input_state_t;

typedef enum
{
    CTETRIS_EVENT_NONE,
    CTETRIS_EVENT_NEW_SHAPE,

    CTETRIS_EVENT_DROP,

    CTETRIS_EVENT_HARD_DROP,
    CTETRIS_EVENT_SOFT_DROP,
    CTETRIS_EVENT_ROTATE,
    CTETRIS_EVENT_SHIFT,

    CTETRIS_EVENT_LOCK_START,
    CTETRIS_EVENT_LOCK_RESET,
    CTETRIS_EVENT_LOCK_CANCEL,
    CTETRIS_EVENT_LOCK_DONE,

    CTETRIS_EVENT_LINE_CLEAR,
} tetris_event_type_t;

typedef struct
{
    tetris_event_type_t type;
    shape_t shape;
    bool engine_inactive;
    uint32_t score;
    uint8_t level, combo;
    uint16_t lines;
    uint8_t lines_cleared_indices[4]; // At max only 4 lines can be cleared.
    uint8_t lines_cleared_count;
} tetris_event_t;

#ifdef __cplusplus
extern "C"
{
#endif

    shape_t *ctetris_shape(shape_type_t shape);

    void ctetris_init(void);

    void ctetris_input_push(input_state_t input_state);

    void ctetris_update(double delta);

    tetris_event_t ctetris_event_pop(void);

    shape_t ctetris_shape_proj_get(void);

    shape_t ctetris_shape_next_get(void);

#ifdef __cplusplus
}
#endif

#endif
