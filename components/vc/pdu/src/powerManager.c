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
#include "drv_tps2hb16ab.h"
#include "drv_vn9008.h"
#include "LIB_Types.h"
#include "string.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    float32_t total_current;
} powerManager_data;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

static void powerManager_init(void)
{
    memset(&powerManager_data, 0x00, sizeof(powerManager_data));

    drv_tps2hb16ab_init();
    drv_vn9008_init();

    drv_tps2hb16ab_setDiagEnabled(DRV_TPS2HB16AB_IC_BMS1_SHUTDOWN, true); // All diag pins are set to the same gpio
    drv_vn9008_setEnabled(DRV_VN9008_CHANNEL_PUMP, true); // All sense enable pins are set to the same gpio
}

static void powerManager_periodic_10Hz(void)
{
    // TODO: Improve
    static drv_tps2hb16ab_output_E output = DRV_TPS2HB16AB_OUT_1;

    for (uint8_t i = 0; i < DRV_TPS2HB16AB_IC_COUNT; i++)
    {
        drv_tps2hb16ab_setEnabled(i, DRV_TPS2HB16AB_OUT_1, true);
        drv_tps2hb16ab_setEnabled(i, DRV_TPS2HB16AB_OUT_2, true);
    }

    for (uint8_t i = 0; i < DRV_VN9008_CHANNEL_COUNT; i++)
    {
        drv_vn9008_setEnabled(i, true);
    }

    output = (output + 1) % DRV_TPS2HB16AB_OUT_COUNT;

    drv_tps2hb16ab_run();
    drv_vn9008_run();
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S powerManager_desc = {
    .moduleInit = &powerManager_init,
    .periodic10Hz_CLK = &powerManager_periodic_10Hz,
};
