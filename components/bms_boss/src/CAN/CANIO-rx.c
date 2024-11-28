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
#include "uds.h"

#include "MessageUnpack_generated.h"
#include "FeatureDefines_generated.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define UDS_BUFFER_LENGTH 8U

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern CAN_HandleTypeDef hcan;

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
#if FEATURE_IS_ENABLED(FEATURE_CANRX_SWI)
    FLAG_create(fifoNotify[CAN_BUS_COUNT], CAN_RX_FIFO_COUNT);
    CAN_RxMessage_T udsBuffer[UDS_BUFFER_LENGTH];
    uint8_t bufferStartIndex;
    uint8_t bufferEndIndex;
#endif // FEATURE_CANRX_SWI
} canrx_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static canrx_S canrx;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void CANIO_rx_addMsgToBuffer(CAN_RxMessage_T* const msg)
{
    canrx.udsBuffer[canrx.bufferEndIndex++] = *msg;

    if (canrx.bufferEndIndex == UDS_BUFFER_LENGTH)
    {
        canrx.bufferEndIndex = 0U;
    }
}

static bool CANIO_rx_isBufferEmpty(void)
{
    return canrx.bufferEndIndex == canrx.bufferStartIndex;
}

/**
 * CANIO_rx_1kHz_PRD
 *
 */
static void CANIO_rx_1kHz_PRD(void)
{
    if (CANIO_rx_isBufferEmpty() == false)
    {
        const udsResult_E result = udsSrv_processMessage((uint8_t*)&canrx.udsBuffer[canrx.bufferStartIndex].data, (uint8_t)canrx.udsBuffer[canrx.bufferStartIndex].lengthBytes);

        if (result == UDS_RESULT_SUCCESS)
        {
            if (++canrx.bufferStartIndex == UDS_BUFFER_LENGTH)
            {
                canrx.bufferStartIndex = 0U;
            }
        }
    }

#if FEATURE_IS_ENABLED(FEATURE_CANRX_SWI)
    if (FLAG_any((uint16_t*)&canrx.fifoNotify[CAN_BUS_VEH], CAN_RX_FIFO_COUNT))
    {
        SWI_invoke(CANRX_BUS_VEH_swi);
    }
#endif // FEATURE_CANRX_SWI
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
    CANRX_init();
}


const ModuleDesc_S CANIO_rx = {
    .moduleInit        = &CANIO_rx_init,
    .periodic1kHz_CLK  = &CANIO_rx_1kHz_PRD,
    .periodic100Hz_CLK = &CANIO_rx_100Hz_PRD,
    .periodic10Hz_CLK  = &CANIO_rx_10Hz_PRD,
};

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

#if FEATURE_CANRX_SWI
/**
 * CANRX_BUS_A_SWI
 * SWI which is called by the FIFO receive interrupts
 * This function will drain the RX FIFO whenever it is called,
 * further calling the correct message processing function
 * for each received message
 */
void CANRX_BUS_VEH_SWI(void)
{
    CAN_bus_E bus = CAN_BUS_VEH;

    for (CAN_RxFifo_E rxFifo = CAN_RX_FIFO_0; rxFifo < CAN_RX_FIFO_COUNT; rxFifo++)
    {
        if (!FLAG_get(canrx.fifoNotify[bus], rxFifo))
        {
            continue;
        }

        while (CAN_getRxFifoEmptyBus0(rxFifo) == false)
        {
            CAN_RxMessage_T msg       = { 0U };
            bool            rxSuccess = CAN_getRxMessageBus0(rxFifo, &msg);

            if (rxSuccess)
            {
#if APP_UDS
                if (msg.id == UDS_REQUEST_ID)
                {
                    udsResult_E result = udsSrv_processMessage((uint8_t*)&msg.data, (uint8_t)msg.lengthBytes);
                    switch (result)
                    {
                        case UDS_RESULT_NOT_READY:
                            CANIO_rx_addMsgToBuffer(&msg);
                            break;

                        default:
                            break;
                    }
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

        FLAG_clear(canrx.fifoNotify[bus], rxFifo);
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
    FLAG_set(canrx.fifoNotify[CAN_BUS_VEH], rxFifo);
}
#endif // FEATURE_CANRX_SWI
