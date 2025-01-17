/**
 * @file HW_can.c
 * @brief  Source code for CAN firmware
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_can.h"

#include "BatteryMonitoring.h"
#include "CAN/CanTypes.h"
#include "CAN/CAN.h"
#include "stm32f103xb.h"
#include "stm32f1xx_hal_can.h"

#include "MessageUnpack_generated.h"
#include "uds.h"
#include "uds_componentSpecific.h"
#include "Cooling.h"

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
#define CAN_ENABLED_INTERRUPTS    (CAN_IER_TMEIE | CAN_IER_FMPIE0 | CAN_IER_FMPIE1 | CAN_IER_FFIE0 | \
                                   CAN_IER_FFIE1 | CAN_IER_FOVIE0 | CAN_IER_FOVIE1 | CAN_IER_EWGIE | \
                                   CAN_IER_EPVIE | CAN_IER_BOFIE | CAN_IER_LECIE | CAN_IER_ERRIE)

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

CAN_HandleTypeDef hcan;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/


/**
 * @brief Starts the CAN module(s)
 *
 * @retval HW_OK
 */
HW_StatusTypeDef_E HW_CAN_start(void)
{
    HAL_CAN_Start(&hcan);    // start CAN

    return HW_OK;
}


/**
 * @brief Stops the CAN module(s)
 *
 * @retval HW_OK
 */
HW_StatusTypeDef_E HW_CAN_stop(void)
{
    HAL_CAN_Stop(&hcan);

    return HW_OK;
}


/**
 * @brief Initializes the CAN peripheral
 *
 * @retval HW_OK
 */
HW_StatusTypeDef_E HW_CAN_init(void)
{
    hcan.Instance                  = CAN1;
    hcan.Init.Prescaler            = 4;
    hcan.Init.Mode                 = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth        = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1             = CAN_BS1_6TQ;
    hcan.Init.TimeSeg2             = CAN_BS2_1TQ;
    hcan.Init.TimeTriggeredMode    = DISABLE;
    hcan.Init.AutoBusOff           = ENABLE;
    hcan.Init.AutoWakeUp           = DISABLE;
    hcan.Init.AutoRetransmission   = ENABLE;
    hcan.Init.ReceiveFifoLocked    = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE;

    if (HAL_CAN_Init(&hcan) != HAL_OK)
    {
        Error_Handler();
    }

    _Static_assert(CANRX_VEH_unpackList_length <= 32, "STM32F1 only has 32 filter id's when used in list mode");
    for (uint32_t i = 0; i < CANRX_VEH_unpackList_length; )
    {
        CAN_FilterTypeDef filt = { 0U };
        filt.FilterBank           = i / 4;
        filt.FilterMode           = CAN_FILTERMODE_IDLIST;
        filt.FilterScale          = CAN_FILTERSCALE_16BIT;
        // All filters are shifted left 5 bits
        filt.FilterIdHigh = CANRX_VEH_unpackList[i + 0] << 5;
        if ((i + 1) < CANRX_VEH_unpackList_length) { filt.FilterIdLow = CANRX_VEH_unpackList[i + 1] << 5; }
        if ((i + 2) < CANRX_VEH_unpackList_length) { filt.FilterMaskIdHigh = CANRX_VEH_unpackList[i + 2] << 5; }
        if ((i + 3) < CANRX_VEH_unpackList_length) { filt.FilterMaskIdLow = CANRX_VEH_unpackList[i + 3] << 5; }
        filt.FilterFIFOAssignment = i % CAN_RX_FIFO_COUNT;
        filt.FilterActivation     = ENABLE;
        HAL_CAN_ConfigFilter(&hcan, &filt);

        i += 4;
    }
    // activate selected CAN interrupts
    HAL_CAN_ActivateNotification(&hcan, CAN_ENABLED_INTERRUPTS);

    return HW_OK;
}

