/**
 * @file drv_userInput.h
 * @brief Abstraction layer for various user input devices
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_inputAD.h"
#include "drv_userInput_componentSpecific.h"

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    const enum
    {
        USERINPUT_TYPE_UNDEFINED,
        USERINPUT_TYPE_GPIO,
        USERINPUT_TYPE_CALLBACK,
    } type;
    const union
    {
        struct
        {
            drv_inputAD_channelDigital_E pin;
            uint16_t debounce_on_ms;
            uint16_t debounce_off_ms;
        } gpio;
        struct
        {
            bool (*func)(void);
        } callback;
    } config;
} drv_userInput_configButton_S;

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern drv_userInput_configButton_S userInput_configButtons[USERINPUT_BUTTON_COUNT];

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void drv_userInput_init(void);
void drv_userInput_run(void);

bool drv_userInput_buttonPressed(drv_userInput_button_E button);
bool drv_userInput_buttonInDebounce(drv_userInput_button_E button);
