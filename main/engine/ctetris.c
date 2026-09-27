#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ctetris.h"

#define TIMER_INACTIVE (-1.0f) // Timer won't be incremented.
#define TIMER_START (0.0f)     // resets the timer.

#define EVENT_QUEUE_CAP 10

typedef enum
{
    STATE_INACTIVE,
    STATE_DROPPING,
    STATE_LOCKING
} engine_state_t;

typedef enum
{
    LEFT,
    RIGHT,
    DOWN
} movement_type_t;

typedef enum
{
    CLOCKWISE,
    COUNTER_CLOCKWISE
} rotation_type_t;

typedef struct
{
    bool hard_drop;
    bool soft_drop;
    uint8_t moves;
} update_context_t;

static shape_t SHAPE_O = {.type = O, .offsets = {{0, 0}, {1, 0}, {0, 1}, {1, 1}}, .pos = {0, 0}};
static shape_t SHAPE_L = {.type = L, .offsets = {{-1, 0}, {0, 0}, {1, 0}, {1, 1}}, .pos = {0, 0}};
static shape_t SHAPE_I = {.type = I, .offsets = {{-1, 0}, {0, 0}, {1, 0}, {2, 0}}, .pos = {0, 0}};
static shape_t SHAPE_T = {.type = T, .offsets = {{-1, 0}, {0, 0}, {1, 0}, {0, 1}}, .pos = {0, 0}};
static shape_t SHAPE_S = {.type = S, .offsets = {{-1, 1}, {0, 1}, {0, 0}, {1, 0}}, .pos = {0, 0}};
static shape_t SHAPE_J = {.type = J, .offsets = {{-1, 0}, {0, 0}, {1, 0}, {-1, 1}}, .pos = {0, 0}};
static shape_t SHAPE_Z = {.type = Z, .offsets = {{-1, 0}, {0, 0}, {0, 1}, {1, 1}}, .pos = {0, 0}};

static shape_t *shape_bag[N] = {
    [O] = &SHAPE_O, [L] = &SHAPE_L, [J] = &SHAPE_J, [I] = &SHAPE_I, [T] = &SHAPE_T, [S] = &SHAPE_S, [Z] = &SHAPE_Z};

static uint8_t shape_bag_next_i;

static engine_state_t engine_state = STATE_INACTIVE;
static double engine_state_timer = TIMER_INACTIVE;

static shape_type_t grid[ROWS][COLS];

static shape_t curr_shape;
static double curr_shape_shift_timer;
static uint8_t curr_shape_y_max;
static bool curr_shape_landed;
static uint8_t curr_shape_land_moves;

static uint32_t score;
static uint8_t level, combo;
static uint16_t lines;

static input_state_t input_buffer;

static tetris_event_t event_queue[EVENT_QUEUE_CAP];
static uint8_t eq_head, eq_tail;

static void event_push(tetris_event_t ev);

static void shape_bag_shuffle(void);
static shape_t shape_bag_next_get(void);

// Shape utilities.
static uint8_t shape_y_max_get(const shape_t *shape);
static bool shape_collides(const shape_t *shape,
                           coord_t *worst_offset);
static bool shape_grounded(const shape_t *shape);
static bool shape_rotate(shape_t *shape, rotation_type_t dir);
static bool shape_move(shape_t *shape, movement_type_t dir,
                       double last_move_timer, double delay);

// Core.
static void input_buffer_process(update_context_t *ctxt);

static bool curr_shape_rotate(rotation_type_t dir);
static bool curr_shape_shift(movement_type_t dir, bool delay);

static void handle_new_move(update_context_t *ctxt);
static double get_drop_delay(bool soft_drop);
static bool handle_grounded_shape(bool hard_drop);
static void handle_airborne_shape(bool soft_drop);

static void grid_write_shape(shape_t *shape);
static void grid_clear_lines(void);
static void grid_clear(void);

static uint16_t scoring_get_bonus(uint8_t val);
static void scoring_score(tetris_event_t *ev);

static void endgame_or_reset(void);

void ctetris_init(void)
{
    srand((unsigned)time(NULL));

    eq_head = eq_tail = 0;

    engine_state = STATE_DROPPING;
    engine_state_timer = TIMER_START;

    grid_clear();
    shape_bag_shuffle();

    curr_shape = shape_bag_next_get();
    curr_shape_y_max = shape_y_max_get(&curr_shape);
    curr_shape_land_moves = 0;
    curr_shape_landed = false;
    curr_shape_shift_timer = TIMER_INACTIVE;

    score = combo = lines = 0;
    level = 1;

    memset(&input_buffer, 0, sizeof(input_state_t));

    event_push((tetris_event_t){.type = CTETRIS_EVENT_NEW_SHAPE,
                                .shape = curr_shape,
                                .engine_inactive = false});
}

