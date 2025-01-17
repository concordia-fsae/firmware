/**
 * @file HW_can.c
 * @brief  Source code for CAN firmware
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW.h"
#include "HW_can.h"

#include "CAN/CAN.h"
#include "CAN/CanTypes.h"
#include "uds.h"
#include "uds_componentSpecific.h"

#include "NetworkDefines_generated.h"
#include "MessageUnpack_generated.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

// which CAN interrupts we want to enable
// stm32f1xx_hal_can.h from ST drivers for description of each one
// Description of interrupts:
// CAN_IER_TMEIE  Transmit Mailbox Empty
// CAN_IER_FMPIEx FIFO x Message Pending (RX FIFO)
// CAN_IER_FFIEx  FIFO x Full
// CAN_IER_FOVIEx FIFO x Overrun
// CAN_IER_WKUIE  Wakeup Interrupt
// CAN_IER_SLKIE  Sleep Interrupt
// CAN_IER_EWGIE  Error Warning Interrupt
// CAN_IER_EPVIE  Error Passive Interrupt
// CAN_IER_BOFIE  Bus Off Interrupt
// CAN_IER_LECIE  Last Error Code Interrupt
// CAN_IER_ERRIE  Error Interrupt
#define CAN_ENABLED_INTERRUPTS (CAN_IER_TMEIE | CAN_IER_FMPIE0 | CAN_IER_FMPIE1 | CAN_IER_FFIE0 | \
                                CAN_IER_FFIE1 | CAN_IER_FOVIE0 | CAN_IER_FOVIE1 | CAN_IER_EWGIE | \
                                CAN_IER_EPVIE | CAN_IER_BOFIE | CAN_IER_LECIE | CAN_IER_ERRIE)

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern CAN_HandleTypeDef hcan[CAN_BUS_COUNT];

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * HW_CAN_checkMbFree
 * @param canHandle which CAN handle to check
 * @param mailbox which mailbox to check
 * @return true if free
 */
static bool HW_CAN_checkMbFree(CAN_HandleTypeDef* canHandle, CAN_TxMailbox_E mailbox)
{
    uint32_t tsr = READ_REG(canHandle->Instance->TSR);

    return (tsr & (CAN_TSR_TME0 << mailbox));
}

/**
 * HW_CAN_sendMsg
 * @param canHandle which CAN handle to operate on
 * @param msg message data
 * @return exit code
 */