void HW_CAN_activateFifoNotifications(CAN_bus_E bus, CAN_RxFifo_E rxFifo)
{
    UNUSED(bus);
    uint32_t it = rxFifo == CAN_RX_FIFO_0 ? CAN_IER_FMPIE0 : CAN_IER_FMPIE1;
    uint32_t itFull = rxFifo == CAN_RX_FIFO_0 ? CAN_IER_FFIE0 : CAN_IER_FFIE1;
    HAL_CAN_ActivateNotification(&hcan, it);
    HAL_CAN_ActivateNotification(&hcan, itFull);
}

/**
 * @brief Deinitializes the CAN peripheral
 *
 * @retval HW_OK
 */
 HW_StatusTypeDef_E HW_CAN_deInit(void)
{
    HAL_CAN_DeactivateNotification(&hcan, CAN_ENABLED_INTERRUPTS);
    HAL_CAN_DeInit(&hcan);

    return HW_OK;
}

/**
 * HW_CAN_checkMbFree
 * @param canHandle which CAN handle to check
 * @param mailbox which mailbox to check
 * @return true if free
 */
static bool HW_CAN_checkMbFree(CAN_HandleTypeDef* canHandle, CAN_TxMailbox_E mailbox)
{
    uint32_t tsr = READ_REG(canHandle->Instance->TSR);

    return(tsr & (CAN_TSR_TME0 << mailbox));
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
        // check that the specified mailbox is free
        if (HW_CAN_checkMbFree(canHandle, msg.mailbox))
        {
            // set CAN ID
            canHandle->Instance->sTxMailBox[msg.mailbox].TIR  = ((msg.id << CAN_TI0R_STID_Pos) | msg.RTR);
            // set message length
            canHandle->Instance->sTxMailBox[msg.mailbox].TDTR = msg.lengthBytes;

            // set message data
            canHandle->Instance->sTxMailBox[msg.mailbox].TDHR = msg.data.u32[1];
            canHandle->Instance->sTxMailBox[msg.mailbox].TDLR = msg.data.u32[0];

            // request message transmission
            SET_BIT(canHandle->Instance->sTxMailBox[msg.mailbox].TIR, CAN_TI0R_TXRQ);

            // Return function status
            ret = HAL_OK;
        }
        else
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


/**
 * HW_CAN_sendMsgBus0
 * @param priority TODO
 * @param data TODO
 * @param id TODO
 * @param len TODO
 * @return TODO
 */
bool HW_CAN_sendMsg(CAN_bus_E bus, CAN_TxMailbox_E mailbox, CAN_data_T data, uint32_t id, uint8_t len)
{
    UNUSED(bus);
    CAN_TxMessage_T msg = {0};

    msg.id          = id;
    msg.data        = data;
    msg.mailbox     = (CAN_TxMailbox_E)mailbox;
    msg.lengthBytes = len;

    return HW_CAN_sendMsgOnPeripheral(&hcan, msg) == HAL_OK;
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
    UNUSED(bus);
    if ((hcan.State != HAL_CAN_STATE_READY) && (hcan.State != HAL_CAN_STATE_LISTENING))
    {
        // Update error code
        hcan.ErrorCode |= HAL_CAN_ERROR_NOT_INITIALIZED;

        return false;
    }

    switch (rxFifo)
    {
        case CAN_RX_FIFO0:
            // Check that the Rx FIFO 0 is not empty
            if ((hcan.Instance->RF0R & CAN_RF0R_FMP0) == 0U)
            {
                // Update error code
                hcan.ErrorCode |= HAL_CAN_ERROR_PARAM;

                return false;
            }
            break;

        case CAN_RX_FIFO1:
            // Check that the Rx FIFO 1 is not empty
            if ((hcan.Instance->RF1R & CAN_RF1R_FMP1) == 0U)
            {
                // Update error code
                hcan.ErrorCode |= HAL_CAN_ERROR_PARAM;

                return false;
            }
            break;

        default:
            // should never reach this state
            return false;

            break;
    }

    // Get the header
    rx->IDE              = (CAN_IdentifierLen_E)(CAN_RI0R_IDE & hcan.Instance->sFIFOMailBox[rxFifo].RIR);
    rx->RTR              = (CAN_RemoteTransmission_E)(CAN_RI0R_RTR & hcan.Instance->sFIFOMailBox[rxFifo].RIR);

    rx->id = (uint32_t)(((CAN_RI0R_EXID | CAN_RI0R_STID) & hcan.Instance->sFIFOMailBox[rxFifo].RIR) >> (rx->IDE == CAN_IDENTIFIER_STD ? CAN_RI0R_STID_Pos : CAN_RI0R_EXID_Pos));
    rx->lengthBytes      = (CAN_RDT0R_DLC & hcan.Instance->sFIFOMailBox[rxFifo].RDTR) >> CAN_RDT0R_DLC_Pos;

    rx->timestamp        = (CAN_RDT0R_TIME & hcan.Instance->sFIFOMailBox[rxFifo].RDTR) >> CAN_RDT0R_TIME_Pos;
    rx->filterMatchIndex = (CAN_RDT0R_FMI & hcan.Instance->sFIFOMailBox[rxFifo].RDTR) >> CAN_RDT0R_FMI_Pos;

    // Get the data
    rx->data.u32[0U] = hcan.Instance->sFIFOMailBox[rxFifo].RDLR;
    rx->data.u32[1U] = hcan.Instance->sFIFOMailBox[rxFifo].RDHR;

    // Release the FIFO
    switch (rxFifo)
    {
        case CAN_RX_FIFO0:
            SET_BIT(hcan.Instance->RF0R, CAN_RF0R_RFOM0);
            // HAL_CAN_ActivateNotification(&hcan, CAN_IER_FMPIE0);
            break;

        case CAN_RX_FIFO1:
            SET_BIT(hcan.Instance->RF1R, CAN_RF1R_RFOM1);
            // HAL_CAN_ActivateNotification(&hcan, CAN_IER_FMPIE1);
            break;

        default:
            // should never get here
            break;
    }

    // Return function status
    return true;
}


/**
 * HW_CAN_getRxFifoFillLevelBus0
 * @brief  Return Bus0 Rx FIFO fill level.
 * @param  rxFifo Rx FIFO.
 *         This parameter can be a value of @arg CAN_receive_FIFO_number.
 * @retval Number of messages available in Rx FIFO.
 */
uint8_t HW_CAN_getRxFifoFillLevel(CAN_bus_E bus, CAN_RxFifo_E rxFifo)
{
    UNUSED(bus);
    if ((hcan.State != HAL_CAN_STATE_READY) && (hcan.State != HAL_CAN_STATE_LISTENING))
    {
        // CAN peripheral is not ready
        return 0U;
    }

    uint8_t fillLevel = 0U;
    switch (rxFifo)
    {
        case CAN_RX_FIFO_0:
            fillLevel = hcan.Instance->RF0R & CAN_RF0R_FMP0;
            break;

        case CAN_RX_FIFO_1:
            fillLevel = hcan.Instance->RF0R & CAN_RF1R_FMP1;
            break;

        default:
            // should never get here
            break;
    }

    return fillLevel;
}


/**
 * HW_CAN_getRxFifoEmpty
 * @brief  Return whether the RX FIFO is empty
 * @param  rxFifo Rx FIFO.
 *         This parameter can be a value of @arg CAN_receive_FIFO_number.
 * @retval Number of messages available in Rx FIFO.
 */
bool HW_CAN_getRxFifoEmpty(CAN_bus_E bus, CAN_RxFifo_E rxFifo)
{
    UNUSED(bus);
    if ((hcan.State != HAL_CAN_STATE_READY) && (hcan.State != HAL_CAN_STATE_LISTENING))
    {
        // CAN peripheral is not ready
        return true;
    }

    bool empty;
    switch (rxFifo)
    {
        case CAN_RX_FIFO_0:
            empty = (hcan.Instance->RF0R & CAN_RF0R_FMP0) == 0UL;
            break;

        case CAN_RX_FIFO_1:
            empty = (hcan.Instance->RF1R & CAN_RF1R_FMP1) == 0UL;
            break;

        default:
            // should never get here
            empty = true;
            break;
    }
    return empty;
}


// define interrupt handlers

/**
 * CAN_TxComplete_ISR
 * This ISR is called whenever a CAN mailbox is free. It should then call the tx thread
 * associated with the mailbox that became free
 * @param mailbox CAN mailbox which has become free
 */
static void HW_CAN_TxComplete_ISR(CAN_HandleTypeDef* canHandle, CAN_TxMailbox_E mailbox)
{
    UNUSED(canHandle);
    UNUSED(mailbox);
}


/**
 * CAN_RxMsgPending_ISR
 * This ISR is called whenever a CAN message is received. It should then move
 * the RXd message into the RX queue to free up the rxFifo
 * @param fifoId RX Fifo with a pending message
 */
static void HW_CAN_RxMsgPending_ISR(CAN_HandleTypeDef* canHandle, CAN_RxFifo_E fifoId)
{
#if FEATURE_CANRX_SWI == FEATURE_DISABLED
    CAN_data_T          data = {0U};
    CAN_RxHeaderTypeDef header = {0U};
#endif // FEATURE_CANRX_SWI == FEATURE_DISABLED

    if (canHandle == &hcan)
    {
#if FEATURE_CANRX_SWI == FEATURE_DISABLED
        HAL_CAN_GetRxMessage(canHandle, fifoId, &header, (uint8_t*)&data);
        CANRX_VEH_unpackMessage((uint16_t)header.StdId, &data);

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
        CANRX_notify(CAN_BUS_VEH, fifoId);
#endif // FEATURE_CANRX_SWI
    }
}


/**
 * HW_CAN_TxError_ISR
 * @param canHandle which CAN handle to operate on
 */
static void HW_CAN_TxError_ISR(CAN_HandleTypeDef* canHandle, CAN_TxMailbox_E mailbox)
{
    UNUSED(canHandle);
    UNUSED(mailbox);
}


// overload default interrupt callback functions provided by HAL

/**
 * HAL_CAN_TxMailbox0CompleteCallback
 * @param canHandle which CAN handle to operate on
 */
void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef* canHandle)
{
    HW_CAN_TxComplete_ISR(canHandle, CAN_TX_MAILBOX_0);
}