void ctetris_input_push(input_state_t input_state)
{
    if (engine_state == STATE_INACTIVE)
    {
        return;
    }

    input_buffer = input_state;
}

void ctetris_update(double delta)
{
    if (engine_state == STATE_INACTIVE)
    {
        return;
    }

    // Increment active timers.
    if (engine_state_timer != TIMER_INACTIVE)
    {
        engine_state_timer += delta;
    }
    if (curr_shape_shift_timer != TIMER_INACTIVE)
    {
        curr_shape_shift_timer += delta;
    }

    update_context_t ctxt = {0};

    // Process buffered input.
    input_buffer_process(&ctxt);

    // In case there are new moves.
    handle_new_move(&ctxt);

    // In case the shape is grounded.
    if (!handle_grounded_shape(ctxt.hard_drop))
    {
        // If it is airborne.
        handle_airborne_shape(ctxt.soft_drop);
    }
}

tetris_event_t ctetris_event_pop(void)
{
    // If the queue is empty, return `CTETRIS_EVENT_NONE`.
    if (eq_head == eq_tail)
    {
        return (tetris_event_t){.type = CTETRIS_EVENT_NONE};
    }
    tetris_event_t ev = event_queue[eq_head];
    eq_head = (eq_head + 1) % EVENT_QUEUE_CAP;
    return ev;
}

shape_t ctetris_shape_proj_get(void)
{
    if (engine_state == STATE_INACTIVE)
    {
        return (shape_t){.type = N};
    }

    uint8_t max_drop = ROWS;

    shape_t proj_shape = curr_shape;

    for (uint8_t i = 0; i < OFFSETS_COUNT; i++)
    {
        int8_t x = curr_shape.pos.x + curr_shape.offsets[i].x;
        int8_t y = curr_shape.pos.y + curr_shape.offsets[i].y;

        uint8_t dist = 0;
        for (uint8_t check_y = y + 1; check_y < ROWS; check_y++)
        {
            if (grid[check_y][x] != N)
            {
                break;
            }
            dist++;
        }

        if (dist < max_drop)
        {
            max_drop = dist;
        }
    }

    proj_shape.pos.y += max_drop;
    return proj_shape;
}

shape_t ctetris_shape_next_get(void)
{
    if (engine_state == STATE_INACTIVE)
    {
        return (shape_t){.type = N};
    }

    return *shape_bag[shape_bag_next_i];
}

static void event_push(tetris_event_t ev)
{
    uint8_t next = (eq_tail + 1) % EVENT_QUEUE_CAP;
    if (next == eq_head)
    {
        return; // When the queue overflows, just don't push.
    }
    event_queue[eq_tail] = ev;
    eq_tail = next;
}

static uint8_t shape_y_max_get(const shape_t *shape)
{
    uint8_t max_y = 0;
    for (uint8_t i = 1; i < OFFSETS_COUNT; i++)
    {
        uint8_t cy = shape->pos.y + shape->offsets[i].y;
        if (cy > max_y)
        {
            max_y = cy;
        }
    }
    return max_y;
}

static void shape_bag_shuffle(void)
{
    for (uint8_t i = 6; i > 0; i--)
    {
        uint8_t j = rand() % (i + 1);
        shape_t *tmp = shape_bag[i];
        shape_bag[i] = shape_bag[j];
        shape_bag[j] = tmp;
    }
    shape_bag_next_i = 0;
}

static shape_t shape_bag_next_get(void)
{
    shape_t shape;

    if (shape_bag_next_i == N - 1)
    {
        shape = *shape_bag[shape_bag_next_i];
        shape_bag_shuffle();
    }
    else
    {
        shape = *shape_bag[shape_bag_next_i++];
    }

    int8_t y = 0;
    for (uint8_t i = 0; i < OFFSETS_COUNT; i++)
    {
        y = (shape.offsets[i].y < y) ? shape.offsets[i].y : y;
    }

    shape.pos = (coord_t){.y = -y, .x = COLS / 2};
    return shape;
}

