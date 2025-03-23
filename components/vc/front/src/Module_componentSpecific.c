/**
 * @file Module.c
 * @brief  Source code for Module Manager
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module Header */
#include "Module.h"
#include "drv_tps20xx.h"

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

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void Module_componentSpecific_Init(void)
{
    // Initialize drivers prior to application runtime
    drv_tps20xx_init();

}

/**
 * #brief Run the pre 10Hz task functions
 * @note typically reserved for drivers
 */
void Module_componentSpecific_10Hz(void)
{
    drv_tps20xx_run();
}
