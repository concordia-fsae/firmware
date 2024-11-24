/*
 * HW_CAN.h
 * This file describes low-level, mostly hardware-specific CAN peripheral values
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Types.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CAN_BASE                (0x40006400UL)

// remap interrupt names to be more clear
#define CAN_TX_IRQHandler       USB_HP_CAN1_TX_IRQHandler
#define CAN_RX0_IRQHandler      USB_LP_CAN1_RX0_IRQHandler
#define CAN_RX1_IRQHandler      CAN1_RX1_IRQHandler
#define CAN_SCE_IRQHandler      CAN1_SCE_IRQHandler

// CAN Filters
#define CAN_FMR_FINIT           (0x00000001UL) // Filter Init Mode Bit

#define CAN_MCR_RESET           (0x00008000UL) // CAN Master Reset
#define CAN_MCR_INRQ            (0x00000001UL) // Initialization Request
#define CAN_MSR_INAK            (0x00000001UL) // Initialization Acknowledge
#define CAN_MCR_SLEEP           (0x00000002UL) // Sleep Mode Request
#define CAN_MSR_SLAK            (0x00000002UL) // Sleep Acknowledge
#define CAN_MCR_TTCM            (0x00000080UL) // Time Triggered Communication Mode
#define CAN_MCR_ABOM            (0x00000040UL) // Automatic Bus-Off Management
#define CAN_MCR_AWUM            (0x00000020UL) // Automatic Wakeup Mode
#define CAN_MCR_NART            (0x00000010UL) // No Automatic Retransmission
#define CAN_MCR_RFLM            (0x00000008UL) // Receive FIFO Locked Mode
#define CAN_MCR_TXFP            (0x00000004UL) // Transmit FIFO Priority

#define CAN_FIFO_RELEASE        (0x00000020UL) // RFOMx, FIFO release bit

#define CAN_RIR_STID            (0xFFE00000UL) // STD ID bits in RIxR register
#define CAN_RIR_STID_OFFSET     (21U)          // STD ID bit range offset
#define CAN_RIR_IDE             (0x00000004UL) // IDE bit in RIxR register
#define CAN_RIR_IDE_OFFSET      (2U)           // IDE bit offset
#define CAN_RIR_RTR             (0x00000002UL) // RTR bit in RIxR register
#define CAN_RIR_RTR_OFFSET      (1U)           // RTR bit offset
#define CAN_RDTR_TIME           (0xFFFF0000UL) // TIME bits in RDTxR register
#define CAN_RDTR_TIME_OFFSET    (16U)          // TIME bit range offset
#define CAN_RDTR_FMI            (0x0000FF00UL) // FMI bits in RDTxR register
#define CAN_RDTR_FMI_OFFSET     (8U)           // FMI bit range offset
#define CAN_RDTR_DLC            (0x0000000FUL) // DLC bits in RDTxR register
#define CAN_RDTR_DLC_OFFSET     (0U)           // DLC bit range offset

#define CAN_TI0R_STID_Pos       (21U)          // Standard ID Position offset
#define CAN_TI0R_TXRQ           (0x00000001UL) // Transmit Mailbox Request
#define CAN_TSR_TME0            (0x04000000UL) // Transmit Mailbox 0 Empty

// http://www.bittiming.can-wiki.info/
// CAN bit timing register helpers
#define CAN_BRP(x)    ((uint32_t)((x - 1U) & 0x01FFU))            // Baud Rate Prescaler
#define CAN_TS1(x)    ((uint32_t)(((x - 1U) & 0x0FU) << 16U))     // Time Segment 1
#define CAN_TS2(x)    ((uint32_t)(((x - 1U) & 0x07U) << 20U))     // Time Segment 2
#define CAN_SJW(x)    ((uint32_t)(((x - 1U) & 0x02U) << 24U))     // Sync Jump Width

// known good config to confirm that the helpers are working
_Static_assert((uint32_t)(CAN_BRP(2U) | CAN_TS1(13U) | CAN_TS2(4U) | CAN_SJW(1U)) == 0x0003C0001UL, "btr calculation wrong");

// configure Bit Timing Register
// with APB1 clock at 36Mhz
// for a 1MHz CAN bus
// 77.8% sample point
// #define CAN_BTR_CONFIG    (uint32_t)(CAN_BRP(2U) | CAN_TS1(13U) | CAN_TS2(4U) | CAN_SJW(1U))
// 88.9% sample point
// #define CAN_BTR_CONFIG    (uint32_t)(CAN_BRP(2U) | CAN_TS1(15U) | CAN_TS2(2U) | CAN_SJW(1U))

// APB1 CLK at 8MHz
// 87.5% sample point
#define CAN_BTR_CONFIG           (uint32_t)(CAN_BRP(1U) | CAN_TS1(6U) | CAN_TS2(1U) | CAN_SJW(1U))

// check how many messages are pending in the given fifo
#define CAN_MSG_PENDING(fifo)    ((uint8_t)(fifo == CAN_RX_FIFO_0 ? (pCAN->RF0R & 0x03UL) : (pCAN->RF1R & 0x03UL)))

// Interrupt Flags

#define CAN_TSR_RQCP0     (0x00000001UL)  // Request Completed Mailbox0
#define CAN_TSR_TXOK0     (0x00000002UL)  // Transmission OK of Mailbox0
#define CAN_TSR_ALST0     (0x00000004UL)  // Arbitration Lost for Mailbox0
#define CAN_TSR_TERR0     (0x00000008UL)  // Transmission Error of Mailbox0
#define CAN_TSR_RQCP1     (0x00000100UL)  // Request Completed Mailbox1
#define CAN_TSR_TXOK1     (0x00000200UL)  // Transmission OK of Mailbox1
#define CAN_TSR_ALST1     (0x00000400UL)  // Arbitration Lost for Mailbox1
#define CAN_TSR_TERR1     (0x00000800UL)  // Transmission Error of Mailbox1
#define CAN_TSR_RQCP2     (0x00010000UL)  // Request Completed Mailbox2
#define CAN_TSR_TXOK2     (0x00020000UL)  // Transmission OK of Mailbox 2
#define CAN_TSR_ALST2     (0x00040000UL)  // Arbitration Lost for mailbox 2
#define CAN_TSR_TERR2     (0x00080000UL)  // Transmission Error of Mailbox 2

#define CAN_RF0R_FMP0     (0x00000003UL)  // FIFO 0 Message Pending
#define CAN_RF0R_FULL0    (0x00000008UL)  // FIFO 0 Full
#define CAN_RF0R_FOVR0    (0x00000010UL)  // FIFO 0 Overrun

#define CAN_RF1R_FMP1     (0x00000003UL)  // FIFO 1 Message Pending
#define CAN_RF1R_FULL1    (0x00000008UL)  // FIFO 1 Full
#define CAN_RF1R_FOVR1    (0x00000010UL)  // FIFO 1 Overrun

#define CAN_MSR_ERRI      (0x00000004UL)  // Error Interrupt
#define CAN_MSR_WKUI      (0x00000008UL)  // Wakeup Interrupt
#define CAN_MSR_SLAKI     (0x00000010UL)  // Sleep Acknowledge Interrupt

#define CAN_ESR_EWGF      (0x00000001UL)  // Error Warning Flag
#define CAN_ESR_EPVF      (0x00000002UL)  // Error Passive Flag
#define CAN_ESR_BOFF      (0x00000004UL)  // Bus-Off Flag

#define CAN_ESR_LEC       (0x00000070UL)  // LEC[2:0] bits (Last Error Code)
#define CAN_ESR_LEC_0     (0x00000010UL)  // 0x00000010
#define CAN_ESR_LEC_1     (0x00000020UL)  // 0x00000020
#define CAN_ESR_LEC_2     (0x00000040UL)  // 0x00000040


// Flags

#define CAN_FLAG_MASK     (0x000000FFUL)
#define __HAL_CAN_CLEAR_FLAG(__FLAG__)                                                     \
        ((((__FLAG__) >> 8U) == 5U) ? ((pCAN->TSR) = (1U << ((__FLAG__)&CAN_FLAG_MASK))):  \
         (((__FLAG__) >> 8U) == 2U) ? ((pCAN->RF0R) = (1U << ((__FLAG__)&CAN_FLAG_MASK))): \
         (((__FLAG__) >> 8U) == 4U) ? ((pCAN->RF1R) = (1U << ((__FLAG__)&CAN_FLAG_MASK))): \
         (((__FLAG__) >> 8U) == 1U) ? ((pCAN->MSR) = (1U << ((__FLAG__)&CAN_FLAG_MASK))): 0U)

#define CAN_FLAG_RQCP0    (0x00000500UL)  // Request complete MailBox 0 flag
#define CAN_FLAG_RQCP1    (0x00000508UL)  // Request complete MailBox1 flag
#define CAN_FLAG_RQCP2    (0x00000510UL)  // Request complete MailBox2 flag
#define CAN_FLAG_FOV0     (0x00000204UL)  // RX FIFO 0 Overrun flag
#define CAN_FLAG_FF0      (0x00000203UL)  // RX FIFO 0 Full flag
#define CAN_FLAG_FOV1     (0x00000404UL)  // RX FIFO 1 Overrun flag
#define CAN_FLAG_FF1      (0x00000403UL)  // RX FIFO 1 Full flag
#define CAN_FLAG_SLAKI    (0x00000104UL)  // Sleep acknowledge interrupt flag
#define CAN_FLAG_WKU      (0x00000103UL)  // Wake up interrupt flag
#define CAN_FLAG_ERRI     (0x00000102UL)  // Error flag


// CAN interrupts
#define CAN_IER_TMEIE     (0x00000001UL)  // Transmit Mailbox Empty Interrupt Enable
#define CAN_IER_FMPIE0    (0x00000002UL)  // FIFO0 Message Pending Interrupt Enable
#define CAN_IER_FFIE0     (0x00000004UL)  // FIFO0 Full Interrupt Enable
#define CAN_IER_FOVIE0    (0x00000008UL)  // FIFO0 Overrun Interrupt Enable
#define CAN_IER_FMPIE1    (0x00000010UL)  // FIFO1 Message Pending Interrupt Enable
#define CAN_IER_FFIE1     (0x00000020UL)  // FIFO1 Full Interrupt Enable
#define CAN_IER_FOVIE1    (0x00000040UL)  // FIFO1 Overrun Interrupt Enable
#define CAN_IER_EWGIE     (0x00000100UL)  // Error Warning Interrupt Enable
#define CAN_IER_EPVIE     (0x00000200UL)  // Error Passive Interrupt Enable
#define CAN_IER_BOFIE     (0x00000400UL)  // Bus-Off Interrupt Enable
#define CAN_IER_LECIE     (0x00000800UL)  // Last Error Code Interrupt Enable
#define CAN_IER_ERRIE     (0x00008000UL)  // Error Interrupt Enable
#define CAN_IER_WKUIE     (0x00010000UL)  // Wakeup Interrupt Enable
#define CAN_IER_SLKIE     (0x00020000UL)  // Sleep Interrupt Enable


// Transmit Interrupt
#define CAN_IT_TX_MAILBOX_EMPTY        CAN_IER_TMEIE  // Transmit mailbox empty interrupt

// Receive Interrupts
#define CAN_IT_RX_FIFO0_MSG_PENDING    CAN_IER_FMPIE0  // FIFO 0 message pending interrupt
#define CAN_IT_RX_FIFO0_FULL           CAN_IER_FFIE0   // FIFO 0 full interrupt
#define CAN_IT_RX_FIFO0_OVERRUN        CAN_IER_FOVIE0  // FIFO 0 overrun interrupt
#define CAN_IT_RX_FIFO1_MSG_PENDING    CAN_IER_FMPIE1  // FIFO 1 message pending interrupt
#define CAN_IT_RX_FIFO1_FULL           CAN_IER_FFIE1   // FIFO 1 full interrupt
#define CAN_IT_RX_FIFO1_OVERRUN        CAN_IER_FOVIE1  // FIFO 1 overrun interrupt

// Operating Mode Interrupts
#define CAN_IT_WAKEUP                  CAN_IER_WKUIE  // Wake-up interrupt
#define CAN_IT_SLEEP_ACK               CAN_IER_SLKIE  // Sleep acknowledge interrupt

// Error Interrupts
#define CAN_IT_ERROR_WARNING           CAN_IER_EWGIE  // Error warning interrupt
#define CAN_IT_ERROR_PASSIVE           CAN_IER_EPVIE  // Error passive interrupt
#define CAN_IT_BUSOFF                  CAN_IER_BOFIE  // Bus-off interrupt
#define CAN_IT_LAST_ERROR_CODE         CAN_IER_LECIE  // Last error code interrupt
#define CAN_IT_ERROR                   CAN_IER_ERRIE  // Error Interrupt

#define CAN_ENABLED_INTERRUPTS         (CAN_IER_TMEIE | CAN_IER_FMPIE0 | CAN_IER_FMPIE1 | CAN_IER_FFIE0 | \
                                        CAN_IER_FFIE1 | CAN_IER_FOVIE0 | CAN_IER_FOVIE1 | \
                                        CAN_IER_EWGIE | CAN_IER_EPVIE | CAN_IER_BOFIE |   \
                                        CAN_IER_LECIE | CAN_IER_ERRIE)


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    HAL_CAN_ERROR_NONE            = 0x00000000UL,  // No error
    HAL_CAN_ERROR_EWG             = 0x00000001UL,  // Protocol Error Warning
    HAL_CAN_ERROR_EPV             = 0x00000002UL,  // Error Passive
    HAL_CAN_ERROR_BOF             = 0x00000004UL,  // Bus-off error
    HAL_CAN_ERROR_STF             = 0x00000008UL,  // Stuff error
    HAL_CAN_ERROR_FOR             = 0x00000010UL,  // Form error
    HAL_CAN_ERROR_ACK             = 0x00000020UL,  // Acknowledgment error
    HAL_CAN_ERROR_BR              = 0x00000040UL,  // Bit recessive error
    HAL_CAN_ERROR_BD              = 0x00000080UL,  // Bit dominant error
    HAL_CAN_ERROR_CRC             = 0x00000100UL,  // CRC error
    HAL_CAN_ERROR_RX_FOV0         = 0x00000200UL,  // Rx FIFO0 overrun error
    HAL_CAN_ERROR_RX_FOV1         = 0x00000400UL,  // Rx FIFO1 overrun error
    HAL_CAN_ERROR_TX_ALST0        = 0x00000800UL,  // TxMailbox 0 transmit failure due to arbitration lost
    HAL_CAN_ERROR_TX_TERR0        = 0x00001000UL,  // TxMailbox 0 transmit failure due to transmit error
    HAL_CAN_ERROR_TX_ALST1        = 0x00002000UL,  // TxMailbox 1 transmit failure due to arbitration lost
    HAL_CAN_ERROR_TX_TERR1        = 0x00004000UL,  // TxMailbox 1 transmit failure due to transmit error
    HAL_CAN_ERROR_TX_ALST2        = 0x00008000UL,  // TxMailbox 2 transmit failure due to arbitration lost
    HAL_CAN_ERROR_TX_TERR2        = 0x00010000UL,  // TxMailbox 2 transmit failure due to transmit error
    HAL_CAN_ERROR_TIMEOUT         = 0x00020000UL,  // Timeout error
    HAL_CAN_ERROR_NOT_INITIALIZED = 0x00040000UL,  // Peripheral not initialized
    HAL_CAN_ERROR_NOT_READY       = 0x00080000UL,  // Peripheral not ready
    HAL_CAN_ERROR_NOT_STARTED     = 0x00100000UL,  // Peripheral not started
    HAL_CAN_ERROR_PARAM           = 0x00200000UL,  // Parameter error
    HAL_CAN_ERROR_INTERNAL        = 0x00800000UL,  // Internal error
} CAN_exitCode_E;


typedef enum
{
    CAN_TX_MAILBOX_0 = 0U,
    CAN_TX_MAILBOX_1,
    CAN_TX_MAILBOX_2,
    CAN_TX_MAILBOX_COUNT,
} CAN_TxMailbox_E;

typedef enum
{
    CAN_IDENTIFIER_STD = 0U, // Standard length CAN ID
    CAN_IDENTIFIER_EXT,      // Extended length CAN ID
} CAN_IdentifierLen_E;

typedef enum
{
    CAN_REMOTE_TRANSMISSION_REQUEST_DATA = 0U,
    CAN_REMOTE_TRANSMISSION_REQUEST_REMOTE,
} CAN_RemoteTransmission_E;


typedef union
{
    uint64_t u64;
    uint32_t u32[2];
    uint16_t u16[4];
    uint8_t  u8[8];
} CAN_data_t;

typedef struct
{
    uint16_t                 id;

    CAN_IdentifierLen_E      IDE;
    CAN_RemoteTransmission_E RTR;

    uint8_t                  lengthBytes;
    CAN_data_t               data;
    CAN_TxMailbox_E          mailbox;
} CAN_TxMessage_S;

typedef struct
{
    uint16_t                 id;

    CAN_IdentifierLen_E      IDE;
    CAN_RemoteTransmission_E RTR;

    uint8_t                  lengthBytes;
    CAN_data_t               data;

    uint16_t                 timestamp;
    uint8_t                  filterMatchIndex;
} CAN_RxMessage_S;


typedef enum
{
    CAN_RX_FIFO_0 = 0U,
    CAN_RX_FIFO_1,
    CAN_RX_FIFO_COUNT,
} CAN_RxFIFO_E;


typedef struct
{
    volatile uint32_t TIR;
    volatile uint32_t TDTR;
    volatile uint32_t TDLR;
    volatile uint32_t TDHR;
} CAN_TxMailBox_regMap;


typedef struct
{
    volatile uint32_t RIR;
    volatile uint32_t RDTR;
    volatile uint32_t RDLR;
    volatile uint32_t RDHR;
} CAN_FIFOMailBox_regMap;


typedef struct
{
    volatile uint32_t FR1;
    volatile uint32_t FR2;
} CAN_FilterRegister_regMap;

typedef struct
{
    uint8_t RQCP0    :1;
    uint8_t TXOK0    :1;
    uint8_t ALST0    :1;
    uint8_t TERR0    :1;
    uint8_t RESERVED0:3;
    uint8_t ABRQ0    :1;
    uint8_t RQCP1    :1;
    uint8_t TXOK1    :1;
    uint8_t ALST1    :1;
    uint8_t TERR1    :1;
    uint8_t RESERVED1:3;
    uint8_t ABRQ1    :1;
    uint8_t RQCP2    :1;
    uint8_t TXOK2    :1;
    uint8_t ALST2    :1;
    uint8_t TERR2    :1;
    uint8_t RESERVED2:3;
    uint8_t ABRQ2    :1;
    uint8_t CODE     :2;
    uint8_t TME0     :1;
    uint8_t TME1     :1;
    uint8_t TME2     :1;
    uint8_t LOW0     :1;
    uint8_t LOW1     :1;
    uint8_t LOW2     :1;
} CAN_TSR_regMap;

_Static_assert((sizeof(CAN_TSR_regMap) == 4), "CAN TSR reg size incorrect");

typedef struct
{
    volatile uint32_t         MCR;
    volatile uint32_t         MSR;
    volatile uint32_t         TSR;
    volatile uint32_t         RF0R;
    volatile uint32_t         RF1R;
    volatile uint32_t         IER;
    volatile uint32_t         ESR;
    volatile uint32_t         BTR;
    uint32_t                  RESERVED0[88];
    CAN_TxMailBox_regMap      sTxMailBox[3];
    CAN_FIFOMailBox_regMap    rxFifos[2];
    uint32_t                  RESERVED1[12];
    volatile uint32_t         FMR;
    volatile uint32_t         FM1R;
    uint32_t                  RESERVED2;
    volatile uint32_t         FS1R;
    uint32_t                  RESERVED3;
    volatile uint32_t         FFA1R;
    uint32_t                  RESERVED4;
    volatile uint32_t         FA1R;
    uint32_t                  RESERVED5[8];
    CAN_FilterRegister_regMap sFilterRegister[14];
} CAN_regMap;

#define pCAN    ((CAN_regMap *)CAN_BASE)


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void           CAN_destroy(void);
CAN_exitCode_E CAN_init(void);
bool           CAN_sendMsg(CAN_TxMessage_S msg);

bool           CAN_getMessage(CAN_RxMessage_S* msg);
void           CAN_rxErrorAck(CAN_RxFIFO_E fifo);


/******************************************************************************
 *                    C A L L B A C K   P R O T O T Y P E S
 ******************************************************************************/

// these functions must be defined in the application
extern void CAN_CB_txComplete(CAN_TxMailbox_E mb);
extern void CAN_CB_txAbort(CAN_TxMailbox_E mb);
extern void CAN_CB_txError(CAN_TxMailbox_E mb, uint32_t errorCode);
extern void CAN_CB_messageRx(CAN_RxFIFO_E fifo);
extern void CAN_CB_rxError(CAN_RxFIFO_E fifo, uint32_t errorCode);
extern void CAN_CB_error(uint32_t errorCode);
