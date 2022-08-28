/**
 * @file HW_gpio.h
 * @brief  Firmware interface for GPIO
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-02
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SystemConfig.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum {
    HW_PIN_RESET = GPIO_PIN_RESET,
    HW_PIN_SET = GPIO_PIN_SET,
} HW_GPIO_PinState_E;

typedef GPIO_TypeDef HW_GPIO_TypeDef;

typedef struct
{
    HW_GPIO_TypeDef* port;
    uint16_t         pin;
} HW_GPIO_S;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void HW_GPIO_Init(void);
HW_GPIO_PinState_E HW_GPIO_ReadPin(const HW_GPIO_S *input);
void HW_GPIO_WritePin(const HW_GPIO_S *output, HW_GPIO_PinState_E PinState);

