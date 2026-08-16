#pragma once

#include "HW.h"
#include "LIB_Types.h"

// The production UART port enum is component-specific.  The simulation
// binding intentionally exposes only the ABI it implements; all ports are
// accepted and ignored by this no-op backend.
typedef enum
{
    RIG_HW_UART_PORT_UNUSED = 0U,
} HW_UART_port_E;

HW_StatusTypeDef_E HW_UART_startDMARX(HW_UART_port_E port, uint32_t* data, uint32_t size);
HW_StatusTypeDef_E HW_UART_stopDMA(HW_UART_port_E port);