/**
 * HAL_CAN_TxMailbox1CompleteCallback
 * @param canHandle which CAN handle to operate on
 */
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef* canHandle)
{
    HW_CAN_TxComplete_ISR(canHandle, CAN_TX_MAILBOX_1);
}


/**
 * HAL_CAN_TxMailbox2CompleteCallback
 * @param canHandle which CAN handle to operate on
 */
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef* canHandle)
{
    HW_CAN_TxComplete_ISR(canHandle, CAN_TX_MAILBOX_2);
}


/**
 * HAL_CAN_TxMailbox0AbortCallback
 * @param canHandle TODO
 */
void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef* canHandle)
{
    HW_CAN_TxError_ISR(canHandle, CAN_TX_MAILBOX_0);
}


/**
 * HAL_CAN_TxMailbox1AbortCallback
 * @param canHandle TODO
 */
void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef* canHandle)
{
    HW_CAN_TxError_ISR(canHandle, CAN_TX_MAILBOX_1);
}


/**
 * HAL_CAN_TxMailbox2AbortCallback
 * @param canHandle TODO
 */
void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef* canHandle)
{
    HW_CAN_TxError_ISR(canHandle, CAN_TX_MAILBOX_2);
}


/**
 * HAL_CAN_RxFifo0MsgPendingCallback
 * @param canHandle TODO
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* canHandle)
{
#if FEATURE_IS_ENABLED(FEATURE_CANRX_SWI)
    HAL_CAN_DeactivateNotification(canHandle, CAN_IER_FMPIE0);
#endif // FEATURE_CANRX_SWI
    HW_CAN_RxMsgPending_ISR(canHandle, CAN_RX_FIFO_0);
}


/**
 * HAL_CAN_RxFifo1MsgPendingCallback
 * @param canHandle TODO
 */
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef* canHandle)
{
#if FEATURE_IS_ENABLED(FEATURE_CANRX_SWI)
    HAL_CAN_DeactivateNotification(canHandle, CAN_IER_FMPIE1);
#endif // FEATURE_CANRX_SWI
    HW_CAN_RxMsgPending_ISR(canHandle, CAN_RX_FIFO_1);
}


