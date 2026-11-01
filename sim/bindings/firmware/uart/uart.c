#include "uart.h"

#include "uart_backend.h"

HW_StatusTypeDef_E HW_UART_startDMARX(HW_UART_port_E port, uint32_t* data, uint32_t size)
{
    return rig_runtime_uart_start_rx((int32_t)port, (uint8_t*)data, size) ? HW_OK : HW_ERROR;
}

HW_StatusTypeDef_E HW_UART_stopDMA(HW_UART_port_E port)
{
    rig_runtime_uart_stop_rx((int32_t)port);
    return HW_OK;
}

uint32_t HW_UART_getDmaRxRemaining(HW_UART_port_E port)
{
    return rig_runtime_uart_rx_remaining((int32_t)port);
}
