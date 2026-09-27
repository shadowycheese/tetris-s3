#include "driver/gpio.h"
#include "esp/espio.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <matrix/hub75.h>
#include <matrix/hub75_driver.h>
#include <engine/renderer.h>
#include <engine/ctetris.h>
#include <stdio.h>

Hub75Driver hub75driver;
Hub75 hub75(&hub75driver);
Renderer renderer(&hub75);

const double TICK_INC = (TICK_SIZE / 1000.0);

extern "C" void app_main(void)
{
    Hub75Driver::Hub75DriverConfig cfg = {
        .clock_div_num = 10,
        .clk_invert = false,
        .swap_byte_order = false, //
    };

    hub75driver.init(cfg);

    renderer.init();

    ctetris_init();

    const TickType_t delay_period = pdMS_TO_TICKS(TICK_SIZE);

    TickType_t last_wake_time = xTaskGetTickCount();

    for (double time = 0.0;; time += TICK_INC)
    {
        if (renderer.input())
        {
            renderer.update(time);
            renderer.render(time);
        }

        vTaskDelayUntil(&last_wake_time, delay_period);
    }
}
