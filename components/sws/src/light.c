/**
 * @file light.c
 * @brief Module source that manages the light
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "drv_outputAD.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

static void light_init(void)
{
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_LED, DRV_IO_INACTIVE);
}

static void light_periodic_1Hz(void)
{
    drv_outputAD_toggleDigitalState(DRV_OUTPUTAD_DIGITAL_LED);
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S light_desc = {
    .moduleInit = &light_init,
    .periodic1Hz_CLK = &light_periodic_1Hz,
};
