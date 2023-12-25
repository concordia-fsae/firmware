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
#include "Utility.h"
#include "string.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

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
void CANRX_BUS_A_SWI(void)
{
    // setInfoDot(INFO_DOT_CAN_RX);
    for (CAN_RxFifo_E rxFifo = CAN_RX_FIFO_0; rxFifo < CAN_RX_FIFO_COUNT; rxFifo++)
    {
        if (!FLAG_get(canrx.fifoNotify, rxFifo))
        {
            continue;
        }

        FLAG_clear(canrx.fifoNotify, rxFifo);

        while (CAN_getRxFifoEmptyBus0(rxFifo) == false)
        {
            // toggleInfoDotState(INFO_DOT_CAN_RX);
            CAN_RxMessage_T msg       = { 0U };
            bool            rxSuccess = CAN_getRxMessageBus0(rxFifo, &msg);

            if (rxSuccess)
            {
                // toggleInfoDotState(INFO_DOT_CAN_RX);
            }
            else
            {
                // TODO: handle errors
            }
        }

        // FIXME: notification should be reactivated in the hardware layer
        uint32_t it = rxFifo == CAN_RX_FIFO_0 ? CAN_IER_FMPIE0 : CAN_IER_FMPIE1;
        HAL_CAN_ActivateNotification(&hcan, it);
    }
}

/**
 * CANRX_BUS_A_notify
 * @brief Notification from HW layer that an RX FIFO has messages waiting
 * @param rxFifo which fifo notified
 */
void CANRX_BUS_A_notify(CAN_RxFifo_E rxFifo)
{
    FLAG_set(canrx.fifoNotify, rxFifo);
}

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * CANIO_rx_1kHz_PRD
 *
 */
static void CANIO_rx_1kHz_PRD(void)
{
    // TODO Implement this
}


/**
 * CANIO_rx_100Hz_PRD
 *
 */
static void CANIO_rx_100Hz_PRD(void)
{
    // TODO Implement this
}


/**
 * CANIO_rx_10Hz_PRD
 *
 */
static void CANIO_rx_10Hz_PRD(void)
{
    // TODO Implement this
}


/**
 * CANIO_rx_init
 *
 */
static void CANIO_rx_init(void)
{
    // initialize module struct to 0
    memset(&canrx, 0x00, sizeof(canrx));
}


const ModuleDesc_S CANIO_rx = {
    .moduleInit        = &CANIO_rx_init,
    .periodic1kHz_CLK  = &CANIO_rx_1kHz_PRD,
    .periodic100Hz_CLK = &CANIO_rx_100Hz_PRD,
    .periodic10Hz_CLK  = &CANIO_rx_10Hz_PRD,
};

