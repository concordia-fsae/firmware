/**
 * @file powerManager.c
 * @brief Module source that manages the power outputs of the board
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "powerManager.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "drv_tps20xx.h"
#include "LIB_Types.h"
#include "string.h"
#include "app_faultManager.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
} powerManager_data;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

static void powerManager_init(void)
{
    memset(&powerManager_data, 0x00, sizeof(powerManager_data));

    drv_tps20xx_init();
}

static void powerManager_periodic_10Hz(void)
{
    const bool sleeping = app_vehicleState_sleeping();
    const drv_tps20xx_state_E stateCrit = drv_tps20xx_getState(DRV_TPS20XX_CHANNEL_5V_CRITICAL);
    const drv_tps20xx_state_E stateExt = drv_tps20xx_getState(DRV_TPS20XX_CHANNEL_5V_EXT);
    const bool faultedCrit = (stateCrit == DRV_TPS20XX_STATE_FAULTED_OC) || (stateCrit == DRV_TPS20XX_STATE_FAULTED_OT);
    const bool faultedExt = (stateExt == DRV_TPS20XX_STATE_FAULTED_OC) || (stateExt == DRV_TPS20XX_STATE_FAULTED_OT);

    app_faultManager_setFaultState(FM_FAULT_VCFRONT_FAULTED5VCRITICAL, faultedCrit);
    app_faultManager_setFaultState(FM_FAULT_VCFRONT_FAULTED5VEXT, faultedExt);

    // TODO: Improve
    for (uint8_t i = 0; i < DRV_TPS20XX_CHANNEL_COUNT; i++)
    {
        drv_tps20xx_setEnabled(i, !sleeping);
    }

    drv_tps20xx_run();
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S powerManager_desc = {
    .moduleInit = &powerManager_init,
    .periodic10Hz_CLK = &powerManager_periodic_10Hz,
};
