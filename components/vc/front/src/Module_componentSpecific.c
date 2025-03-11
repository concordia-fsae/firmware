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
const ModuleDesc_S* modules[] = {
    &IO_desc,
#if APP_UDS
    &UDS_desc,
#endif
    &CANIO_rx,
    &CANIO_tx,
};