/**
 * HAL_CAN_RxFifo0FullCallback
 * @param canHandle TODO
 */
void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef* canHandle)
{
    HW_CAN_RxMsgPending_ISR(canHandle, CAN_RX_FIFO_0);
#if FEATURE_IS_ENABLED(FEATURE_CANRX_SWI)
    HAL_CAN_DeactivateNotification(canHandle, CAN_IER_FFIE0);
    SWI_invokeFromISR(CANRX_swi);
#endif // FEATURE_CANRX_SWI
}


/**
 * HAL_CAN_RxFifo1FullCallback
 * @param canHandle TODO
 */
void HAL_CAN_RxFifo1FullCallback(CAN_HandleTypeDef* canHandle)
{
    HW_CAN_RxMsgPending_ISR(canHandle, CAN_RX_FIFO_1);
#if FEATURE_IS_ENABLED(FEATURE_CANRX_SWI)
    HAL_CAN_DeactivateNotification(canHandle, CAN_IER_FFIE1);
    SWI_invokeFromISR(CANRX_swi);
#endif // FEATURE_CANRX_SWI
}

/**
 * HAL_CAN_ErrorCallback
 * @param canHandle TODO
 */
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef* canHandle)
{
    UNUSED(canHandle);
    HAL_CAN_DeactivateNotification(canHandle, CAN_IER_ERRIE);
}


