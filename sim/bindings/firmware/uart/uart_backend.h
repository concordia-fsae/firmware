#pragma once

#include <stdbool.h>
#include <stdint.h>

// Simulation receive configuration. Set before firmware initialization; reset
// invalidates configuration and all firmware-owned DMA buffer registrations.
typedef enum
{
    RIG_UART_RX_CIRCULAR_DMA    = 0,
    RIG_UART_RX_SINGLE_SHOT_DMA = 1,
    RIG_UART_RX_POLLING         = 2,
} RigUartRxMode_E;

typedef struct
{
    uint64_t accepted;
    uint64_t dropped;
    uint32_t remaining;
    uint32_t queued;
    uint32_t active;
    uint32_t completed;
} RigUartRxStatus_S;

bool     rig_runtime_uart_configure_rx(int32_t port, uint32_t mode, uint32_t capacity);
bool     rig_runtime_uart_start_rx(int32_t port, uint8_t* data, uint32_t size);
void     rig_runtime_uart_stop_rx(int32_t port);
uint32_t rig_runtime_uart_rx_remaining(int32_t port);
// Nonblocking polling read: count read, or -1 for invalid/wrong-mode port.
int64_t  rig_runtime_uart_read_rx(int32_t port, uint8_t* output, uint32_t capacity);
// Inject between scheduler steps: count accepted, or -1 for invalid arguments.
int64_t  rig_uart_receive(int32_t port, const uint8_t* data, uint32_t size);
bool     rig_uart_rx_status(int32_t port, RigUartRxStatus_S* status);
void     rig_runtime_uart_reset(void);
