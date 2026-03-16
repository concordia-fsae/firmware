/**
 * @file Module.c
 * @brief  Source code for Module Manager
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module Header */
#include "Module.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

/**
 * @brief  Modules run by the Module Manager. Order will apply to execution.
 */
const ModuleDesc_S* modules[MODULE_CNT] = {
    &CANIO_rx,
#if FEATURE_IS_ENABLED(APP_UDS)
    &UDS_desc,
#endif // FEATURE_UDS
    &drv_inputAD_desc,
    &BMS_desc,
    &ENV_desc,
    &cooling_desc,
    &SYS_desc,
    &CANIO_tx,
};
