/**
 * HW_can.h
 * Header file for the CAN hardware implementation
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SystemConfig.h"

#include "CAN/CanTypes.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

// which CAN interrupts we want to enable
#define CAN_ENABLED_INTERRUPTS    (CAN_IER_TMEIE | CAN_IER_FMPIE0 | \
                                   CAN_IER_FMPIE1 | CAN_IER_FFIE0 | \
                                   CAN_IER_FFIE1 | CAN_IER_FOVIE0 | \
                                   CAN_IER_FOVIE1 | CAN_IER_WKUIE | \
                                   CAN_IER_SLKIE | CAN_IER_EWGIE |  \
                                   CAN_IER_EPVIE | CAN_IER_BOFIE |  \
                                   CAN_IER_LECIE | CAN_IER_ERRIE)


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern CAN_HandleTypeDef hcan;


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    CAN_TX_PRIO_1kHz = 0U,
    CAN_TX_PRIO_100Hz,
    CAN_TX_PRIO_10Hz,
    CAN_TX_PRIO_1Hz,
} CAN_TX_Priorities_E;

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/* static uint8_t txComplete; */


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void MX_CAN_Init(void);

void CAN_Error_SWI(uint16_t errorId);
void CAN_ErrorUnknown_SWI(CAN_HandleTypeDef* canHandle);
void CAN_TxError_SWI(CAN_HandleTypeDef* canHandle);
void CAN_ErrorFIFOFull_SWI(CAN_HandleTypeDef* canHandle);

void CAN_TxComplete_SWI(uint8_t mailboxId);
void CAN_TxComplete_MB0_SWI(CAN_HandleTypeDef* canHandle);
void CAN_TxComplete_MB1_SWI(CAN_HandleTypeDef* canHandle);
void CAN_TxComplete_MB2_SWI(CAN_HandleTypeDef* canHandle);

void CAN_RxMsgPending_SWI(uint8_t fifoId);
void CAN_RxMsgPending_FIFO0_SWI(CAN_HandleTypeDef* canHandle);
void CAN_RxMsgPending_FIFO1_SWI(CAN_HandleTypeDef* canHandle);

HAL_StatusTypeDef CAN_sendMsg(CAN_HandleTypeDef* hcan, CAN_TxMessage_t msg);

#ifdef __cplusplus
}
#endif
