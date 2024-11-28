/*
 * CANIO-rx.c
 * CAN Receive
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CAN/CAN.h"

#include "HW_can.h"

#include "FreeRTOS_SWI.h"
#include "ModuleDesc.h"
#include "string.h"
#include "Utility.h"
#include "uds.h"

#include "MessageUnpack_generated.h"
#include "FeatureDefines_generated.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

#if FEATURE_CANRX_SWI
extern CAN_HandleTypeDef hcan;

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    FLAG_create(fifoNotify, CAN_RX_FIFO_COUNT);
} canrx_S;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static canrx_S canrx;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * CANRX_BUS_A_SWI
 * SWI which is called by the FIFO receive interrupts
 * This function will drain the RX FIFO whenever it is called,
 * further calling the correct message processing function
 * for each received message
 */
void CANRX_BUS_VEH_SWI(void)
{
    // setInfoDot(INFO_DOT_CAN_RX);
    for (CAN_RxFifo_E rxFifo = CAN_RX_FIFO_0; rxFifo < CAN_RX_FIFO_COUNT; rxFifo++)
    {
        if (!FLAG_get(canrx.fifoNotify, rxFifo))
        {
            continue;
        }

        FLAG_clear(canrx.fifoNotify, (uint16_t)rxFifo);

        while (CAN_getRxFifoEmptyBus0(rxFifo) == false)
        {
            // toggleInfoDotState(INFO_DOT_CAN_RX);
            CAN_RxMessage_T msg       = { 0U };
            bool            rxSuccess = CAN_getRxMessageBus0(rxFifo, &msg);

            if (rxSuccess)
            {
#if APP_UDS
                if (msg.id == UDS_REQUEST_ID)
                {
                    // FIXME: there needs to be a queue here for received UDS messages
                    // which will then be processed in the periodic.
                    // Right now, a successfully received UDS message will overwritten by
                    // the next one, even if it hasn't been processed yet.
                    udsSrv_processMessage((uint8_t*)&msg.data, (uint8_t)msg.lengthBytes);
                }
                else
#endif // APP_UDS
                {
                    CANRX_VEH_unpackMessage((uint16_t)msg.id, &msg.data);
                }
            }
            else
            {
                // TODO: handle errors
            }
        }
        HW_CAN_activateFifoNotifications(rxFifo);
    }
}

/**
 * CANRX_BUS_A_notify
 * @brief Notification from HW layer that an RX FIFO has messages waiting
 * @param rxFifo which fifo notified
 */
void CANRX_BUS_VEH_notify(CAN_RxFifo_E rxFifo)
{
    FLAG_set(canrx.fifoNotify, rxFifo);
}
#endif // FEATURE_CANRX_SWI

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * CANIO_rx_init
 *
 */
static void CANIO_rx_init(void)
{
    // initialize module struct to 0
    CANRX_init();
    memset(&canrx, 0x00, sizeof(canrx));
}


const ModuleDesc_S CANIO_rx = {
    .moduleInit        = &CANIO_rx_init,
};
