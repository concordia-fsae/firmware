/**
 * @file HW_can.c
 * @brief  Source code for CAN firmware
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_can.h"

#include "HW.h"
#include "stdint.h"

#include "CAN/CAN.h"
#include "CAN/CanTypes.h"
#include "FloatTypes.h"

#include "BMS.h"
#include "Sys.h"

#include "NetworkDefines_generated.h"
#include "MessageUnpack_generated.h"
#include "uds.h"
#include "uds_componentSpecific.h"
#include "LIB_app.h"

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
 *                           P U B L I C  V A R S
 ******************************************************************************/

CAN_HandleTypeDef hcan[CAN_BUS_COUNT];

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static inline CAN_bus_E HW_CAN_getBusFromPeripheral(CAN_HandleTypeDef* canHandle)
{
    CAN_bus_E bus = 0U;

    for (CAN_bus_E i = 0U; i < CAN_BUS_COUNT; i++)
    {
        if (&hcan[i] == canHandle)
        {
            bus = i;
            i = CAN_BUS_COUNT;
        }
    }

    return bus;
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

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
#if BMSB_CONFIG_ID == 0U
    bus = CAN_BUS_VEH;
#endif

    CAN_TxMessage_T msg = {0};

    msg.id          = id;
    msg.data        = data;
    msg.lengthBytes = len;
    msg.IDE         = (id <= 0x7ff) ? CAN_IDENTIFIER_STD : CAN_IDENTIFIER_EXT;

    return HW_CAN_sendMsgOnPeripheral(bus, msg) == HW_OK;
}

/**
 * HW_CAN_Init
 * initialize the CAN peripheral
 */
