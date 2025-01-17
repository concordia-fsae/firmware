/**
 RX_config* CAN.h
 * Header file for CANRX configuration
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CAN/CAN.h"
#include "CANIO_componentSpecific.h"
#include "Utility.h"
#include "MessageUnpack_generated.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void CANRX_unpackMessage(CAN_bus_E bus, uint32_t id, CAN_data_T *data)
{
#if BMSB_CONFIG_ID == 0U
    for (uint8_t i = 0U; i < COUNTOF(CANRX_PRIVBMS_unpackList); i++)
    {
        if (id == CANRX_PRIVBMS_unpackList[i])
        {
            CANRX_PRIVBMS_unpackMessage(id, data);
            return;
        }
    }
    for (uint8_t i = 0U; i < COUNTOF(CANRX_PRIVBMS_unpackListExtID); i++)
    {
        if (id == CANRX_PRIVBMS_unpackListExtID[i])
        {
            CANRX_PRIVBMS_unpackMessage(id, data);
            return;
        }
    }
#endif

    switch (bus)
    {
#if BMSB_CONFIG_ID > 0U
        case CAN_BUS_PRIVBMS:
            CANRX_PRIVBMS_unpackMessage(id, data);
            break;
#endif
        case CAN_BUS_VEH:
            CANRX_VEH_unpackMessage(id, data);
            break;
        default:
            break;
    }
}

uint8_t CANIO_tx_getNLG513ControlByte(void)
{
    uint8_t ret = 0x00;
    switch (SYS.contacts)
    {
        case SYS_CONTACTORS_CLOSED:
            ret = 0x40;
            break;

        case SYS_CONTACTORS_HVP_CLOSED:
            ret = 0x80;
            break;

        default:
            ret = 0x00;
            break;
    }

    return ret;
}

CAN_prechargeContactorState_E CANIO_tx_getContactorState(void)
{
    CAN_prechargeContactorState_E ret = CAN_PRECHARGECONTACTORSTATE_CONTACTORS_SNA;

    switch (SYS.contacts)
    {
        case SYS_CONTACTORS_OPEN:
            ret = CAN_PRECHARGECONTACTORSTATE_CONTACTORS_OPEN;
            break;

        case SYS_CONTACTORS_PRECHARGE:
            ret = CAN_PRECHARGECONTACTORSTATE_CONTACTORS_PRECHARGE_CLOSED;
            break;

        case SYS_CONTACTORS_CLOSED:
            ret = CAN_PRECHARGECONTACTORSTATE_CONTACTORS_PRECHARGE_HVP_CLOSED;
            break;

        case SYS_CONTACTORS_HVP_CLOSED:
            ret = CAN_PRECHARGECONTACTORSTATE_CONTACTORS_HVP_CLOSED;
            break;
    }

    return ret;
}
