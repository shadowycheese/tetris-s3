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
#include "controller/controller.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdio.h>

Hub75Driver hub75_driver;
Hub75 hub75(&hub75_driver);
Renderer renderer(&hub75);
TFT tft;
Audio audio;
UI ui(&tft, &audio);
JoyPad joy_pad;

#define TICK_SIZE (10)
#define RESET_TICKS (25)

const double TICK_INC = (TICK_SIZE / 1000.0);

extern "C" void app_main(void)
{
    nvs_flash_init();

    edt_init();

    audio.init();

    hub75_driver.init();

    tft.init();

    ui.init();

    joy_pad.init();

    renderer.init();

    const TickType_t delay_period = pdMS_TO_TICKS(TICK_SIZE);
    int reset_count = 0;

    TickType_t last_wake_time = xTaskGetTickCount();

    for (double time = 0.0;; time += TICK_INC)
    {
        button_press_type_t a = joy_pad.button_a();
        button_press_type_t b = joy_pad.button_b();
        button_press_type_t c = joy_pad.button_c();
        button_press_type_t d = joy_pad.button_d();
        direction_press_type_t lr = joy_pad.left_right();
        direction_press_type_t ud = joy_pad.up_down();

        input_state_t input_state = {
            .shift_left_pressed = (lr == NegativePressed),
            .shift_left_held = (lr == NegativeHold),
            .shift_right_pressed = (lr == PositivePressed),
            .shift_right_held = (lr == PositiveHold),
            .rotate_right_pressed = (a == Pressed),
            .rotate_left_pressed = (b == Pressed),
            .soft_drop_held = (ud == NegativeHold),
            .hard_drop_pressed = (c == Pressed),
        };

        if ((a == Hold) && (b == Hold) && (c == Hold) && (d == Hold))
        {
            reset_count++;

            if (reset_count >= RESET_TICKS)
            {
                if (reset_count == RESET_TICKS)
                {
                    time = 0.0f;

                    renderer.reset();
                }

                continue;
            }
        }
        else
        {
            reset_count = 0;
        }

        if (renderer.input(&input_state))
        {
            renderer.update(time);
            renderer.render(time);
        }
        else
        {
        }

        vTaskDelayUntil(&last_wake_time, delay_period);
    }
}
