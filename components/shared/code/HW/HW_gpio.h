/**
 * @file HW_gpio.h
 * @brief  Header file for GPIO firmware
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW.h"
// System Includes
#include "SystemConfig.h"
#include "stdbool.h"

#include "HW_gpio_componentSpecific.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef GPIO_TypeDef HW_GPIO_typeDef;
typedef uint16_t     HW_GPIO_pin;
typedef uint32_t     HW_GPIO_mode;
typedef uint32_t     HW_GPIO_speed;
typedef uint32_t     HW_GPIO_pull;

typedef enum
{
    HW_GPIO_NOSET = 0U,
    HW_GPIO_PINRESET,
    HW_GPIO_PINSET,
} HW_GPIO_resetState_E;

typedef struct
{
    HW_GPIO_typeDef*     port;
    HW_GPIO_pin          pin;
    HW_GPIO_mode         mode;
    HW_GPIO_speed        speed;
    HW_GPIO_pull         pull;
    HW_GPIO_resetState_E resetState;
} HW_GPIO_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

HW_StatusTypeDef_E HW_GPIO_init(void);
HW_StatusTypeDef_E HW_GPIO_deInit(void);
bool HW_GPIO_readPin(HW_GPIO_pinmux_E pin);
void HW_GPIO_writePin(HW_GPIO_pinmux_E pin, bool state);
void HW_GPIO_togglePin(HW_GPIO_pinmux_E pin);
