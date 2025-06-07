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
    // TODO: Improve
    for (uint8_t i = 0; i < DRV_TPS20XX_CHANNEL_COUNT; i++)
    {
        drv_tps20xx_setEnabled(i, true);
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
