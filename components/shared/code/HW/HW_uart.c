/**
 * @file HW_uart.c
 * @brief  Source code for UART firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_uart.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

UART_HandleTypeDef huart[HW_UART_PORT_COUNT];

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

HW_StatusTypeDef_E HW_UART_startDMARX(HW_UART_port_E port, uint32_t* data, uint32_t size)
{
    return HAL_UART_Receive_DMA(&huart[port], (uint8_t*)data, (uint16_t)size) == HAL_OK ? HW_OK : HW_ERROR;
}

HW_StatusTypeDef_E HW_UART_stopDMA(HW_UART_port_E port)
{
    return HAL_UART_DMAStop(&huart[port]) == HAL_OK ? HW_OK : HW_ERROR;
}

HW_StatusTypeDef_E HW_UART_init(void)
{
    HW_StatusTypeDef_E status = HW_UART_init_componentSpecific();

    return status;
}

