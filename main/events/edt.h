#ifndef EDT_h
#define EDT_h

#include <functional>
#include "esp_log.h"
#include "engine/ctetris.h"

typedef enum : uint8_t
{
    EVENT_GAME_STATE,
    EVENT_NEXT_SHAPE,
    EVENT_SCORE_UPDATE,
    EVENT_ROW_CLEARED,
    EVENT_NORMAL_DROP,
    EVENT_SOFT_DROP,
    EVENT_HARD_DROP,
    EVENT_COUNT,
} game_event_type_t;

typedef enum : uint8_t
{
    GAME_STATE_RESET,
    GAME_STATE_GAME_OVER,
    GAME_STATE_PAUSED,
    GAME_STATE_ACTIVE,
} game_state_t;

typedef struct
{
    game_event_type_t event_type;
    shape_type_t shape_type;
    game_state_t game_state;
    uint8_t cleared_rows;
    uint16_t total_rows;
    uint8_t level;
    uint8_t state;
    uint32_t score;
} edt_job_t;

using game_event_handler_t = std::function<void(edt_job_t)>;

#ifdef __cplusplus
extern "C"
{
#endif
    extern void edt_init();
    extern void edt_post(edt_job_t job);

    extern bool edt_add_game_event_handler(game_event_type_t event_type, game_event_handler_t cb);

#ifdef __cplusplus
}
#endif

#endif