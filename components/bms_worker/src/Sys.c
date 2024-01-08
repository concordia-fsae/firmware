/**
 * @file Sys.c
 * @brief  Source code for System Manager
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-27
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module Header */
#include "Sys.h"

/**< HW Includes */
#include "SystemConfig.h"

/**< Other Includes */
#include "Module.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/**
 * @brief  System Manager Public declaration
 */
Sys_S SYS;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

/**
 * @brief  LED GPIO descriptor
 */
HW_GPIO_S led = {
    .port = LED_GPIO_Port,
    .pin  = LED_Pin,
};

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Initializes OS' System Manager
 */
static void Sys_Init()
{
    SYS.state = SYS_INIT;

    SYS.state = SYS_RUNNING;
}

/**
 * @brief  100Hz System Manager task
 */
static void Sys100Hz_PRD()
{
    /**< Evaluate state of all systems */
}

/**
 * @brief  10 Hz System Manager task
 */
static void Sys10Hz_PRD()
{
    /**< 10Hz only toggles the LED in an error state */
    switch (SYS.state)
    {
      case SYS_RUNNING:
        break;
      case SYS_INIT:
        break;
      case SYS_ERROR:
        HW_GPIO_TogglePin(&led);
        break;
    }
}

/**
 * @brief  1 Hz System Manager task
 */
static void Sys1Hz_PRD()
{
    /**< 1Hz only toggles the LED in the running state */
    switch (SYS.state)
    {
      case SYS_RUNNING:
        HW_GPIO_TogglePin(&led);
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
const ModuleDesc_S Sys_desc = {
    .moduleInit        = &Sys_Init,
    .periodic100Hz_CLK = &Sys100Hz_PRD,
    .periodic10Hz_CLK  = &Sys10Hz_PRD,
    .periodic1Hz_CLK   = &Sys1Hz_PRD,
};

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/