/**
 * @file drv_userInput_componentSpecific.h
 * @brief Abstraction layer for various user input devices
 */

#pragma once


/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    USERINPUT_BUTTON_LEFT_TOP = 0x00U,
    USERINPUT_BUTTON_LEFT_MID,
    USERINPUT_BUTTON_LEFT_BOT,
    USERINPUT_BUTTON_RIGHT_TOP,
    USERINPUT_BUTTON_RIGHT_MID,
    USERINPUT_BUTTON_RIGHT_BOT,
    USERINPUT_BUTTON_LEFT_TOGGLE,
    USERINPUT_BUTTON_RIGHT_TOGGLE,
    USERINPUT_BUTTON_COUNT,
} drv_userInput_button_E;
