/**
 * @file HW_gpio.h
 * @brief  Header file for GPIO firmware
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "SystemConfig.h"
#include "stdbool.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef GPIO_TypeDef HW_GPIO_TypeDef;

typedef uint16_t     HW_GPIO_Pin;

typedef struct
{
    HW_GPIO_TypeDef* port;
    HW_GPIO_Pin      pin;
} HW_GPIO_S;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void HW_GPIO_init(void);
void HW_GPIO_deInit(void);
bool HW_GPIO_readPin(HW_GPIO_S* dev);
void HW_GPIO_writePin(HW_GPIO_S* dev, bool state);
void HW_GPIO_togglePin(HW_GPIO_S* dev);
