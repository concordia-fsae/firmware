/**
 * HW_can.c
 * Hardware CAN controller implementation
 */
/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_can.h"

#include "CAN/CAN.h"
#include "CAN/CanTypes.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

// which CAN interrupts we want to enable
// stm32f1xx_hal_can.h from ST drivers for description of each one
#define CAN_ENABLED_INTERRUPTS    (CAN_IER_TMEIE | CAN_IER_FMPIE0 | CAN_IER_FMPIE1 | CAN_IER_FFIE0 | \
                                   CAN_IER_FFIE1 | CAN_IER_FOVIE0 | CAN_IER_FOVIE1 | CAN_IER_WKUIE | \
                                   CAN_IER_SLKIE | CAN_IER_EWGIE | CAN_IER_EPVIE | CAN_IER_BOFIE |   \
                                   CAN_IER_LECIE | CAN_IER_ERRIE)


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

CAN_HandleTypeDef hcan;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/


/**
 * CAN_Start
 * Start the CAN module(s)
 */
void CAN_Start(void)
{
    HAL_CAN_Start(&hcan);    // start CAN
}


/**
 * HW_CAN_Init
 * initialize the CAN peripheral
 */
void HW_CAN_Init(void)
{
    hcan.Instance                  = CAN1;
    hcan.Init.Prescaler            = 4;
    hcan.Init.Mode                 = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth        = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1             = CAN_BS1_6TQ;
    hcan.Init.TimeSeg2             = CAN_BS2_1TQ;
    hcan.Init.TimeTriggeredMode    = DISABLE;
    hcan.Init.AutoBusOff           = DISABLE;
    hcan.Init.AutoWakeUp           = DISABLE;
    hcan.Init.AutoRetransmission   = DISABLE;
    hcan.Init.ReceiveFifoLocked    = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE;

    if (HAL_CAN_Init(&hcan) != HAL_OK)
    {
        Error_Handler();
    }

    // activate selected CAN interrupts
    HAL_CAN_ActivateNotification(&hcan, CAN_ENABLED_INTERRUPTS);
}


/**
 * CAN_checkMbFree
 * @param canHandle which CAN handle to check
 * @param mailbox which mailbox to check
 * @return true if free
 */
static bool CAN_checkMbFree(CAN_HandleTypeDef *canHandle, CAN_TxMailbox_E mailbox)
{
    uint32_t tsr = READ_REG(canHandle->Instance->TSR);

    return(tsr & (CAN_TSR_TME0 << mailbox));
}


/**
 * CAN_sendMsg
 * @param canHandle which CAN handle to operate on
 * @param msg message data
 * @return exit code
 */
static HAL_StatusTypeDef CAN_sendMsg(CAN_HandleTypeDef* canHandle, CAN_TxMessage_t msg)
{
    HAL_CAN_StateTypeDef state = canHandle->State;

    if ((state == HAL_CAN_STATE_READY) || (state == HAL_CAN_STATE_LISTENING))
    {
        // check that the specified mailbox is free
        if (CAN_checkMbFree(canHandle, msg.mailbox))
        {
            // set CAN ID
            canHandle->Instance->sTxMailBox[msg.mailbox].TIR = ((msg.id << CAN_TI0R_STID_Pos) | msg.RTR);
            // set message length
            canHandle->Instance->sTxMailBox[msg.mailbox].TDTR = msg.lengthBytes;

            // set message data
            canHandle->Instance->sTxMailBox[msg.mailbox].TDHR = msg.data.u32[1];
            canHandle->Instance->sTxMailBox[msg.mailbox].TDLR = msg.data.u32[0];
            // TODO: test whether WRITE_REG compiles down to a different instruction than
            // just directly setting the register
            // WRITE_REG(hcan->Instance->sTxMailBox[msg.mailbox].TDHR,
            // ((uint32_t)aData[7] << CAN_TDH0R_DATA7_Pos) |
            // ((uint32_t)aData[6] << CAN_TDH0R_DATA6_Pos) |
            // ((uint32_t)aData[5] << CAN_TDH0R_DATA5_Pos) |
            // ((uint32_t)aData[4] << CAN_TDH0R_DATA4_Pos));
            // WRITE_REG(hcan->Instance->sTxMailBox[msg.mailbox].TDLR,
            // ((uint32_t)aData[3] << CAN_TDL0R_DATA3_Pos) |
            // ((uint32_t)aData[2] << CAN_TDL0R_DATA2_Pos) |
            // ((uint32_t)aData[1] << CAN_TDL0R_DATA1_Pos) |
            // ((uint32_t)aData[0] << CAN_TDL0R_DATA0_Pos));

            // request message transmission
            SET_BIT(canHandle->Instance->sTxMailBox[msg.mailbox].TIR, CAN_TI0R_TXRQ);

            // Return function status
            return HAL_OK;
        }
        else
        {
            // update error to show that no mailbox was free
            canHandle->ErrorCode |= HAL_CAN_ERROR_PARAM;

            return HAL_ERROR;
        }
    }
    else
    {
        // update error to show that peripheral was in the wrong state for transmission
        canHandle->ErrorCode |= HAL_CAN_ERROR_NOT_INITIALIZED;

        return HAL_ERROR;
    }
}


/**
 * CAN_sendMsgBus0
 * @param priority TODO
 * @param data TODO
 * @param id TODO
 * @param len TODO
 * @return TODO
 */
