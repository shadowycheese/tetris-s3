#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "driver/i2c_master.h"
#include "esp_log.h"
#include <string.h>

#define I2C_ADDRESS 0x5A
#define I2C_SDA GPIO_NUM_37
#define I2C_SCL GPIO_NUM_38
#define I2C_SPEED 115200

#define PRESS_DOWN 0
#define PRESS_UP 1
#define PRESS_REPEAT 2
#define SINGLE_CLICK 3
#define DOUBLE_CLICK 4
#define LONG_PRESS_START 5
#define LONG_PRESS_HOLD 6
#define NONE_PRESS 8

#define BUTTON_A 0
#define BUTTON_B 1
#define BUTTON_C 2
#define BUTTON_D 3
#define BUTTON_OK 4
#define BUTTON_L 5
#define BUTTON_R 6
#define BUTTON_U 7
#define BUTTON_D 8

#define BUTTON_D_REG 0x24
#define BUTTON_B_REG 0x23
#define BUTTON_A_REG 0x22
#define BUTTON_C_REG 0x21
#define BUTTON_OK_REG 0x20
#define JOYSTICK_LEFT_X_REG 0x10
#define JOYSTICK_LEFT_Y_REG 0x11

typedef enum
{
    NotPressed,
    Pressed,
    Hold
} button_press_type_t;

typedef enum
{
    Neutral,
    NegativePressed,
    NegativeHold,
    PositivePressed,
    PositiveHold,
} direction_press_type_t;

class JoyPad
{
public:
    void init()
    {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = I2C_SDA,
            .scl_io_num = I2C_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .flags = {
                .enable_internal_pullup = true,
            },
        };

        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &_bus_handle));

        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = I2C_ADDRESS,
            .scl_speed_hz = I2C_SCL,
        };

        ESP_ERROR_CHECK(i2c_master_bus_add_device(_bus_handle, &dev_config, &_joystick_handle));
    }

    button_press_type_t button_a()
    {
        return button(BUTTON_A, BUTTON_A_REG);
    }

    button_press_type_t button_b()
    {
        return button(BUTTON_B, BUTTON_B_REG);
    }

    button_press_type_t button_c()
    {
        return button(BUTTON_C, BUTTON_C_REG);
    }

    button_press_type_t button_d()
    {
        return button(BUTTON_D, BUTTON_D_REG);
    }

    button_press_type_t button_ok()
    {
        return button(BUTTON_OK, BUTTON_OK_REG);
    }

    direction_press_type_t left_right()
    {
        return axis(BUTTON_L, BUTTON_R, JOYSTICK_LEFT_X_REG);
    }

    direction_press_type_t up_down()
    {
        return axis(BUTTON_U, BUTTON_D, JOYSTICK_LEFT_Y_REG);
    }

    int8_t horizontal_axis()
    {
        return read_register(JOYSTICK_LEFT_X_REG);
    }

    int8_t vertical_axis()
    {
        return read_register(JOYSTICK_LEFT_Y_REG);
    }

private:
    i2c_master_bus_handle_t _bus_handle;
    i2c_master_dev_handle_t _joystick_handle;

    bool _last_value[9];
    int8_t _left_right_hold;
    int8_t _up_down_hold;

    direction_press_type_t axis(int neg_button, int pos_button, uint8_t reg)
    {
        uint8_t val = read_register(reg);

        if (val < 0x7F)
        {
            _last_value[pos_button] = false;

            if (_last_value[neg_button])
            {
                return NegativeHold;
            }
            else
            {
                _last_value[neg_button] = true;

                return NegativePressed;
            }
        }
        else if (val > 0x81)
        {
            _last_value[neg_button] = false;

            if (_last_value[pos_button])
            {
                return PositiveHold;
            }
            else
            {
                _last_value[pos_button] = true;

                return PositivePressed;
            }
        }

        _last_value[pos_button] = false;
        _last_value[neg_button] = false;

        return Neutral;
    }

    button_press_type_t button(int button, uint8_t reg)
    {
        uint8_t val = read_register(reg);

        bool pressed = (val == PRESS_DOWN) || (val == LONG_PRESS_START) || (val == LONG_PRESS_HOLD);

        if (pressed)
        {
            if (_last_value[button])
            {
                return Hold;
            }
            else
            {
                _last_value[button] = true;

                return Pressed;
            }
        }

        _last_value[button] = false;

        return NotPressed;
    }

    uint8_t read_register(uint8_t reg_addr)
    {

        uint8_t write_buf[1] = {reg_addr};
        uint8_t read_buf[1] = {0};

        esp_err_t ret = i2c_master_transmit_receive(
            _joystick_handle,
            write_buf, sizeof(write_buf),
            read_buf, sizeof(read_buf),
            -1);

        if (ret == ESP_OK)
        {
            return read_buf[0];
        }
        else
        {
            return -1;
        }
    }
};

#endif