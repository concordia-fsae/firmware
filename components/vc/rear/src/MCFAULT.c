/**
 * @file MCFAULT.c
 * @brief Module source that manages motor controller faults
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "MCFAULT.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "string.h"

#include "app_faultManager.h"
#include "Yamcan.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    bool faulted;
    bool timedOut;
} MCFAULT_data;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

CAN_faultStatus_E MCFAULT_getFaultedCAN(void)
{
    return MCFAULT_data.faulted ? CAN_FAULTSTATUS_FAULT : CAN_FAULTSTATUS_OK ;
}

CAN_faultStatus_E MCFAULT_getTimedOutCAN(void)
{
    return MCFAULT_data.timedOut ? CAN_FAULTSTATUS_FAULT : CAN_FAULTSTATUS_OK;
}

static void MCFAULT_init(void)
{
    memset(&MCFAULT_data, 0x00U, sizeof(MCFAULT_data));
}

static void MCFAULT_periodic_100Hz(void)
{
    const bool messageValid = (CANRX_validate(VEH, PM100DX_faults) == CANRX_MESSAGE_VALID);

    MCFAULT_data.timedOut = !messageValid;

    if (messageValid)
    {
        MCFAULT_data.faulted = app_faultManager_getNetworkedFault_anySet(VEH, PM100DX_faults);
    }
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S MCFAULT_desc =
{
    .moduleInit = &MCFAULT_init,
    .periodic100Hz_CLK = &MCFAULT_periodic_100Hz,
};