static HAL_StatusTypeDef HW_CAN_sendMsgOnPeripheral(CAN_HandleTypeDef* canHandle, CAN_TxMessage_T msg)
{
    HAL_CAN_StateTypeDef state = canHandle->State;
    HAL_StatusTypeDef ret = HAL_ERROR;

    if ((state == HAL_CAN_STATE_READY) || (state == HAL_CAN_STATE_LISTENING))
    {
        bool no_mailbox_empty = true;
        // check that a mailbox is free
        for (CAN_TxMailbox_E mailbox = 0U; mailbox < CAN_TX_MAILBOX_COUNT; mailbox++)
        {
            if (HW_CAN_checkMbFree(canHandle, mailbox))
            {
                // set CAN ID
                canHandle->Instance->sTxMailBox[mailbox].TIR  = ((msg.IDE == CAN_IDENTIFIER_STD) ?
                                                                 msg.id << CAN_TI0R_STID_Pos :
                                                                 msg.id << CAN_TI0R_EXID_Pos) |
                                                                ((msg.IDE == CAN_IDENTIFIER_EXT) ? 0x01 << 2U : 0x00);
                // set message length
                canHandle->Instance->sTxMailBox[mailbox].TDTR = msg.lengthBytes;

                // set message data
                canHandle->Instance->sTxMailBox[mailbox].TDHR = msg.data.u32[1];
                canHandle->Instance->sTxMailBox[mailbox].TDLR = msg.data.u32[0];

                // request message transmission
                SET_BIT(canHandle->Instance->sTxMailBox[mailbox].TIR, CAN_TI0R_TXRQ);

                // Return function status
                ret = HAL_OK;
                no_mailbox_empty = false;
            }
        }
        if (no_mailbox_empty)
        {
            // update error to show that no mailbox was free
            canHandle->ErrorCode |= HAL_CAN_ERROR_PARAM;
        }
    }
    else
    {
        // update error to show that peripheral was in the wrong state for transmission
        canHandle->ErrorCode |= HAL_CAN_ERROR_NOT_INITIALIZED;
    }

    return ret;
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Deinitializes the CAN peripheral
 * @retval HW_OK
 */
 HW_StatusTypeDef_E HW_CAN_deInit(void)
{
    for (CAN_bus_E bus = 0U; bus < CAN_BUS_COUNT; bus++)
    {
        HAL_CAN_DeactivateNotification(&hcan[bus], CAN_ENABLED_INTERRUPTS);
        HAL_CAN_DeInit(&hcan[bus]);
    }

    return HW_OK;
}

/**
 * CAN_Start
 * Start the CAN module(s)
 */
HW_StatusTypeDef_E HW_CAN_start(CAN_bus_E bus)
{
    HAL_CAN_Start(&hcan[bus]);    // start CAN

    return HW_OK;
}

/**
 * @brief Stops the CAN module(s)
 *
 * @retval HW_OK
 */
HW_StatusTypeDef_E HW_CAN_stop(CAN_bus_E bus)
{
    HAL_CAN_Stop(&hcan[bus]);

    return HW_OK;
}

void HW_CAN_activateFifoNotifications(CAN_bus_E bus, CAN_RxFifo_E rxFifo)
{
    uint32_t it = rxFifo == CAN_RX_FIFO_0 ? CAN_IER_FMPIE0 : CAN_IER_FMPIE1;
    uint32_t itFull = rxFifo == CAN_RX_FIFO_0 ? CAN_IER_FFIE0 : CAN_IER_FFIE1;
    HAL_CAN_ActivateNotification(&hcan[bus], it);
    HAL_CAN_ActivateNotification(&hcan[bus], itFull);
}

/**
 * HW_CAN_sendMsgBus0
 * @param priority TODO
 * @param data TODO
 * @param id TODO
 * @param len TODO
 * @return TODO
 */
bool HW_CAN_sendMsg(CAN_bus_E bus, CAN_data_T data, uint32_t id, uint8_t len)
{
    UNUSED(bus);
    CAN_TxMessage_T msg = {0};

    msg.id          = id;
    msg.data        = data;
    msg.lengthBytes = len;
    msg.IDE         = (id <= 0x7ff) ? CAN_IDENTIFIER_STD : CAN_IDENTIFIER_EXT;

    return HW_CAN_sendMsgOnPeripheral(&hcan[bus], msg) == HAL_OK;
}


/**
 * HW_CAN_getRxMessageBus0
 * @brief  Get an CAN frame from the Rx FIFO zone into the message RAM.
 * @param  rxFifo Fifo number of the received message to be read.
 *         This parameter can be a value of @arg CAN_receive_FIFO_number.
 * @param  rx pointer to a CAN_RxMessage_T where the message will be stored
 * @retval HAL status
 */
bool HW_CAN_getRxMessage(CAN_bus_E bus, CAN_RxFifo_E rxFifo, CAN_RxMessage_T* rx)
{
    if ((hcan[bus].State != HAL_CAN_STATE_READY) && (hcan[bus].State != HAL_CAN_STATE_LISTENING))
    {
        // Update error code
        hcan[bus].ErrorCode |= HAL_CAN_ERROR_NOT_INITIALIZED;

        return false;
    }

    switch (rxFifo)
    {
        case CAN_RX_FIFO0:
            // Check that the Rx FIFO 0 is not empty
            if ((hcan[bus].Instance->RF0R & CAN_RF0R_FMP0) == 0U)
            {
                // Update error code
                hcan[bus].ErrorCode |= HAL_CAN_ERROR_PARAM;

                return false;
            }
            break;

        case CAN_RX_FIFO1:
            // Check that the Rx FIFO 1 is not empty
            if ((hcan[bus].Instance->RF1R & CAN_RF1R_FMP1) == 0U)
            {
                // Update error code
                hcan[bus].ErrorCode |= HAL_CAN_ERROR_PARAM;

                return false;
            }
            break;

        default:
            // should never reach this state
            return false;
            break;
    }

    // Get the header
    rx->IDE = (CAN_IdentifierLen_E)(CAN_RI0R_IDE & hcan[bus].Instance->sFIFOMailBox[rxFifo].RIR);
    rx->RTR = (CAN_RemoteTransmission_E)(CAN_RI0R_RTR & hcan[bus].Instance->sFIFOMailBox[rxFifo].RIR);

    rx->id = (((CAN_RI0R_EXID | CAN_RI0R_STID) & hcan[bus].Instance->sFIFOMailBox[rxFifo].RIR) >> (rx->IDE == CAN_IDENTIFIER_STD ? CAN_RI0R_STID_Pos : CAN_RI0R_EXID_Pos));
    rx->lengthBytes = (CAN_RDT0R_DLC & hcan[bus].Instance->sFIFOMailBox[rxFifo].RDTR) >> CAN_RDT0R_DLC_Pos;

    rx->timestamp        = (CAN_RDT0R_TIME & hcan[bus].Instance->sFIFOMailBox[rxFifo].RDTR) >> CAN_RDT0R_TIME_Pos;
    rx->filterMatchIndex = (CAN_RDT0R_FMI & hcan[bus].Instance->sFIFOMailBox[rxFifo].RDTR) >> CAN_RDT0R_FMI_Pos;

    // Get the data
    rx->data.u32[0U] = hcan[bus].Instance->sFIFOMailBox[rxFifo].RDLR;
    rx->data.u32[1U] = hcan[bus].Instance->sFIFOMailBox[rxFifo].RDHR;

    // Release the FIFO
    switch (rxFifo)
    {
        case CAN_RX_FIFO0:
            SET_BIT(hcan[bus].Instance->RF0R, CAN_RF0R_RFOM0);
            /* HAL_CAN_ActivateNotification(&hcan, CAN_IER_FMPIE0); */
            break;

        case CAN_RX_FIFO1:
            SET_BIT(hcan[bus].Instance->RF1R, CAN_RF1R_RFOM1);
            /* HAL_CAN_ActivateNotification(&hcan, CAN_IER_FMPIE1); */
            break;

        default:
            // should never get here
            break;
    }

    // Return function status
    return true;
}


// define interrupt handlers

/**
 * HW_CAN_TxComplete_ISR
 * This ISR is called whenever a CAN mailbox is free. It should then call the tx thread
 * associated with the mailbox that became free
 * @param mailbox CAN mailbox which has become free
 */
void HW_CAN_TxComplete_ISR(CAN_bus_E bus, CAN_TxMailbox_E mailbox)
{
    UNUSED(bus);
    UNUSED(mailbox);
}

/**
 * HW_CAN_RxMsgPending_ISR
 * This ISR is called whenever a CAN message is received. It should then move
 * the RXd message into the RX queue to free up the rxFifo
 * @param bus CANBus with a pending message
 * @param fifoId RX Fifo with a pending message
 */
void HW_CAN_RxMsgPending_ISR(CAN_bus_E bus, CAN_RxFifo_E fifoId)
{
#if FEATURE_IS_DISABLED(FEATURE_CANRX_SWI)
    CAN_data_T          data = {0U};
    CAN_RxHeaderTypeDef header = {0U};
#endif // FEATURE_CANRX_SWI == FEATURE_DISABLED

    if (CAN_BUS_VEH)
    {
#if FEATURE_CANRX_SWI == FEATURE_DISABLED
        HAL_CAN_GetRxMessage(&hcan[bus], fifoId, &header, (uint8_t*)&data);
        CANRX_VEH_unpackMessage(header.StdId, &data);

#if APP_UDS
        if (header.StdId == UDS_REQUEST_ID)
        {
            // FIXME: there needs to be a queue here for received UDS messages
            // which will then be processed in the periodic.
            // Right now, a successfully received UDS message will overwritten by
            // the next one, even if it hasn't been processed yet.
            udsSrv_processMessage(data.u8, (uint8_t)header.DLC);
        }
#endif // FEATURE_UDS
#else // FEATURE_CANRX_SWI == FEATURE_DISABLED
        CANRX_notify(bus, fifoId);
#endif // FEATURE_CANRX_SWI
    }
}

/**
 * HW_CAN_TxError_ISR
 * @param bus which can handle to operate on
 * @param mailbox which can mailbox to operate on
 */
void HW_CAN_TxError_ISR(CAN_bus_E bus, CAN_TxMailbox_E mailbox)
{
    UNUSED(bus);
    UNUSED(mailbox);
}