HW_StatusTypeDef_E HW_CAN_init(void)
{
    hcan[CAN_BUS_VEH].Instance = CAN1;
#if BMSB_CONFIG_ID > 0U
    hcan[CAN_BUS_PRIVBMS].Instance = CAN2;
    for (CAN_bus_E bus = 0U; bus < CAN_BUS_COUNT; bus++)
    {
#else
    {
        CAN_bus_E bus = CAN_BUS_VEH;
#endif
        hcan[bus].Init.Mode                 = CAN_MODE_NORMAL;
        hcan[bus].Init.TimeTriggeredMode    = DISABLE;
        hcan[bus].Init.AutoBusOff           = ENABLE;
        hcan[bus].Init.AutoWakeUp           = DISABLE;
        hcan[bus].Init.AutoRetransmission   = ENABLE;
        hcan[bus].Init.ReceiveFifoLocked    = DISABLE;
        hcan[bus].Init.TransmitFifoPriority = DISABLE;

        switch(CAN_busConfig[bus].baudrate)
        {
            case CAN_BAUDRATE_1MBIT:
                hcan[bus].Init.Prescaler     = 4;
                hcan[bus].Init.SyncJumpWidth = CAN_SJW_1TQ;
                hcan[bus].Init.TimeSeg1      = CAN_BS1_6TQ;
                hcan[bus].Init.TimeSeg2      = CAN_BS2_1TQ;
                break;
            case CAN_BAUDRATE_500KBIT:
                hcan[bus].Init.Prescaler     = 4;
                hcan[bus].Init.SyncJumpWidth = CAN_SJW_1TQ;
                hcan[bus].Init.TimeSeg1      = CAN_BS1_12TQ;
                hcan[bus].Init.TimeSeg2      = CAN_BS2_3TQ;
                break;
            default:
                return HW_ERROR;
        }

        if (HAL_CAN_Init(&hcan[bus]) != HAL_OK)
        {
            Error_Handler();
        }
    }

    _Static_assert(((COUNTOF(CANRX_VEH_unpackList) + ((4U - COUNTOF(CANRX_VEH_unpackList) % 4U) % 4U)) / 4) + (COUNTOF(CANRX_VEH_unpackListExtID) / 2U) +
                   ((COUNTOF(CANRX_PRIVBMS_unpackList) + ((4U - COUNTOF(CANRX_PRIVBMS_unpackList) % 4U) % 4U)) / 4) + (COUNTOF(CANRX_PRIVBMS_unpackListExtID) / 2U)
                   <= CAN_FILTERBANK_LENGTH, "Too many CAN filter bank's used");
    uint8_t filterBank = 0U;
    for (uint32_t i = 0U; i < COUNTOF(CANRX_VEH_unpackList); i += 4U)
    {
        CAN_FilterTypeDef filt = { 0U };
        filt.FilterBank           = filterBank++;
        filt.FilterMode           = CAN_FILTERMODE_IDLIST;
        filt.FilterScale          = CAN_FILTERSCALE_16BIT;
        // All filters are shifted left 5 bits
        filt.FilterIdHigh = CANRX_VEH_unpackList[i + 0U] << 5U;
        if ((i + 1U) < COUNTOF(CANRX_VEH_unpackList)) { filt.FilterIdLow = CANRX_VEH_unpackList[i + 1U] << 5U; }
        if ((i + 2U) < COUNTOF(CANRX_VEH_unpackList)) { filt.FilterMaskIdHigh = CANRX_VEH_unpackList[i + 2U] << 5U; }
        if ((i + 3U) < COUNTOF(CANRX_VEH_unpackList)) { filt.FilterMaskIdLow = CANRX_VEH_unpackList[i + 3U] << 5U; }
        filt.FilterFIFOAssignment = i % CAN_RX_FIFO_COUNT;
        filt.FilterActivation     = ENABLE;
        filt.SlaveStartFilterBank = (COUNTOF(CANRX_VEH_unpackList) + ((4U - COUNTOF(CANRX_VEH_unpackList) % 4U) % 4U)) / 4U;
        HAL_CAN_ConfigFilter(&hcan[CAN_BUS_VEH], &filt);
    }
    for (uint32_t i = 0U; i < COUNTOF(CANRX_VEH_unpackListExtID); i+= 2U)
    {
        CAN_FilterTypeDef filt = { 0U };
        filt.FilterBank           = filterBank++;
        filt.FilterMode           = CAN_FILTERMODE_IDLIST;
        filt.FilterScale          = CAN_FILTERSCALE_32BIT;
        // All filters are fucky - lookup RM0008 information
        filt.FilterIdHigh = (uint16_t)(CANRX_VEH_unpackListExtID[i + 0U] >> 13U);
        filt.FilterIdLow = (uint16_t)(CANRX_VEH_unpackListExtID[i + 0U] << 3U) | (0x01 << 2U);
        if ((i + 1U) < COUNTOF(CANRX_VEH_unpackListExtID)) {
            filt.FilterMaskIdHigh = (uint16_t)(CANRX_VEH_unpackListExtID[i + 1U] >> 13U);
            filt.FilterMaskIdLow = (uint16_t)(CANRX_VEH_unpackListExtID[i + 1U] << 3U) | (0x01 << 2U);
        }
        filt.FilterFIFOAssignment = i % CAN_RX_FIFO_COUNT;
        filt.FilterActivation     = ENABLE;
        filt.SlaveStartFilterBank = (COUNTOF(CANRX_VEH_unpackList) + ((4U - COUNTOF(CANRX_VEH_unpackList) % 4U) % 4U)) / 4U;
        HAL_CAN_ConfigFilter(&hcan[CAN_BUS_VEH], &filt);
    }
#if BMSB_CONFIG_ID > 0U
    HAL_CAN_ActivateNotification(&hcan[CAN_BUS_VEH], CAN_ENABLED_INTERRUPTS);
#endif

    for (uint32_t i = 0U; i < COUNTOF(CANRX_PRIVBMS_unpackList); i += 4U)
    {
        CAN_FilterTypeDef filt = { 0U };
        filt.FilterBank           = filterBank++;
        filt.FilterMode           = CAN_FILTERMODE_IDLIST;
        filt.FilterScale          = CAN_FILTERSCALE_16BIT;
        // All filters are shifted left 5 bits
        filt.FilterIdHigh = CANRX_PRIVBMS_unpackList[i + 0U] << 5U;
        if ((i + 1U) < COUNTOF(CANRX_PRIVBMS_unpackList)) { filt.FilterIdLow = CANRX_PRIVBMS_unpackList[i + 1U] << 5U; }
        if ((i + 2U) < COUNTOF(CANRX_PRIVBMS_unpackList)) { filt.FilterMaskIdHigh = CANRX_PRIVBMS_unpackList[i + 2U] << 5U; }
        if ((i + 3U) < COUNTOF(CANRX_PRIVBMS_unpackList)) { filt.FilterMaskIdLow = CANRX_PRIVBMS_unpackList[i + 3U] << 5U; }
        filt.FilterFIFOAssignment = i % CAN_RX_FIFO_COUNT;
        filt.FilterActivation     = ENABLE;
        filt.SlaveStartFilterBank = (COUNTOF(CANRX_VEH_unpackList) + ((4U - COUNTOF(CANRX_VEH_unpackList) % 4U) % 4U)) / 4U;
#if BMSB_CONFIG_ID > 0U
        HAL_CAN_ConfigFilter(&hcan[CAN_BUS_PRIVBMS], &filt);
#else
        HAL_CAN_ConfigFilter(&hcan[CAN_BUS_VEH], &filt);
#endif
    }
    for (uint32_t i = 0U; i < COUNTOF(CANRX_PRIVBMS_unpackListExtID); i+= 2U)
    {
        CAN_FilterTypeDef filt = { 0U };
        filt.FilterBank           = filterBank++;
        filt.FilterMode           = CAN_FILTERMODE_IDLIST;
        filt.FilterScale          = CAN_FILTERSCALE_32BIT;
        // All filters are fucky - lookup RM0008 information
        filt.FilterIdHigh = (uint16_t)(CANRX_PRIVBMS_unpackListExtID[i + 0U] >> 13U);
        filt.FilterIdLow = (uint16_t)(CANRX_PRIVBMS_unpackListExtID[i + 0U] << 3U) | (0x01 << 2U);
        if ((i + 1U) < COUNTOF(CANRX_PRIVBMS_unpackListExtID)) {
            filt.FilterMaskIdHigh = (uint16_t)(CANRX_PRIVBMS_unpackListExtID[i + 1U] >> 13U);
            filt.FilterMaskIdLow = (uint16_t)(CANRX_PRIVBMS_unpackListExtID[i + 1U] << 3U) | (0x01 << 2U);
        }
        filt.FilterFIFOAssignment = i % CAN_RX_FIFO_COUNT;
        filt.FilterActivation     = ENABLE;
        filt.SlaveStartFilterBank = (COUNTOF(CANRX_VEH_unpackList) + ((4U - COUNTOF(CANRX_VEH_unpackList) % 4U) % 4U)) / 4U;
#if BMSB_CONFIG_ID > 0U
        HAL_CAN_ConfigFilter(&hcan[CAN_BUS_PRIVBMS], &filt);
#else
        HAL_CAN_ConfigFilter(&hcan[CAN_BUS_VEH], &filt);
#endif
    }
#if BMSB_CONFIG_ID > 0U
    HAL_CAN_ActivateNotification(&hcan[CAN_BUS_PRIVBMS], CAN_ENABLED_INTERRUPTS);
#else
    HAL_CAN_ActivateNotification(&hcan[CAN_BUS_VEH], CAN_ENABLED_INTERRUPTS);
#endif

    return HW_OK;
}

/**
 * HAL_CAN_MspInit
 * @param canHandle which CAN handle to operate on
 */
void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{
    if (canHandle->Instance == hcan[CAN_BUS_VEH].Instance)
    {
        // CAN1 clock enable
        __HAL_RCC_CAN1_CLK_ENABLE();

#if BMSB_CONFIG_ID == 0U
        __HAL_AFIO_REMAP_CAN1_2();
#endif
        HAL_NVIC_SetPriority(CAN1_SCE_IRQn, CAN_TX_IRQ_PRIO, 0U);
        HAL_NVIC_SetPriority(CAN1_TX_IRQn, CAN_TX_IRQ_PRIO, 0U);
        HAL_NVIC_SetPriority(CAN1_RX0_IRQn, CAN_RX_IRQ_PRIO, 0U);
        HAL_NVIC_SetPriority(CAN1_RX1_IRQn, CAN_RX_IRQ_PRIO, 0U);

        HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);
        HAL_NVIC_EnableIRQ(CAN1_TX_IRQn);
        HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
        HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);

    }
