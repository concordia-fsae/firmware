#include "rig_runtime.h"

#include "runtime_state.h"

#include "CAN/CAN.h"
#include "HW.h"
#include "HW_can.h"
#include "HW_flash.h"

typedef enum
{
    RIG_HW_UART_PORT_UNUSED = 0U,
} HW_UART_port_E;

RTOS_swiHandle_T* SWI_create(RTOS_swiPri_E priority, RTOS_swiFn_t handler)
{
    if ((priority >= RTOS_SWI_PRI_COUNT) || (rig_runtime_swi_count[priority] >= RTOS_SWI_MAX_PER_PRI))
    {
        return NULL;
    }

    const uint8_t   index    = rig_runtime_swi_count[priority]++;
    RTOS_swiHandle_T* handle = &rig_runtime_swi_handles[priority][index];
    handle->handler  = handler;
    handle->priority = priority;
    handle->event    = 1UL << index;
    return handle;
}

void SWI_invoke(RTOS_swiHandle_T* handle)
{
    if ((handle != NULL) && (handle->handler != NULL))
    {
        handle->handler();
    }
}

bool SWI_invokeFromISR(RTOS_swiHandle_T* handle)
{
    SWI_invoke(handle);
    return true;
}

void SWI_disable(void)
{
}

void SWI_enable(void)
{
}

HW_StatusTypeDef_E HW_CAN_start(CAN_bus_E bus)
{
    (void)bus;
    return HW_OK;
}

HW_StatusTypeDef_E HW_CAN_stop(CAN_bus_E bus)
{
    (void)bus;
    return HW_OK;
}

HW_StatusTypeDef_E HW_CAN_sendMsgOnPeripheral(CAN_bus_E bus, CAN_TxMessage_T msg)
{
    rig_can_packet_S packet = {
        .id  = msg.id,
        .len = msg.lengthBytes,
    };

    packet.data[0] = msg.data.u8[0];
    packet.data[1] = msg.data.u8[1];
    packet.data[2] = msg.data.u8[2];
    packet.data[3] = msg.data.u8[3];
    packet.data[4] = msg.data.u8[4];
    packet.data[5] = msg.data.u8[5];
    packet.data[6] = msg.data.u8[6];
    packet.data[7] = msg.data.u8[7];
    return rig_runtime_can_push_tx((uint8_t)bus, &packet) ? HW_OK : HW_ERROR;
}

void HW_CAN_activateFifoNotifications(CAN_bus_E bus, CAN_RxFifo_E rxFifo)
{
    (void)bus;
    (void)rxFifo;
}

void rig_runtime_can_notify_rx(uint8_t bus)
{
#if FEATURE_IS_ENABLED(FEATURE_CANRX_SWI)
    CANRX_notify((CAN_bus_E)bus, CAN_RX_FIFO_0);
#else
    (void)bus;
#endif
}

bool HW_CAN_sendMsg(CAN_bus_E bus, CAN_data_T data, uint32_t id, uint8_t len)
{
    const CAN_TxMessage_T msg = {
        .id          = id,
        .IDE         = CAN_IDENTIFIER_STD,
        .RTR         = CAN_REMOTE_TRANSMISSION_REQUEST_DATA,
        .lengthBytes = len,
        .data        = data,
    };

    return HW_CAN_sendMsgOnPeripheral(bus, msg) == HW_OK;
}

bool HW_CAN_getRxMessage(CAN_bus_E bus, CAN_RxFifo_E rxFifo, CAN_RxMessage_T* rx)
{
    (void)rxFifo;
    rig_can_packet_S packet = { 0U };
    if ((rx == NULL) || !rig_runtime_can_pop_rx((uint8_t)bus, &packet))
    {
        return false;
    }

    rx->id               = packet.id;
    rx->IDE              = CAN_IDENTIFIER_STD;
    rx->RTR              = CAN_REMOTE_TRANSMISSION_REQUEST_DATA;
    rx->lengthBytes      = packet.len;
    rx->data.u8[0]       = packet.data[0];
    rx->data.u8[1]       = packet.data[1];
    rx->data.u8[2]       = packet.data[2];
    rx->data.u8[3]       = packet.data[3];
    rx->data.u8[4]       = packet.data[4];
    rx->data.u8[5]       = packet.data[5];
    rx->data.u8[6]       = packet.data[6];
    rx->data.u8[7]       = packet.data[7];
    rx->timestamp        = (uint16_t)(rig_runtime.time_ns / 1000000ULL);
    rx->filterMatchIndex = 0U;
    return true;
}

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

uint32_t FLASH_getPageSize(void)
{
    return 1024U;
}

bool FLASH_erasePages(uint32_t pageAddr, uint16_t pages)
{
    (void)pageAddr;
    (void)pages;
    return true;
}

bool FLASH_writeHalfwords(uint32_t addr, uint16_t* data, uint16_t dataLen)
{
    (void)addr;
    (void)data;
    (void)dataLen;
    return true;
}

bool HW_mcuShuttingDown(void)
{
    return false;
}

void HW_systemHardReset(void)
{
}
