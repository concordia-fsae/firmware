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
#include "app_vehicleState.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

/**< Modules */
extern const ModuleDesc_S CANIO_rx;
extern const ModuleDesc_S UDS_desc;
extern const ModuleDesc_S powerManager_desc;
extern const ModuleDesc_S brakeLight_desc;
extern const ModuleDesc_S horn_desc;
extern const ModuleDesc_S tssi_desc;
extern const ModuleDesc_S brakePressure_desc;
extern const ModuleDesc_S shockpot_desc;
extern const ModuleDesc_S CANIO_tx;

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    MODULE_CANIO_rx = 0x00U,
    MODULE_UDS,
    MODULE_VEHICLESTATE,
    MODULE_POWERMANAGER,
    MODULE_BRAKELIGHT,
    MODULE_HORN,
    MODULE_TSSI,
    MODULE_BRAKEPRESSURE,
    MODULE_SHOCKPOT,
    MODULE_CANIO_tx,
    MODULE_CNT
} Module_tasks_E;
