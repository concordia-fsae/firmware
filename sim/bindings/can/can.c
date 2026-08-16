#include "rig_runtime.h"

#include "runtime_state.h"

#include "CAN/CAN.h"
#include "HW.h"
#include "HW_can.h"

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
