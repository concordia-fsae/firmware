/**
 * IO_Digital.h
 * Digital IO Module Header
 * @note To transfer this module to another system, the digitalInput_E and the
 *      typedef'd IO_Digital_Flag_t must be modified to be congruent with the new system
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Types.h"
#include "HW_gpio.h" /**< Needed for GPIO_TypeDef */


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    SW1 = 0U,
    SW2,
    SW4,
    SW5,
    BTN_TOP_LEFT,
    BTN_MIDDLE,
    BTN_TOP_RIGHT,
    NUM_INPUTS
} digitalInput_E;

typedef uint8_t IO_Digital_Flag_t; /**< Current Implementation can handle upto 8 inputs */
_Static_assert(NUM_INPUTS <= (sizeof(IO_Digital_Flag_t) * 8), 
    "Size of DigitalInput_E must be less than or equal to the width of IO_Digital_Flag_t");

typedef struct
{
    GPIO_TypeDef* port;
    uint16_t      pin;
} IO_Digital_Input_S;

typedef struct
{
    const IO_Digital_Input_S inputs[NUM_INPUTS];
    IO_Digital_Flag_t        inputState;
} IO_Digital_S;


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

extern IO_Digital_S IO_DIGITAL;