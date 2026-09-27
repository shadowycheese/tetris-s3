#ifndef EDT_h
#define EDT_h

#include "esp_log.h"

typedef struct
{
    job_type_t type;
    union
    {
        metric_event_t metric_event;
        system_event_t system_event;
    };
} edt_job_t;

using metric_event_handler_t = std::function<void(metric_event_t)>;
using system_event_handler_t = std::function<void(system_event_t)>;

#ifdef __cplusplus
extern "C"
{
#endif
    extern void edt_init();
    extern void edt_add_metric_event_handler(metric_id_t metric_id, metric_event_handler_t callback);
    extern void edt_add_system_event_handler(system_event_id_t event_id, system_event_handler_t callback);
    extern void edt_post(edt_job_t job);

#ifdef __cplusplus
}
#endif

#endif