static bool shape_collides(const shape_t *shape,
                           coord_t *worst_offset)
{
    bool hit = false;
    bool out_of_bounds = false;
    int8_t worst_dist = 0;

    for (uint8_t i = 0; i < OFFSETS_COUNT; i++)
    {
        coord_t offset = shape->offsets[i];
        int8_t x = shape->pos.x + offset.x;
        int8_t y = shape->pos.y + offset.y;

        if (x < 0 || y < 0 || x >= COLS || y >= ROWS)
        {
            if (!worst_offset)
            {
                return true;
            }

            int8_t dx = 0, dy = 0;

            if (x < 0)
            {
                dx = -x;
            }
            else if (x >= COLS)
            {
                dx = x - (COLS - 1);
            }
            if (y < 0)
            {
                dy = -y;
            }
            else if (y >= ROWS)
            {
                dy = y - (ROWS - 1);
            }

            int8_t dist = (dx > dy) ? dx : dy;

            if (!out_of_bounds || dist > worst_dist)
            {
                worst_dist = dist;
                *worst_offset = offset;
                if (dx >= dy)
                {
                    worst_offset->y = 0;
                }
                else
                {
                    worst_offset->x = 0;
                }
            }
            hit = true;
            out_of_bounds = true;
        }
        else if (!out_of_bounds && grid[y][x] != N)
        {
            if (!worst_offset)
            {
                return true;
            }

            hit = true;
            *worst_offset = offset;
        }
    }
    return hit;
}

static bool shape_grounded(const shape_t *shape)
{
    shape_t down = *shape;
    down.pos.y++;
    return shape_collides(&down, NULL);
}

static bool shape_rotate(shape_t *shape, rotation_type_t dir)
{
    if (shape->type == O)
    {
        return true;
    }

    shape_t rotated = *shape;

    for (uint8_t i = 0; i < OFFSETS_COUNT; i++)
    {
        int8_t x = rotated.offsets[i].x;
        int8_t y = rotated.offsets[i].y;
        switch (dir)
        {
        case CLOCKWISE:
            rotated.offsets[i].x = -y;
            rotated.offsets[i].y = x;
            break;
        case COUNTER_CLOCKWISE:
            rotated.offsets[i].x = y;
            rotated.offsets[i].y = -x;
            break;
        }
    }

    coord_t worst_offset = {0};
    if (!shape_collides(&rotated, &worst_offset))
    {
        *shape = rotated;
        return true;
    }

    rotated.pos.x -= worst_offset.x;
    rotated.pos.y -= worst_offset.y;
    if (!shape_collides(&rotated, &worst_offset))
    {
        *shape = rotated;
        return true;
    }

    return false;
}

static bool shape_move(shape_t *shape, movement_type_t dir,
                       double last_move_timer, double delay)
{
    shape_t next = *shape;

    switch (dir)
    {
    case DOWN:
        next.pos.y++;
        break;
    case LEFT:
        next.pos.x--;
        break;
    case RIGHT:
        next.pos.x++;
        break;
    }

    if (!shape_collides(&next, NULL))
    {
        if (last_move_timer == TIMER_INACTIVE || last_move_timer >= delay)
        {
            *shape = next;
            return true;
        }
    }
    return false;
}

static void input_buffer_process(update_context_t *ctxt)
{
    // Rotation.
    if (input_buffer.rotate_right_pressed)
    {
        if (curr_shape_rotate(CLOCKWISE))
        {
            ctxt->moves++;
        }
    }
    else if (input_buffer.rotate_left_pressed)
    {
        if (curr_shape_rotate(COUNTER_CLOCKWISE))
        {
            ctxt->moves++;
        }
    }

    // Shift.
    if (input_buffer.shift_left_pressed)
    {
        if (curr_shape_shift(LEFT, false))
        {
            ctxt->moves++;
        }
    }
    else if (input_buffer.shift_left_held)
    {
        if (curr_shape_shift(LEFT, true))
        {
            ctxt->moves++;
        }
    }
    else if (input_buffer.shift_right_pressed)
    {
        if (curr_shape_shift(RIGHT, false))
        {
            ctxt->moves++;
        }
    }
    else if (input_buffer.shift_right_held)
    {
        if (curr_shape_shift(RIGHT, true))
        {
            ctxt->moves++;
        }
    }

    ctxt->soft_drop = input_buffer.soft_drop_held;
    ctxt->hard_drop = input_buffer.hard_drop_pressed;

    // Reset everything.
    input_buffer.rotate_right_pressed = false;
    input_buffer.rotate_left_pressed = false;
    input_buffer.shift_left_pressed = false;
    input_buffer.shift_right_pressed = false;
    input_buffer.shift_left_held = false;
    input_buffer.shift_right_held = false;
    input_buffer.hard_drop_pressed = false;
    input_buffer.soft_drop_held = false;
}

