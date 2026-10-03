#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "events/edt.h"

const char *EDT_TAG = "EDT";

class EventDispatcher
{
public:
    static const size_t MAX_LISTENERS = 8;

    void init()
    {
        size_t event_bytes = EVENT_COUNT * MAX_LISTENERS * sizeof(game_event_handler_t);

        _game_event_handlers = (game_event_handler_t(*)[MAX_LISTENERS])heap_caps_malloc(event_bytes, MALLOC_CAP_DEFAULT);

        memset(_game_event_handlers, 0, event_bytes);
    }

    bool add_game_event_handler(game_event_type_t event_type, game_event_handler_t cb)
    {
        if (event_type >= EVENT_COUNT)
        {
            ESP_LOGE("EDT", "Add sytem event handler add failed: Invalid id %d [max:%d]", event_type, EVENT_COUNT);
            return false;
        }

        for (int i = 0; i < MAX_LISTENERS; i++)
        {
            if (!_game_event_handlers[event_type][i])
            {
                _game_event_handlers[event_type][i] = cb;

                return true;
            }
        }

        ESP_LOGE("EDT", "Add sytem event handler add failed: %d listener count full", event_type);
        return false;
    }

    void dispatch_game_event(edt_job_t job)
    {
        if (job.event_type >= EVENT_COUNT)
        {
            ESP_LOGE("EDT", "Invalid game event: %d", job.event_type);
            return;
        }

        for (int i = 0; i < MAX_LISTENERS; i++)
        {
            if (_game_event_handlers[job.event_type][i])
            {
                ESP_LOGI(EDT_TAG, "Dispatching game event %d", job.event_type);
                _game_event_handlers[job.event_type][i](job);
            }
            else
            {
                break;
            }
        }
    }

private:
    game_event_handler_t (*_game_event_handlers)[MAX_LISTENERS];
};

EventDispatcher _event_dispatcher;

static QueueHandle_t _edt_job_queue = NULL;

bool edt_add_game_event_handler(game_event_type_t event_type, game_event_handler_t cb)
{
    return _event_dispatcher.add_game_event_handler(event_type, cb);
}

void edt_post(edt_job_t job)
{
    xQueueSend(_edt_job_queue, &job, portMAX_DELAY);
}

void edt_task(void *pvParameters)
{
    edt_job_t job;

    for (;;)
    {
        // Block indefinitely until ANY producer posts a job to the queue
        if (xQueueReceive(_edt_job_queue, &job, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(EDT_TAG, "Handling event %d", job.event_type);

            _event_dispatcher.dispatch_game_event(job);
        }
    }
}

void edt_init()
{
    ESP_LOGI(EDT_TAG, "Starting EDT...");

    _edt_job_queue = xQueueCreate(128, sizeof(edt_job_t));

    _event_dispatcher.init();

    xTaskCreatePinnedToCore(edt_task, "edt_task", 8192, NULL, 2, NULL, 1);
}
