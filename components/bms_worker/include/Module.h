/**
 * @file Module.h
 * @brief  Header file for Module Manager
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-28
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "ModuleDesc.h"


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

/**< Modules */
extern const ModuleDesc_S BMS_desc;
extern const ModuleDesc_S Cooling_desc;
extern const ModuleDesc_S Environment_desc;
extern const ModuleDesc_S Sys_desc;
extern const ModuleDesc_S IO_desc;
extern const ModuleDesc_S CANIO_rx;
extern const ModuleDesc_S CANIO_tx;

/**< Module tasks to get called by the RTOS */
extern void Module_Init(void);
extern void Module_10kHz_TSK(void);
extern void Module_1kHz_TSK(void);
extern void Module_100Hz_TSK(void);
extern void Module_10Hz_TSK(void);
extern void Module_1Hz_TSK(void);
