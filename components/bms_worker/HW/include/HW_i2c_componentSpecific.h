/**
 * @file HW_i2c.h
 * @brief  Header file for the I2C firmware/hardware interface
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW.h"

#include "SystemConfig.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

extern I2C_HandleTypeDef i2c2;
extern DMA_HandleTypeDef hdma_i2c2_rx;
extern DMA_HandleTypeDef hdma_i2c2_tx;
