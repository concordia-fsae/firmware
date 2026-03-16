/**
 * @file Module.c
 * @brief  Source code for Module Manager
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module Header */
#include "Module.h"

/**< System Includes*/
#include "SystemConfig.h"
#include "stddef.h"
#include "stdint.h"

// FreeRTOS Includes
#include "FreeRTOS.h"
#include "task.h"

/**< Other Includes */
#include "Utility.h"
#include "lib_utility.h"
#include "FeatureDefines_generated.h"
#include "lib_nvm.h"

#include "drv_userInput.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

// Required because BMSB Module hasnt been ported to the shared definition Modules
// so these calls cant be sequenced in the module pre and post functions
static ModuleDesc_S userInput_desc = {
    .moduleInit       = &drv_userInput_init,
    .periodic1kHz_CLK = &drv_userInput_run,
};

/**
 * @brief  Modules run by the Module Manager. Order will apply to execution.
 */
const ModuleDesc_S* modules[MODULE_CNT] = {
    &CANIO_rx,
#if APP_UDS
    &UDS_desc,
#endif
    &drv_inputAD_desc,
    &userInput_desc,
    &ENV_desc,
    &BMS_desc,
    &SYS_desc,
    &CANIO_tx,
};
