/**
 RX_config* CAN.h
 * Header file for CANRX configuration
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CAN/CANIO_componentSpecific.h"


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
