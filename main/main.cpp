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

class ButtonDebounce
{
public:
    bool is_pressed(button_press_type_t press)
    {
        if (_last_value == NotPressed)
        {
            _last_value = press;

            if (press != NotPressed)
            {
                return true;
            }

            return false;
        }
        else
        {
            _last_value = press;

            return false;
        }
    }

private:
    button_press_type_t _last_value;
};

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
    ButtonDebounce pause;

    TickType_t last_wake_time = xTaskGetTickCount();

    ESP_LOGI("APP_MAIN", "Running main loop on core %d", xPortGetCoreID());

    for (double time = 0.0;; time += TICK_INC)
    {
        uint64_t now = esp_timer_get_time();

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

        if (pause.is_pressed(d))
        {
            if (renderer.is_paused())
            {
                renderer.resume();
            }
            else
            {
                renderer.pause();
            }
        }

        if (renderer.is_paused())
        {
            if (a == Pressed)
            {
                audio.toggle_mute();
            }

            continue;
        }

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

        uint64_t io_time = esp_timer_get_time() - now;

        if (renderer.input(&input_state))
        {
            renderer.update(time);
            renderer.render(time);
        }
        else
        {
        }

        uint64_t frame_time = (esp_timer_get_time() - now) - io_time;

        if (io_time > 3000)
        {
            printf("Timings: IO=%0.1f, Frame=%0.1f\n", (float)io_time / 1000.0f, (float)frame_time / 1000.0f);
        }

        vTaskDelayUntil(&last_wake_time, delay_period);
    }
}
