#include "uart.h"

HW_StatusTypeDef_E HW_UART_startDMARX(HW_UART_port_E port, uint32_t* data, uint32_t size)
{
    (void)port;
    (void)data;
    (void)size;
    return HW_OK;
}

HW_StatusTypeDef_E HW_UART_stopDMA(HW_UART_port_E port)
{
    (void)port;
    return HW_OK;
}

// This binding has no incoming UART source yet; the producer stays at index zero.
uint32_t HW_UART_getDmaRxRemaining(HW_UART_port_E port)
{
    (void)port;
    return 0U;
}