static bool curr_shape_rotate(rotation_type_t dir)
{
    if (shape_rotate(&curr_shape, dir))
    {
        event_push((tetris_event_t){.type = CTETRIS_EVENT_ROTATE,
                                    .shape = curr_shape});
        return true;
    }
    return false;
}

static bool curr_shape_shift(movement_type_t dir, bool repeat)
{
    static bool already_repeating = false;

    if (dir != LEFT && dir != RIGHT)
    {
        return false;
    }

    double delay_time, timer;

    if (repeat)
    {
        if (already_repeating)
        {
            delay_time = SHIFT_DELAY_REPEAT;
        }
        else
        {
            delay_time = SHIFT_DELAY_INITIAL;
        }
        timer = curr_shape_shift_timer;
    }
    else
    {
        already_repeating = false;
        delay_time = SHIFT_DELAY_REPEAT;
        timer = TIMER_INACTIVE;
    }

    if (shape_move(&curr_shape, dir, timer, delay_time))
    {
        curr_shape_shift_timer = TIMER_START;
        event_push((tetris_event_t){.type = CTETRIS_EVENT_SHIFT,
                                    .shape = curr_shape});
        if (delay_time == SHIFT_DELAY_INITIAL)
        {
            already_repeating = true;
        }
        return true;
    }

    return false;
}

static void handle_new_move(update_context_t *ctxt)
{
    if (ctxt->moves == 0)
    {
        return;
    }

    if (engine_state == STATE_DROPPING && !curr_shape_landed)
    {
        return;
    }

    if (curr_shape_land_moves < MOVES_BEFORE_LOCK - 1)
    {
        engine_state_timer = TIMER_START;
        curr_shape_land_moves += ctxt->moves;
        if (engine_state == STATE_LOCKING)
        {
            event_push((tetris_event_t){
                .type = CTETRIS_EVENT_LOCK_RESET,
            });
        }
    }
    else
    {
        ctxt->hard_drop = true;
    }
}

static double get_drop_delay(bool soft_drop)
{
    if (soft_drop)
    {
        return SOFT_DROP_DELAY;
    }
    double delay = pow(0.8 - ((level - 1) * 0.007), level - 1);
    return delay < SOFT_DROP_DELAY ? SOFT_DROP_DELAY : delay;
}

static bool handle_grounded_shape(bool hard_drop)
{
    if (!shape_grounded(&curr_shape) && !hard_drop)
    {
        return false;
    }

    bool timer_expired = (engine_state == STATE_LOCKING &&
                          engine_state_timer >= SHAPE_LOCK_DELAY);

    if (!hard_drop && !timer_expired)
    {
        if (engine_state != STATE_LOCKING)
        {
            engine_state = STATE_LOCKING;
            engine_state_timer = TIMER_START;
            curr_shape_landed = true;
            event_push((tetris_event_t){
                .type = CTETRIS_EVENT_LOCK_START,
            });
        }
        return true;
    }

    if (hard_drop)
    {
        shape_t proj_shape = ctetris_shape_proj_get();

        uint8_t rows_dropped = proj_shape.pos.y - curr_shape.pos.y;
        score += rows_dropped * 2;
        curr_shape = proj_shape;

        if (engine_state == STATE_LOCKING)
        {
            event_push((tetris_event_t){
                .type = CTETRIS_EVENT_LOCK_CANCEL,
            });
        }

        event_push((tetris_event_t){.type = CTETRIS_EVENT_HARD_DROP,
                                    .shape = curr_shape,
                                    .score = score});
    }

    grid_write_shape(&curr_shape);

    event_push((tetris_event_t){
        .type = CTETRIS_EVENT_LOCK_DONE,
    });

    grid_clear_lines();
    endgame_or_reset();

    return true;
}

