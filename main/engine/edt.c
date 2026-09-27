#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "events/edt.h"
#include "log/debug.h"

const char *EDT_TAG = "EDT";

static QueueHandle_t _edt_job_queue = NULL;

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
