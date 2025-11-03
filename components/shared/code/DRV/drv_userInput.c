/**
 * @file drv_userInput.c
 * @brief Abstraction layer for various user input devices
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_userInput.h"
#include "stdbool.h"
#include "drv_timer.h"

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    drv_timer_S debounce_timer;
    bool        is_pressed;
} buttonData_S;

typedef struct
{
    buttonData_S buttons[USERINPUT_BUTTON_COUNT];
} data_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static data_S data;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void drv_userInput_init(void)
{
    for (uint8_t i = 0U; i < USERINPUT_BUTTON_COUNT; i++)
    {
        switch (userInput_configButtons[i].type)
        {
            case USERINPUT_TYPE_GPIO:
                drv_timer_init(&data.buttons[i].debounce_timer);
                break;
            default:
                break;
        }

        data.buttons[i].is_pressed = false;
    }
}

void drv_userInput_run(void)
{
    for (uint8_t i = 0U; i < USERINPUT_BUTTON_COUNT; i++)
    {
        switch (userInput_configButtons[i].type)
        {
            case USERINPUT_TYPE_GPIO:
            {
                const bool is_active = drv_inputAD_getDigitalActiveState(userInput_configButtons[i].config.gpio.pin) == DRV_IO_ACTIVE;

                if (is_active == data.buttons[i].is_pressed)
                {
                    drv_timer_stop(&data.buttons[i].debounce_timer);
                }
                else
                {
                    drv_timer_state_E timer_state = drv_timer_getState(&data.buttons[i].debounce_timer);
                    if (timer_state == DRV_TIMER_STOPPED)
                    {
                        drv_timer_start(&data.buttons[i].debounce_timer, is_active ?
                            userInput_configButtons[i].config.gpio.debounce_on_ms :
                            userInput_configButtons[i].config.gpio.debounce_off_ms);
                    }
                    else if (timer_state == DRV_TIMER_EXPIRED)
                    {
                        data.buttons[i].is_pressed = is_active;
                    }
                }
                break;
            }
            case USERINPUT_TYPE_CALLBACK:
                data.buttons[i].is_pressed = userInput_configButtons[i].config.callback.func();
                break;
            default:
                break;
        }
    }
}

bool drv_userInput_buttonPressed(drv_userInput_button_E button)
{
    return data.buttons[button].is_pressed;
}

bool drv_userInput_buttonInDebounce(drv_userInput_button_E button)
{
    return drv_timer_getState(&data.buttons[button].debounce_timer) == DRV_TIMER_RUNNING;
}