static void handle_airborne_shape(bool soft_drop)
{
    if (engine_state == STATE_LOCKING)
    {
        engine_state = STATE_DROPPING;
        engine_state_timer = TIMER_START;
        event_push((tetris_event_t){
            .type = CTETRIS_EVENT_LOCK_CANCEL,
        });
    }

    shape_t next_possible_shape = curr_shape;
    double drop_delay = get_drop_delay(soft_drop);
    if (shape_move(&next_possible_shape, DOWN, engine_state_timer,
                   drop_delay))
    {

        curr_shape = next_possible_shape;
        engine_state_timer = TIMER_START;

        if (soft_drop)
        {
            score++;
            event_push((tetris_event_t){.type = CTETRIS_EVENT_SOFT_DROP,
                                        .shape = curr_shape,
                                        .score = score});
        }
        else
        {
            event_push((tetris_event_t){
                .type = CTETRIS_EVENT_DROP,
                .shape = curr_shape,
            });
        }
    }

    uint8_t current_y_max = shape_y_max_get(&curr_shape);
    if (current_y_max > curr_shape_y_max)
    {
        curr_shape_y_max = current_y_max;
        curr_shape_land_moves = 0;
        curr_shape_landed = false;
    }
}

// Write a shape on to the grid.
static void grid_write_shape(shape_t *shape)
{
    for (uint8_t i = 0; i < OFFSETS_COUNT; i++)
    {
        uint8_t x = shape->pos.x + shape->offsets[i].x;
        uint8_t y = shape->pos.y + shape->offsets[i].y;
        grid[y][x] = shape->type;
    }
}

// Clear completed lines and settle the grid.
static void grid_clear_lines(void)
{
    tetris_event_t clear_ev = {0};
    clear_ev.type = CTETRIS_EVENT_LINE_CLEAR;

    uint8_t write = ROWS - 1;
    for (int8_t read = ROWS - 1; read >= 0; read--)
    {
        bool is_full = true;
        for (uint8_t c = 0; c < COLS; c++)
        {
            if (grid[read][c] == N)
            {
                is_full = false;
                break;
            }
        }

        if (is_full)
        {
            clear_ev.lines_cleared_indices[clear_ev.lines_cleared_count++] =
                (uint8_t)read;
            continue;
        }

        if (write != read)
        {
            memcpy(grid[write], grid[read], sizeof(grid[read]));
        }
        write--;
    }

    // Clear empty rows at the top after settling the grid.
    for (int8_t r = write; r >= 0; r--)
    {
        for (uint8_t c = 0; c < COLS; c++)
        {
            grid[r][c] = N;
        }
    }

    scoring_score(&clear_ev);

    if (clear_ev.lines == 0)
    {
        return;
    }

    event_push(clear_ev);
}

// Clear the grid.
static void grid_clear(void)
{
    for (uint8_t r = 0; r < ROWS; r++)
    {
        for (uint8_t c = 0; c < COLS; c++)
        {
            grid[r][c] = N;
        }
    }
}

// Helper to return line clear bonus based on line count.
static uint16_t scoring_get_bonus(uint8_t val)
{
    switch (val)
    {
    case 1:
        return 100;
    case 2:
        return 300;
    case 3:
        return 500;
    case 4:
        return 800;
    default:
        return 0;
    }
}

// Adjust scoring based on number of lines cleared.
static void scoring_score(tetris_event_t *ev)
{
    if (ev->lines_cleared_count)
    {
        uint16_t bonus = scoring_get_bonus(ev->lines_cleared_count);
        score += bonus * level;
        score += CALC_COMBO_POINTS(level, combo);
        lines += ev->lines_cleared_count;
        level = (lines / 10) + 1;
        ev->score = score;
        ev->level = level;
        ev->lines = lines;
        ev->combo = combo;
        combo++;
    }
    else if (combo > 0)
    {
        combo = 0;
    }
}

static void endgame_or_reset(void)
{
    // Get a new shape.
    curr_shape = shape_bag_next_get();

    tetris_event_t ev = {.type = CTETRIS_EVENT_NEW_SHAPE,
                         .shape = curr_shape,
                         .engine_inactive = false};

    // If the new shape collides, then the engine goes inactive.
    if (shape_collides(&curr_shape, NULL))
    {
        engine_state = STATE_INACTIVE;
        engine_state_timer = TIMER_INACTIVE;
        ev.engine_inactive = true;
    }
    else
    {
        // Else reset engine state for the next shape.
        engine_state = STATE_DROPPING;
        engine_state_timer = TIMER_START;
        curr_shape_land_moves = 0;
        curr_shape_y_max = shape_y_max_get(&curr_shape);
        curr_shape_shift_timer = TIMER_INACTIVE;
        curr_shape_landed = false;
    }

    event_push(ev);
}
