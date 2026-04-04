/**
 * @file Module_componentSpecific.h
 * @brief  Header file for Module Manager for the component specific modules
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "ModuleDesc.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

/**< Modules */
extern const ModuleDesc_S BMS_desc;
extern const ModuleDesc_S ENV_desc;
extern const ModuleDesc_S SYS_desc;
extern const ModuleDesc_S drv_inputAD_desc;
extern const ModuleDesc_S UDS_desc;
extern const ModuleDesc_S CANIO_rx;
extern const ModuleDesc_S CANIO_tx;

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    MODULE_CANIO_rx = 0x00U,
    MODULE_UDS,
    MODULE_INPUTAD,
    MODULE_USERINPUTINPUT,
    MODULE_BMS,
    MODULE_ENV,
    MODULE_SYS,
    MODULE_CANIO_tx,
    MODULE_CNT,
} Module_tasks_E;
