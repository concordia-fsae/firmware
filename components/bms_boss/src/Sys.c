/**
 * @file Sys.c
 * @brief  Source code for System Manager
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module Header */
#include "Sys.h"

/**< HW Includes */
#include "HW.h"
#include "drv_outputAD.h"
#include "SystemConfig.h"

/**< Other Includes */
#include "Module.h"
#include "BMS.h"
#include "IMD.h"
#include "MessageUnpack_generated.h"
#include "stdbool.h"
#include "stdint.h"
#include "FeatureDefines_generated.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

SYS_S SYS;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Initializes OS' System Manager
 */
static void SYS_Init()
{
    SYS.state = SYS_RUNNING;
}

/**
 * @brief  100Hz System Manager task
 */
static void SYS100Hz_PRD()
{
    /**< Evaluate state of all systems */
    SYS.state = (BMS.fault || IMD_getState() == IMD_ERROR) ? SYS_ERROR : SYS_RUNNING;
}

/**
 * @brief  10 Hz System Manager task
 */
static void SYS10Hz_PRD()
{
    /**< 10Hz only toggles the LED in an error state */
    switch (SYS.state)
    {
        case SYS_RUNNING:
            break;
        case SYS_INIT:
            break;
        case SYS_ERROR:
            drv_outputAD_toggleDigitalState(DRV_OUTPUTAD_DIGITAL_LED);
            break;
    }
}

/**
 * @brief  1 Hz System Manager task
 */
static void SYS1Hz_PRD()
{
    /**< 1Hz only toggles the LED in the running state */
    switch (SYS.state)
    {
        case SYS_RUNNING:
            drv_outputAD_toggleDigitalState(DRV_OUTPUTAD_DIGITAL_LED);
            break;
        case SYS_INIT:
            break;
        case SYS_ERROR:
            break;
    }
}

/**
 * @brief  System Manager Module descriptor
 */
const ModuleDesc_S SYS_desc = {
    .moduleInit        = &SYS_Init,
    .periodic100Hz_CLK = &SYS100Hz_PRD,
    .periodic10Hz_CLK  = &SYS10Hz_PRD,
    .periodic1Hz_CLK   = &SYS1Hz_PRD,
};

void SYS_SFT_openShutdown(void)
{
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_STATUS_BMS, DRV_IO_INACTIVE);
}

void SYS_SFT_closeShutdown(void)
{
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_STATUS_BMS, DRV_IO_ACTIVE);
}

void SYS_SFT_openContactors(void)
{
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_AIR, DRV_IO_INACTIVE);
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_PRECHG, DRV_IO_INACTIVE);
    SYS.contacts = SYS_CONTACTORS_OPEN;
}

void SYS_SFT_cycleContacts(void)
{
    if (SYS.contacts == SYS_CONTACTORS_OPEN)
    {
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_PRECHG, DRV_IO_ACTIVE);
        SYS.contacts = SYS_CONTACTORS_PRECHARGE;
    }
    else if (SYS.contacts == SYS_CONTACTORS_PRECHARGE)
    {
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_AIR, DRV_IO_ACTIVE);
        SYS.contacts = SYS_CONTACTORS_CLOSED;
    }
    else if (SYS.contacts == SYS_CONTACTORS_CLOSED)
    {
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_PRECHG, DRV_IO_INACTIVE);
        SYS.contacts = SYS_CONTACTORS_HVP_CLOSED;
    }
}

bool SYS_SFT_checkMCTimeout(void)
{
    return (CANRX_validate(VEH, PM100DX_criticalData) != CANRX_MESSAGE_VALID);
}

bool SYS_SFT_checkBrusaChargerTimeout(void)
{
    return (CANRX_validate(VEH, BRUSA513_criticalData) != CANRX_MESSAGE_VALID);
}

void SYS_stopCharging(void)
{
    BMS.charging_paused = true;
}

void SYS_continueCharging(void)
{
    BMS.charging_paused = false;
}

bool SYS_SFT_checkElconChargerTimeout(void)
{
    return (CANRX_validate(PRIVBMS, ELCON_criticalData) != CANRX_MESSAGE_VALID);
}
