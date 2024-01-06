/**
 * HW_gpio.h
 * Header file for the GPIO hardware configuration
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW.h"
#include "SystemConfig.h"
#include "stdbool.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef GPIO_TypeDef HW_GPIO_TypeDef;
typedef uint16_t HW_GPIO_Pin;

typedef struct {
    HW_GPIO_TypeDef *port;
    HW_GPIO_Pin pin;
} HW_GPIO_S;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

HW_StatusTypeDef_E HW_GPIO_Init(void);
HW_StatusTypeDef_E HW_GPIO_DeInit(void);
bool HW_GPIO_ReadPin(HW_GPIO_S*);
void HW_GPIO_WritePin(HW_GPIO_S*, bool);
void HW_GPIO_TogglePin(HW_GPIO_S*);

