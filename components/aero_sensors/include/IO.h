/**
 * @file IO.h
 * @brief  Header file for ARS Input/Output
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-23
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_gpio.h"
#include "Files.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    BUTTON = 0,
    INPUT_COUNT,
} IO_Input_E;

typedef enum
{
    RED_LED = 0,
    GREEN_LED,
    BLUE_LED,
    LED_COUNT,
} IO_Output_E;
