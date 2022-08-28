/**
 * @file Module.h
 * @brief  Header file for Modularized functions
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-23
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "ModuleDesc.h"


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern void Module_Init(void);
extern void Module_1kHz_TSK(void);
extern void Module_100Hz_TSK(void);
extern void Module_10Hz_TSK(void);
extern void Module_1Hz_TSK(void);

