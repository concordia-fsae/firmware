/**
 * @file drv_io.h
 * @brief  Header file for the generic components of the input and output driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_io.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

drv_io_logicLevel_E drv_io_invertLogicLevel(drv_io_logicLevel_E level)
{
    return (level == DRV_IO_LOGIC_HIGH) ? DRV_IO_LOGIC_LOW : DRV_IO_LOGIC_HIGH;
}