// low level hardware initialization

/**
 * HAL_CAN_MspInit
 * @param canHandle which CAN handle to operate on
 */
void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{
    if (canHandle->Instance == CAN1)
    {
        // CAN1 clock enable
        __HAL_RCC_CAN1_CLK_ENABLE();

        __HAL_RCC_GPIOB_CLK_ENABLE();

        /**CAN GPIO Configuration
         * PB8     ------> CAN_RX
         * PB9     ------> CAN_TX
         */
        GPIO_InitTypeDef GPIO_InitStruct = { 0 };

        GPIO_InitStruct.Pin   = CAN_RXD_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        HAL_GPIO_Init(CAN_Port, &GPIO_InitStruct);

        GPIO_InitStruct.Pin   = CAN_TXD_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(CAN_Port, &GPIO_InitStruct);

        __HAL_AFIO_REMAP_CAN1_2();

        HAL_NVIC_SetPriority(CAN1_SCE_IRQn, CAN_TX_IRQ_PRIO, 0U);
        HAL_NVIC_SetPriority(CAN1_TX_IRQn,  CAN_TX_IRQ_PRIO, 0U);
        HAL_NVIC_SetPriority(CAN1_RX0_IRQn, CAN_RX_IRQ_PRIO, 0U);
        HAL_NVIC_SetPriority(CAN1_RX1_IRQn, CAN_RX_IRQ_PRIO, 0U);

        HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);
        HAL_NVIC_EnableIRQ(CAN1_TX_IRQn);
        HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
        HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);
    }
}

/**
 * HAL_CAN_MspDeInit
 * @param canHandle which CAN handle to operate on
 */
void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{
    if (canHandle->Instance == CAN1)
    {
        // Peripheral clock disable
        __HAL_RCC_CAN1_CLK_DISABLE();

        /**CAN GPIO Configuration
         * PA11     ------> CAN_RX
         * PA12     ------> CAN_TX
         */
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8 | GPIO_PIN_9);

        HAL_NVIC_DisableIRQ(CAN1_SCE_IRQn);
        HAL_NVIC_DisableIRQ(CAN1_TX_IRQn);
        HAL_NVIC_DisableIRQ(CAN1_RX0_IRQn);
        HAL_NVIC_DisableIRQ(CAN1_RX1_IRQn);
    }
}
