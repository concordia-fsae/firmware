/**
 * @file drv_io.h
 * @brief  Header file for the generic components of the input and output driver
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_gpio.h"

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

/**
 * @brief The logic level of the pin. This is conceptually distinct from the active
 *        state of the input
 * @value DRV_IO_LOGIC_LOW The logic level is low, representing 0V on the input.
 * @value DRV_IO_LOGIC_HIGH The logic level is high, representing VDD or some
 *        other voltage.
 */
typedef enum
{
    DRV_IO_LOGIC_LOW = 0U,
    DRV_IO_LOGIC_HIGH,
} drv_io_logicLevel_E;

/**
 * @brief The current state of the switch. For convention and clarity, an active
 *        switch is one currently being pressed, is latched on, or otherwise not
 *        in it's default resting state. A signal is active when it is the same
 *        as it's configured `active_state` parameter. The active state of a switch
 *        is the cleaned, debounced, and nominal state of the input.
 * @value DRV_IO_INACTIVE Switch is presently in it's default state.
 * @value DRV_IO_ACTIVE Switch is presently actuated.
 */
typedef enum
{
    DRV_IO_INACTIVE = 0x00,
    DRV_IO_ACTIVE,
} drv_io_activeState_E;

/**
 * @brief Configuration of a digital input or output pin. A pin which is DRV_IO_ACTIVE
 *        will have it's active_state, otherwise it will be DRV_IO_INACTIVE
 */
typedef struct
{
    HW_GPIO_pinmux_E    pin;
    drv_io_logicLevel_E active_level;
} drv_io_pinConfig_S;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

drv_io_logicLevel_E drv_io_invertLogicLevel(drv_io_logicLevel_E level);
