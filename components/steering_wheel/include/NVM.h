/**
 * @file NVM.h
 * @author Josh Lafleur (josh.lafleur@outlook.com)
 * @brief Header file of the NVM interface
 * @version 0.1
 * @date 2022-06-19
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#pragma once


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SystemConfig.h"
#include "stm32f1xx_hal_conf.h"


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

HAL_StatusTypeDef NVM_write_page(uint8_t page_num);
void NVM_read_page(uint8_t page_num);
HAL_StatusTypeDef NVM_clear_page(uint8_t page_num);