bool CAN_sendMsgBus0(CAN_TX_Priorities_E priority, CAN_data_T data, uint16_t id, uint8_t len)
{
    CAN_TxMessage_t msg;

    msg.id          = id;
    msg.data        = data;
    msg.mailbox     = (CAN_TxMailbox_E)priority;
    msg.lengthBytes = len;

    return CAN_sendMsg(&hcan, msg) == HAL_OK;
}


// define interrupt handlers

/**
 * CAN_TxComplete_ISR
 * This ISR is called whenever a CAN mailbox is free. It should then call the tx thread
 * associated with the mailbox that became free
 * @param mailbox CAN mailbox which has become free
 */
static void CAN_TxComplete_ISR(CAN_HandleTypeDef* canHandle, CAN_TxMailbox_E mailbox)
{
    UNUSED(canHandle);
    switch ((CAN_TX_Priorities_E)mailbox)
    {
        case CAN_TX_PRIO_1KHZ:
            // not yet implemented
            // SWI_invokeFromISR(CAN_BUS_A_1ms_swi);
            break;

        case CAN_TX_PRIO_100HZ:
            SWI_invokeFromISR(CAN_BUS_A_10ms_swi);
            break;

        case CAN_TX_PRIO_10HZ:
            // not yet implemented
            // SWI_invokeFromISR(CAN_BUS_A_100ms_swi);
            break;

        default:
            // should never reach here
            break;
    }
}


/**
 * CAN_RxMsgPending_ISR
 * This ISR is called whenever a CAN message is received. It should then move
 * the RXd message into the RX queue to free up the rxFifo
 * @param fifoId RX Fifo with a pending message
 */
static void CAN_RxMsgPending_ISR(CAN_HandleTypeDef* canHandle, CAN_RxFifo_E fifoId)
{
    UNUSED(canHandle);
    UNUSED(fifoId);
}


/**
 * CAN_TxError_ISR
 * @param canHandle which CAN handle to operate on
 */
static void CAN_TxError_ISR(CAN_HandleTypeDef* canHandle, CAN_TxMailbox_E mailbox)
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
    CAN_TxComplete_ISR(canHandle, CAN_TX_MAILBOX_0);
}


/**
 * HAL_CAN_TxMailbox1CompleteCallback
 * @param canHandle which CAN handle to operate on
 */
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef* canHandle)
{
    CAN_TxComplete_ISR(canHandle, CAN_TX_MAILBOX_1);
}


/**
 * HAL_CAN_TxMailbox2CompleteCallback
 * @param canHandle which CAN handle to operate on
 */
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef* canHandle)
{
    CAN_TxComplete_ISR(canHandle, CAN_TX_MAILBOX_2);
}


/**
 * HAL_CAN_TxMailbox0AbortCallback
 * @param canHandle TODO
 */
void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef* canHandle)
{
    CAN_TxError_ISR(canHandle, CAN_TX_MAILBOX_0);
}


/**
 * HAL_CAN_TxMailbox1AbortCallback
 * @param canHandle TODO
 */
void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef* canHandle)
{
    CAN_TxError_ISR(canHandle, CAN_TX_MAILBOX_1);
}


/**
 * HAL_CAN_TxMailbox2AbortCallback
 * @param canHandle TODO
 */
void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef* canHandle)
{
    CAN_TxError_ISR(canHandle, CAN_TX_MAILBOX_2);
}


/**
 * HAL_CAN_RxFifo0MsgPendingCallback
 * @param canHandle TODO
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* canHandle)
{
    CAN_RxMsgPending_ISR(canHandle, CAN_RX_FIFO_0);
}


/**
 * HAL_CAN_RxFifo1MsgPendingCallback
 * @param canHandle TODO
 */
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef* canHandle)
{
    CAN_RxMsgPending_ISR(canHandle, CAN_RX_FIFO_1);
}


/**
 * HAL_CAN_RxFifo0FullCallback
 * @param canHandle TODO
 */
void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef* canHandle)
{
    UNUSED(canHandle);
}


/**
 * HAL_CAN_RxFifo1FullCallback
 * @param canHandle TODO
 */
void HAL_CAN_RxFifo1FullCallback(CAN_HandleTypeDef* canHandle)
{
    UNUSED(canHandle);
}

/**
 * HAL_CAN_ErrorCallback
 * @param canHandle TODO
 */
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef* canHandle)
{
    UNUSED(canHandle);
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

        __HAL_RCC_GPIOA_CLK_ENABLE();
        /**CAN GPIO Configuration
         * PA11     ------> CAN_RX
         * PA12     ------> CAN_TX
         */
        GPIO_InitTypeDef GPIO_InitStruct = { 0 };

        GPIO_InitStruct.Pin  = GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        GPIO_InitStruct.Pin   = GPIO_PIN_12;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        HAL_NVIC_SetPriority(CAN1_SCE_IRQn, CAN_IRQ_PRIO, 0U);
        HAL_NVIC_SetPriority(CAN1_TX_IRQn, CAN_IRQ_PRIO, 0U);
        HAL_NVIC_SetPriority(CAN1_RX0_IRQn, CAN_IRQ_PRIO, 0U);
        HAL_NVIC_SetPriority(CAN1_RX1_IRQn, CAN_IRQ_PRIO, 0U);

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
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);

        HAL_NVIC_DisableIRQ(CAN1_SCE_IRQn);
        HAL_NVIC_DisableIRQ(CAN1_TX_IRQn);
        HAL_NVIC_DisableIRQ(CAN1_RX0_IRQn);
        HAL_NVIC_DisableIRQ(CAN1_RX1_IRQn);
    }
}

