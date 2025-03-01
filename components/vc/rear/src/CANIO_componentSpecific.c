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
    switch (bus)
    {
        case CAN_BUS_ASS:
            CANRX_ASS_unpackMessage(id, data);
            break;
        case CAN_BUS_VEH:
            CANRX_VEH_unpackMessage(id, data);
            break;
        default:
            break;
    }
}
