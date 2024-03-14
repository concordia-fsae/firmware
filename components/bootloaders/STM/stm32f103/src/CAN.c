/*
 * CAN.c
 * This file describes higher level CAN behaviour
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// module include
#include "CAN.h"

#include "HW_NVIC.h"
#include "Utilities.h"


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

CAN_S CAN = { 0U };


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/


/******************************************************************************
 *                    C A L L B A C K   F U N C T I O N S
 ******************************************************************************/

void CAN_CB_txComplete(CAN_TxMailbox_E mb)
{
    CAN.bit.txInterrupt = 1;
    UNUSED(mb);
}

void CAN_CB_txAbort(CAN_TxMailbox_E mb)
{
    CAN.bit.txInterrupt = 1;
    UNUSED(mb);
}

void CAN_CB_txError(CAN_TxMailbox_E mb, uint32_t errorCode)
{
    CAN.bit.txInterrupt = 1;
    CAN.txBits          = errorCode;
    UNUSED(mb);
}

void CAN_CB_messageRx(CAN_RxFIFO_E fifo)
{
    switch (fifo)
    {
        case CAN_RX_FIFO_0:
            CAN.bit.rx0Interrupt = 1;
            CAN.bit.rxFifo0      = 1;
            break;

        case CAN_RX_FIFO_1:
            CAN.bit.rx1Interrupt = 1;
            CAN.bit.rxFifo1      = 1;
            break;

        default:
            break;
    }

    CAN_RxMessage_S msg = { 0U };
    if (CAN_getMessage(&msg))
    {
        CAN.rxMsg = msg;
    }
}

void CAN_CB_rxError(CAN_RxFIFO_E fifo, uint32_t errorCode)
{
    UNUSED(errorCode);

    CAN.bit.rx0Interrupt = false;
    CAN.bit.rx1Interrupt = false;
    CAN.bit.rxFifo0 = false;
    CAN.bit.rxFifo1 = false;

    CAN_rxErrorAck(fifo);
}

void CAN_CB_error(uint32_t errorCode)
{
    CAN.bit.sceInterrupt = 1;
    CAN.itBits           = errorCode;
}
