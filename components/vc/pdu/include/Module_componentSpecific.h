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
extern const ModuleDesc_S IO_desc;
extern const ModuleDesc_S UDS_desc;
extern const ModuleDesc_S CANIO_rx;
extern const ModuleDesc_S CANIO_tx;

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    MODULE_IO = 0x00,
    MODULE_UDS,
    MODULE_CANIO_tx,
    MODULE_CANIO_rx,
    MODULE_CNT
} Module_tasks_E;
