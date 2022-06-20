/**
 * @file NVM.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-06-19
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#pragma once

#include "SystemConfig.h"
#include "stm32f1xx_hal_conf.h"

void nvm_init(void);
HAL_StatusTypeDef nvm_write_page(uint8_t page_num);
void nvm_read_page(uint8_t page_num);
HAL_StatusTypeDef nvm_clear_page(uint8_t page_num);