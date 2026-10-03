#include "driver/gpio.h"
#include "esp/espio.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include "matrix/hub75.h"
#include "matrix/hub75_driver.h"
#include "engine/renderer.h"
#include "engine/ui.h"
#include "engine/ctetris.h"
#include "events/edt.h"
#include "tft/tft.h"
#include "audio/audio.h"
#include <stdio.h>

Hub75Driver hub75driver;
Hub75 hub75(&hub75driver);
Renderer renderer(&hub75);
TFT tft;
Audio audio;
UI ui(&tft, &audio);

const double TICK_INC = (TICK_SIZE / 1000.0);

uint32_t _last_io = 0;

extern "C" uint32_t get_mask(gpio_num_t pin, uint32_t press_mask, uint32_t held_mask)
{
    if (gpio_get_level(pin))
    {
        if ((_last_io & press_mask) == press_mask)
        {
            _last_io |= held_mask;
            return held_mask;
        }
        else
        {
            _last_io |= press_mask;
            return press_mask;
        }
    }
    else
    {
        _last_io &= ~(press_mask | held_mask);

        return 0;
    }
}

extern "C" void app_main(void)
{
    configure_input_pin_pd(GPIO_NUM_37);
    configure_input_pin_pd(GPIO_NUM_38);
    configure_input_pin_pd(GPIO_NUM_39);
    configure_input_pin_pd(GPIO_NUM_40);

    edt_init();

    audio.init();

    hub75driver.init();

    tft.init();

    ui.init();

    renderer.init();

    ctetris_init();

    const TickType_t delay_period = pdMS_TO_TICKS(TICK_SIZE);

    TickType_t last_wake_time = xTaskGetTickCount();

    uint32_t last_io;

    for (double time = 0.0;; time += TICK_INC)
    {
        uint32_t mask = 0;

        mask |= get_mask(GPIO_NUM_37, 0x01, 0x02);
        mask |= get_mask(GPIO_NUM_38, 0x04, 0x08);
        mask |= get_mask(GPIO_NUM_39, 0x10, 0x20);
        mask |= get_mask(GPIO_NUM_40, 0x40, 0x80);

        if (renderer.input(mask))
        {
            renderer.update(time);
            renderer.render(time);
        }

        vTaskDelayUntil(&last_wake_time, delay_period);
    }
}