#if BMSB_CONFIG_ID > 0U
    else if (canHandle->Instance == hcan[CAN_BUS_PRIVBMS].Instance)
    {
        __HAL_RCC_CAN2_CLK_ENABLE();

        HAL_NVIC_SetPriority(CAN2_SCE_IRQn, CAN_TX_IRQ_PRIO, 0U);
        HAL_NVIC_SetPriority(CAN2_TX_IRQn, CAN_TX_IRQ_PRIO, 0U);
        HAL_NVIC_SetPriority(CAN2_RX0_IRQn, CAN_RX_IRQ_PRIO, 0U);
        HAL_NVIC_SetPriority(CAN2_RX1_IRQn, CAN_RX_IRQ_PRIO, 0U);

        HAL_NVIC_EnableIRQ(CAN2_SCE_IRQn);
        HAL_NVIC_EnableIRQ(CAN2_TX_IRQn);
        HAL_NVIC_EnableIRQ(CAN2_RX0_IRQn);
        HAL_NVIC_EnableIRQ(CAN2_RX1_IRQn);
    }
#endif
}

/**
 * HAL_CAN_MspDeInit
 * @param canHandle which CAN handle to operate on
 */
void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{
    if (canHandle->Instance == hcan[CAN_BUS_VEH].Instance)
    {
        // Peripheral clock disable
        __HAL_RCC_CAN1_CLK_DISABLE();

        HAL_NVIC_DisableIRQ(CAN1_SCE_IRQn);
        HAL_NVIC_DisableIRQ(CAN1_TX_IRQn);
        HAL_NVIC_DisableIRQ(CAN1_RX0_IRQn);
        HAL_NVIC_DisableIRQ(CAN1_RX1_IRQn);
    }
#if BMSB_CONFIG_ID > 0U
    else if (canHandle->Instance == hcan[CAN_BUS_VEH].Instance)
    {
        // Peripheral clock disable
        __HAL_RCC_CAN2_CLK_DISABLE();

        HAL_NVIC_DisableIRQ(CAN2_SCE_IRQn);
        HAL_NVIC_DisableIRQ(CAN2_TX_IRQn);
        HAL_NVIC_DisableIRQ(CAN2_RX0_IRQn);
        HAL_NVIC_DisableIRQ(CAN2_RX1_IRQn);
    }
#endif
}

