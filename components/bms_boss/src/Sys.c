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
#include "HW_gpio.h"
#include "SystemConfig.h"

/**< Other Includes */
#include "Module.h"
#include "BMS.h"
#include "IMD.h"
#include "MessageUnpack_generated.h"
#include "stdbool.h"
#include "stdint.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

SYS_S SYS;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

HW_GPIO_S led = {
    .port = LED_Port,
    .pin  = LED_Pin,
};

HW_GPIO_S shtdn = {
    .pin = BMS_STATUS_Pin,
    .port = BMS_STATUS_Port,
};

HW_GPIO_S hvp = {
    .pin = AIR_Pin,
    .port = AIR_Port,
};

HW_GPIO_S pchg = {
    .pin = PCHG_Pin,
    .port = PCHG_Port
};


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
            HW_GPIO_togglePin(&led);
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
            HW_GPIO_togglePin(&led);
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
    HW_GPIO_writePin(&shtdn, false);
}

void SYS_SFT_closeShutdown(void)
{
    HW_GPIO_writePin(&shtdn, true);
}

void SYS_SFT_openContactors(void)
{
    HW_GPIO_writePin(&hvp, false);
    HW_GPIO_writePin(&pchg, false);
    SYS.contacts = SYS_CONTACTORS_OPEN;
}

void SYS_SFT_cycleContacts(void)
{
    if (SYS.contacts == SYS_CONTACTORS_OPEN)
    {
        HW_GPIO_writePin(&pchg, true);
        SYS.contacts = SYS_CONTACTORS_PRECHARGE;
    }
    else if (SYS.contacts == SYS_CONTACTORS_PRECHARGE)
    {
        HW_GPIO_writePin(&hvp, true);        
        SYS.contacts = SYS_CONTACTORS_CLOSED;
    }
    else if (SYS.contacts == SYS_CONTACTORS_CLOSED)
    {
        HW_GPIO_writePin(&pchg, false);        
        SYS.contacts = SYS_CONTACTORS_HVP_CLOSED;
    }
}

bool SYS_SFT_checkMCTimeout(void)
{
    return (CANRX_get_signal(VEH, PM100DX_tractiveSystemVoltage).health == CANRX_MESSAGE_MIA);
}

bool SYS_SFT_checkChargerTimeout(void)
{
    return (CANRX_get_signal(VEH, BRUSA513_chargerBusVoltage).health == CANRX_MESSAGE_MIA);
}
