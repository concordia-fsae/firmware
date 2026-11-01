#pragma once
#include <stdint.h>
typedef enum { HW_UART_PORT_GPS, HW_UART_PORT_COUNT, } HW_UART_port_E;
typedef enum { HW_OK, HW_ERROR, }                      HW_StatusTypeDef_E;
#define HAL_UART_ERROR_ORE    8U
#define HAL_UART_ERROR_FE     4U
#define HAL_UART_ERROR_NE     2U
#define HAL_UART_ERROR_PE     1U
HW_StatusTypeDef_E HW_UART_startDMARX(HW_UART_port_E port, uint32_t *buffer, uint32_t size);
HW_StatusTypeDef_E HW_UART_stopDMA(HW_UART_port_E port);

uint32_t           HW_UART_getDmaRxRemaining(HW_UART_port_E port);

// Stop DMA and mask the UART interrupt source.
void               HW_UART_disable(HW_UART_port_E port);