// overload default interrupt callback functions provided by HAL
/**
 * HAL_CAN_TxMailbox0CompleteCallback
 * @param canHandle which CAN handle to operate on
 */
void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef* canHandle)
{
    CAN_bus_E bus = HW_CAN_getBusFromPeripheral(canHandle);
    HW_CAN_TxComplete_ISR(bus, CAN_TX_MAILBOX_0);
    HAL_CAN_DeactivateNotification(&hcan[bus], CAN_IER_TMEIE);
}

/**
 * HAL_CAN_TxMailbox1CompleteCallback
 * @param canHandle which CAN handle to operate on
 */
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef* canHandle)
{
    CAN_bus_E bus = HW_CAN_getBusFromPeripheral(canHandle);
    HW_CAN_TxComplete_ISR(bus, CAN_TX_MAILBOX_1);
    HAL_CAN_DeactivateNotification(&hcan[bus], CAN_IER_TMEIE);
}

/**
 * HAL_CAN_TxMailbox2CompleteCallback
 * @param canHandle which CAN handle to operate on
 */
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef* canHandle)
{
    CAN_bus_E bus = HW_CAN_getBusFromPeripheral(canHandle);
    HW_CAN_TxComplete_ISR(bus, CAN_TX_MAILBOX_2);
    HAL_CAN_DeactivateNotification(&hcan[bus], CAN_IER_TMEIE);
}

/**
 * HAL_CAN_TxMailbox0AbortCallback
 * @param canHandle TODO
 */
void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef* canHandle)
{
    CAN_bus_E bus = HW_CAN_getBusFromPeripheral(canHandle);
    HW_CAN_TxError_ISR(bus, CAN_TX_MAILBOX_0);
}

/**
 * HAL_CAN_TxMailbox1AbortCallback
 * @param canHandle TODO
 */
void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef* canHandle)
{
    CAN_bus_E bus = HW_CAN_getBusFromPeripheral(canHandle);
    HW_CAN_TxError_ISR(bus, CAN_TX_MAILBOX_1);
}

/**
 * HAL_CAN_TxMailbox2AbortCallback
 * @param canHandle TODO
 */
void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef* canHandle)
{
    CAN_bus_E bus = HW_CAN_getBusFromPeripheral(canHandle);
    HW_CAN_TxError_ISR(bus, CAN_TX_MAILBOX_2);
}

/**
 * HAL_CAN_RxFifo0MsgPendingCallback
 * @param canHandle TODO
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* canHandle)
{
    CAN_bus_E bus = HW_CAN_getBusFromPeripheral(canHandle);
#if FEATURE_IS_ENABLED(FEATURE_CANRX_SWI)
    HAL_CAN_DeactivateNotification(canHandle, CAN_IER_FMPIE0);
#endif // FEATURE_CANRX_SWI
    HW_CAN_RxMsgPending_ISR(bus, CAN_RX_FIFO_0);
}

/**
 * HAL_CAN_RxFifo1MsgPendingCallback
 * @param canHandle TODO
 */
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef* canHandle)
{
    CAN_bus_E bus = HW_CAN_getBusFromPeripheral(canHandle);
#if FEATURE_IS_ENABLED(FEATURE_CANRX_SWI)
    HAL_CAN_DeactivateNotification(canHandle, CAN_IER_FMPIE1);
#endif // FEATURE_CANRX_SWI
    HW_CAN_RxMsgPending_ISR(bus, CAN_RX_FIFO_1);
}

/**
 * HAL_CAN_RxFifo0FullCallback
 * @param canHandle TODO
 */
void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef* canHandle)
{
    CAN_bus_E bus = HW_CAN_getBusFromPeripheral(canHandle);
    HW_CAN_RxMsgPending_ISR(bus, CAN_RX_FIFO_0);
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
    CAN_bus_E bus = HW_CAN_getBusFromPeripheral(canHandle);
    HW_CAN_RxMsgPending_ISR(bus, CAN_RX_FIFO_1);
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
    HAL_CAN_DeactivateNotification(canHandle, CAN_IER_ERRIE);
}
