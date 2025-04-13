/**
 * @file Module.c
 * @brief  Source code for Module Manager
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module Header */
#include "Module.h"
#include "drv_tps20xx.h"
#include "drv_inputAD.h"
#include "drv_pedalMonitor.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

/**
 * @brief  Modules run by the Module Manager. Order will apply to execution.
 */
const ModuleDesc_S* modules[MODULE_CNT] = {
    &CANIO_rx,
    &UDS_desc,
    &apps_desc,
    &bppc_desc,
    &app_vehicleState_desc,
    &torque_desc,
    &CANIO_tx,
};

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void Module_componentSpecific_Init(void)
{
    // Initialize drivers prior to application runtime
    drv_inputAD_init_componentSpecific();
    drv_tps20xx_init();
    drv_pedalMonitor_init();
}

/**
 * #brief Run the pre 10Hz task functions
 * @note typically reserved for drivers
 */
void Module_componentSpecific_10Hz(void)
{
    drv_tps20xx_run();
}

/**
 * #brief Run the pre 100Hz task functions
 * @note typically reserved for drivers
 */
void Module_componentSpecific_100Hz(void)
{
    drv_pedalMonitor_run();
}

/**
 * #brief Run the pre 1kHz task functions
 * @note typically reserved for drivers
 */
void Module_componentSpecific_1kHz(void)
{
    drv_inputAD_1kHz_componentSpecific();
}
