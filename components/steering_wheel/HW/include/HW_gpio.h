/**
 * HW_gpio.h
 * Header file for the GPIO hardware configuration
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SystemConfig.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    HW_PIN_RESET = GPIO_PIN_RESET,
    HW_PIN_SET   = GPIO_PIN_SET,
} HW_GPIO_PinState_E;

typedef GPIO_TypeDef HW_GPIO_TypeDef;

typedef struct
{
    HW_GPIO_TypeDef* port;
    uint16_t         pin;
} HW_GPIO_Input_S;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void MX_GPIO_Init(void);

HW_GPIO_PinState_E HW_GPIO_ReadPin(const HW_GPIO_Input_S *input);

#ifdef __cplusplus
}
#endif
