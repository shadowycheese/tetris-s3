#ifndef ESPIO_H
#define ESPIO_H

#include <stdint.h>
#include "driver/gpio.h"

#ifdef __cplusplus
#include <cstring>
#else
#include <string.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    extern esp_err_t configure_output_pin(gpio_num_t pin);
    extern esp_err_t configure_output_pin_high(gpio_num_t pin);
    extern esp_err_t configure_output_pin_low(gpio_num_t pin);

    extern esp_err_t configure_input_pin(gpio_num_t pin, gpio_int_type_t interruptType);
    extern esp_err_t configure_input_pin_pd(gpio_num_t pin);
    extern esp_err_t configure_input_pin_pu(gpio_num_t pin);

    extern esp_err_t disable_pin(gpio_num_t pin);

#ifdef __cplusplus
}
#endif

#endif
