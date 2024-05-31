/*
 * HW_CAN.c
 * This file desribes low-level, mostly hardware-specific CAN peripheral behaviour
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "BuildDefines_generated.h"
#include "CAN.h"
#include "HW.h"
#include "HW_CAN.h"
#include "Utilities.h"

#include <string.h>    // memset


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * checkMbFree
 * @brief Check if the given TX mailbox is free
 * @param mailbox which mailbox to check
 * @return true if free
 */
static bool checkMbFree(CAN_TxMailbox_E mailbox)
{
    volatile uint32_t tsr = READ_REG(pCAN->TSR);

    return(tsr & (CAN_TSR_TME0 << mailbox));
}


/*
 * CAN_releaseRxFifo
 * @brief release the given FIFO, allowing the CAN peripheral to fill it again
 */
static void CAN_releaseRxFifo(CAN_RxFIFO_E fifo)
{
    switch (fifo)
    {
        case CAN_RX_FIFO_0:
            SET_BIT(pCAN->RF0R, CAN_FIFO_RELEASE);
            break;

        case CAN_RX_FIFO_1:
            SET_BIT(pCAN->RF1R, CAN_FIFO_RELEASE);
            break;

        default:
            break;
    }
}

/*
 * CAN_messageRxAck
 * @brief acknowledge that a message on the given fifo has been received
 *        this re-enables the message pending interrupt for that fifo
 * @param fifo CAN_RxFIFO_E the fifo to ack
 */
static void CAN_messageRxAck(CAN_RxFIFO_E fifo)
{
    switch (fifo)
    {
        case CAN_RX_FIFO_0:
            SET_BIT(pCAN->IER, CAN_IT_RX_FIFO0_MSG_PENDING);
            break;

        case CAN_RX_FIFO_1:
            SET_BIT(pCAN->IER, CAN_IT_RX_FIFO1_MSG_PENDING);
            break;

        default:
            break;
    }
}


/*
 * filterInit
 * @brief Initialize CAN RX Hardware Filters
 * TODO: build a public interface for this and refactor so it can enable multiple filters
 */
static void filterInit(void)
{
    // TODO: Generalize this so that we can pass in a list of filter config structs
    // and have it configure all the filters at once

    // Enter filter initialization mode
    SET_BIT(pCAN->FMR, CAN_FMR_FINIT);

    // Set slave bank config (is this necessary?)
    // SET_REG(pCAN->FMR, GET_REG(pCAN->FMR) | (0x28DUL << 7));

    // put filter 0 in Identifier List mode
    SET_BIT(pCAN->FM1R, (0x01UL << 0U));
    // leave filter 0 in dual 16bit mode
    CLEAR_BIT(pCAN->FS1R,  (0x01UL << 0U));
    // set FIFO assignment for filter 0 to FIFO 0
    CLEAR_BIT(pCAN->FFA1R, (0x01UL << 0U));
    // enable filter 0
    SET_BIT(pCAN->FA1R, (0x01UL << 0U));

    // set the filter bits to a particular identifier
    // per the configuration above (dual 16bit, identifier list mode)
    // we can catch up to four IDs with this filter
    SET_REG(&(pCAN->sFilterRegister[0].FR1), UDS_REQUEST_ID << 5U);
    SET_REG(&(pCAN->sFilterRegister[0].FR2), 0x00UL);

    // Exit filter initialization mode
    CLEAR_BIT(pCAN->FMR, CAN_FMR_FINIT);
}

/*
 * hwInit
 * @brief Initialize the low-level hardware that the CAN peripheral needs
 */
static void CAN_hwInit(void)
{
    // Enable CAN1 clock
    pRCC->APB1ENR |= RCC_APB1ENR_CAN1_CLK;
    // Enable AFIO clock
    pRCC->APB2ENR |= RCC_APB2ENR_AFIO_CLK;


    // set interrupt priorities
    NVIC_SetPriority(CAN1_SCE_IRQn, 5U, 0U);
    NVIC_SetPriority(CAN1_RX0_IRQn, 5U, 1U);
    NVIC_SetPriority(CAN1_RX1_IRQn, 5U, 2U);
    NVIC_SetPriority(CAN1_TX_IRQn,  5U, 3U);

    // enable interrupts for rx, tx, and errors
    NVIC_EnableIRQ(CAN1_SCE_IRQn);
    NVIC_EnableIRQ(CAN1_RX0_IRQn);
    NVIC_EnableIRQ(CAN1_RX1_IRQn);
    NVIC_EnableIRQ(CAN1_TX_IRQn);
}


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * CAN_sendMsg
 * @brief Send a CAN message
 * @param msg message data
 * @return true for success, false if mailbox is full
 */
bool CAN_sendMsg(CAN_TxMessage_S msg)
{
    // I really hate this, but it looks like it takes
    // somewhere between 200 and 1000 CPU cycles for
    // the CAN peripheral to transmit a message (depending on load on the 3 mailboxes)
    // Trash.
    for (uint16_t retries = 0; retries < 1000; retries++)
    {
        // check that the specified mailbox is free
        if (checkMbFree(msg.mailbox))
        {
            // set CAN ID
            pCAN->sTxMailBox[msg.mailbox].TIR  = ((msg.id << CAN_TI0R_STID_Pos) | msg.RTR);
            // set message length
            pCAN->sTxMailBox[msg.mailbox].TDTR = msg.lengthBytes;

            // set message data
            pCAN->sTxMailBox[msg.mailbox].TDHR = msg.data.u32[1];
            pCAN->sTxMailBox[msg.mailbox].TDLR = msg.data.u32[0];

            // request message transmission
            SET_BIT(pCAN->sTxMailBox[msg.mailbox].TIR, CAN_TI0R_TXRQ);

            // Return function status
            return true;
        }
    }

    return false;
}

/*
 * CAN_getMessage
 * @brief Receive the first available message from the two RX FIFOs
 */
bool CAN_getMessage(CAN_RxMessage_S* msg)
{
    memset(msg, 0x00, sizeof(CAN_RxMessage_S));

    for (CAN_RxFIFO_E fifo = 0U; fifo < CAN_RX_FIFO_COUNT; fifo++)
    {
        if (CAN_MSG_PENDING(fifo) != 0U)
        {
            msg->id               = (pCAN->rxFifos[fifo].RIR & CAN_RIR_STID) >> CAN_RIR_STID_OFFSET;
            msg->IDE              = (pCAN->rxFifos[fifo].RIR & CAN_RIR_IDE) >> CAN_RIR_IDE_OFFSET;
            msg->RTR              = (pCAN->rxFifos[fifo].RIR & CAN_RIR_RTR) >> CAN_RIR_RTR_OFFSET;
            msg->timestamp        = (pCAN->rxFifos[fifo].RDTR & CAN_RDTR_TIME) >> CAN_RDTR_TIME_OFFSET;
            msg->filterMatchIndex = (pCAN->rxFifos[fifo].RDTR & CAN_RDTR_FMI) >> CAN_RDTR_FMI_OFFSET;
            msg->lengthBytes      = (pCAN->rxFifos[fifo].RDTR & CAN_RDTR_DLC) >> CAN_RDTR_DLC_OFFSET;

            msg->data.u32[0]      = pCAN->rxFifos[fifo].RDLR;
            msg->data.u32[1]      = pCAN->rxFifos[fifo].RDHR;

            CAN_releaseRxFifo(fifo);
            CAN_messageRxAck(fifo);
            return true;
        }
    }

    return false;
}

/*
 * CAN_rxErrorAck
 * @brief Called by the application when the RX error has been processed
 * and it is safe to re-enable rx error interrupts
 */
void CAN_rxErrorAck(CAN_RxFIFO_E fifo)
{
    switch (fifo)
    {
        case CAN_RX_FIFO_0:
            SET_BIT(pCAN->IER, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO0_FULL | CAN_IT_RX_FIFO0_OVERRUN);
            break;

        case CAN_RX_FIFO_1:
            SET_BIT(pCAN->IER, CAN_IT_RX_FIFO1_MSG_PENDING | CAN_IT_RX_FIFO1_FULL | CAN_IT_RX_FIFO1_OVERRUN);
            break;

        default:
            break;
    }
}


/*
 * CAN_init
 * @brief Initialize the CAN peripheral and hardware
 */
CAN_exitCode_E CAN_init(void)
{
    // debug
    CAN.instance = pCAN;
    CAN.TSR      = (CAN_TSR_regMap*)&CAN.instance->TSR;

    uint32_t tick = 0U;

    CAN_hwInit();

    // Request peripheral initialisation
    SET_BIT(pCAN->MCR, CAN_MCR_INRQ);

    // Wait for initialisation req ack
    while ((pCAN->MSR & CAN_MSR_INAK) == 0U)
    {
        if (tick > CAN_TIMEOUT_VALUE)
        {
            return HAL_CAN_ERROR_NOT_READY;
        }
        tick++;
    }

    // Wake the peripheral
    CLEAR_BIT(pCAN->MCR, CAN_MCR_SLEEP);

    tick = 0U;

    // Wait for wake req ack
    while ((pCAN->MSR & CAN_MSR_SLAK) != 0U)
    {
        if (tick > CAN_TIMEOUT_VALUE)
        {
            return HAL_CAN_ERROR_NOT_READY;
        }
        tick++;
    }

    // enable automatic retransmission
    // disable time triggered communication mode
    // disable automatic wake-up mode
    // disable receive FIFO overrun locking
    // make TX FIFO prioritize mailboxes by message ID
    CLEAR_BIT(pCAN->MCR, CAN_MCR_NART
              | CAN_MCR_TTCM
              | CAN_MCR_AWUM
              | CAN_MCR_RFLM
              | CAN_MCR_TXFP);

    // enable automatic bus-off management
    SET_BIT(pCAN->MCR, CAN_MCR_ABOM);

    // Set the bit timing register
    WRITE_REG(pCAN->BTR, CAN_BTR_CONFIG);

    // enable interrupts
    WRITE_REG(pCAN->IER, GET_REG(pCAN->IER) | CAN_ENABLED_INTERRUPTS);

    // clear the initialization request bit to exit init mode
    // and allow the peripheral to begin operating
    CLEAR_BIT(pCAN->MCR, CAN_MCR_INRQ);

    // configure filters
    filterInit();

    while ((pCAN->MSR & CAN_MSR_INAK) != 0U)
    {
        // Check for the Timeout
        if (tick > CAN_TIMEOUT_VALUE)
        {
            return HAL_CAN_ERROR_NOT_READY;
        }
    }

    return HAL_CAN_ERROR_NONE;
}


/*
 * CAN_destroy
 * @brief: Disable CAN GPIO and Peripheral
 * No need to disable interrupts here, as that is done in NVIC_disableInterrupts
 */
void CAN_destroy(void)
{
    // Disable CAN1 clock
    pRCC->APB1ENR &= ~RCC_APB1ENR_CAN1_CLK;

    // Request peripheral reset
    SET_BIT(pCAN->MCR, CAN_MCR_RESET);

    // Set pins back to reset value (input floating)
    SET_REG(GPIO_CR(GPIOA, CAN_TX_PIN), (GET_REG(GPIO_CR(GPIOA, CAN_TX_PIN)) & CR_MASK(CAN_TX_PIN)) | CR_INPUT << CR_SHIFT(CAN_TX_PIN));
    SET_REG(GPIO_CR(GPIOA, CAN_RX_PIN), (GET_REG(GPIO_CR(GPIOA, CAN_RX_PIN)) & CR_MASK(CAN_RX_PIN)) | CR_INPUT << CR_SHIFT(CAN_RX_PIN));

    // clear odr bits
    SET_REG(GPIO_ODR(GPIOA), GET_REG(GPIO_ODR(GPIOA)) & ~CAN_TX_PIN);
    SET_REG(GPIO_ODR(GPIOA), GET_REG(GPIO_ODR(GPIOA)) & ~CAN_RX_PIN);
}


/******************************************************************************
 *                    I N T E R R U P T   H A N D L E R S
 ******************************************************************************/

// these are defined in c_only_startup.S
// they are part of the interrupt vector table
extern void CAN_TX_IRQHandler(void);
extern void CAN_RX0_IRQHandler(void);
extern void CAN_RX1_IRQHandler(void);
extern void CAN_SCE_IRQHandler(void);


// handle tx interrupt
void CAN_TX_IRQHandler(void)
{
    uint32_t          errorCode  = HAL_CAN_ERROR_NONE;
    volatile uint32_t interrupts = READ_REG(pCAN->IER);
    volatile uint32_t tsrflags   = READ_REG(pCAN->TSR);


    // Transmit Mailbox empty interrupt management ****************************
    if ((interrupts & CAN_IT_TX_MAILBOX_EMPTY) != 0U)
    {
        // Transmit Mailbox 0 management ****************************************
        if ((tsrflags & CAN_TSR_RQCP0) != 0U)
        {
            // Clear the Transmission Complete flag (and TXOK0,ALST0,TERR0 bits)
            __HAL_CAN_CLEAR_FLAG(CAN_FLAG_RQCP0);

            if ((tsrflags & CAN_TSR_TXOK0) != 0U)
            {
                // Transmission complete callback
                CAN_CB_txComplete(CAN_TX_MAILBOX_0);
            }
            else
            {
                if ((tsrflags & CAN_TSR_ALST0) != 0U)
                {
                    // Update error code
                    errorCode |= HAL_CAN_ERROR_TX_ALST0;
                }
                else if ((tsrflags & CAN_TSR_TERR0) != 0U)
                {
                    // Update error code
                    errorCode |= HAL_CAN_ERROR_TX_TERR0;
                }
                else
                {
                    // Transmission abort callback
                    CAN_CB_txAbort(CAN_TX_MAILBOX_0);
                }

                if (errorCode != HAL_CAN_ERROR_NONE)
                {
                    CAN_CB_txError(CAN_TX_MAILBOX_0, errorCode);
                }
            }
        }

        errorCode = HAL_CAN_ERROR_NONE;
        // Transmit Mailbox 1 management ****************************************
        if ((tsrflags & CAN_TSR_RQCP1) != 0U)
        {
            // Clear the Transmission Complete flag (and TXOK1,ALST1,TERR1 bits)
            __HAL_CAN_CLEAR_FLAG(CAN_FLAG_RQCP1);

            if ((tsrflags & CAN_TSR_TXOK1) != 0U)
            {
                // Transmission complete callback
                CAN_CB_txComplete(CAN_TX_MAILBOX_1);
            }
            else
            {
                if ((tsrflags & CAN_TSR_ALST1) != 0U)
                {
                    // Update error code
                    errorCode |= HAL_CAN_ERROR_TX_ALST1;
                }
                else if ((tsrflags & CAN_TSR_TERR1) != 0U)
                {
                    // Update error code
                    errorCode |= HAL_CAN_ERROR_TX_TERR1;
                }
                else
                {
                    // Transmission abort callback
                    CAN_CB_txAbort(CAN_TX_MAILBOX_1);
                }

                if (errorCode != HAL_CAN_ERROR_NONE)
                {
                    CAN_CB_txError(CAN_TX_MAILBOX_1, errorCode);
                }
            }
        }

        errorCode = HAL_CAN_ERROR_NONE;
        // Transmit Mailbox 2 management ****************************************
        if ((tsrflags & CAN_TSR_RQCP2) != 0U)
        {
            // Clear the Transmission Complete flag (and TXOK2,ALST2,TERR2 bits)
            __HAL_CAN_CLEAR_FLAG(CAN_FLAG_RQCP2);

            if ((tsrflags & CAN_TSR_TXOK2) != 0U)
            {
                // Transmission complete callback
                CAN_CB_txComplete(CAN_TX_MAILBOX_2);
            }
            else
            {
                if ((tsrflags & CAN_TSR_ALST2) != 0U)
                {
                    // Update error code
                    errorCode |= HAL_CAN_ERROR_TX_ALST2;
                }
                else if ((tsrflags & CAN_TSR_TERR2) != 0U)
                {
                    // Update error code
                    errorCode |= HAL_CAN_ERROR_TX_TERR2;
                }
                else
                {
                    // Transmission Mailbox 2 abort callback
                    CAN_CB_txAbort(CAN_TX_MAILBOX_2);
                }

                if (errorCode != HAL_CAN_ERROR_NONE)
                {
                    CAN_CB_txError(CAN_TX_MAILBOX_2, errorCode);
                }
            }
        }
    }
}

void CAN_RX0_IRQHandler(void)
{
    uint32_t          errorCode  = HAL_CAN_ERROR_NONE;
    volatile uint32_t interrupts = READ_REG(pCAN->IER);
    volatile uint32_t rf0rflags  = READ_REG(pCAN->RF0R);

    // Receive FIFO 0 overrun interrupt management ****************************
    if ((interrupts & CAN_IT_RX_FIFO0_OVERRUN) != 0U)
    {
        if ((rf0rflags & CAN_RF0R_FOVR0) != 0U)
        {
            // disable interrupts once the fifo is full so we don't get stuck
            CLEAR_BIT(pCAN->IER, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO0_OVERRUN);

            // Set CAN error code to Rx Fifo 0 overrun error
            errorCode |= HAL_CAN_ERROR_RX_FOV0;

            // Clear FIFO0 Overrun Flag
            __HAL_CAN_CLEAR_FLAG(CAN_FLAG_FOV0);
        }
    }

    // Receive FIFO 0 full interrupt management *******************************
    if ((interrupts & CAN_IT_RX_FIFO0_FULL) != 0U)
    {
        if ((rf0rflags & CAN_RF0R_FULL0) != 0U)
        {
            // disable interrupt until acked
            CLEAR_BIT(pCAN->IER, CAN_IT_RX_FIFO0_FULL);

            // Clear FIFO 0 full Flag
            __HAL_CAN_CLEAR_FLAG(CAN_FLAG_FF0);
        }
    }

    // Receive FIFO 0 message pending interrupt management ********************
    if ((interrupts & CAN_IT_RX_FIFO0_MSG_PENDING) != 0U)
    {
        // Check if message is still pending
        if ((pCAN->RF0R & CAN_RF0R_FMP0) != 0U)
        {
            CLEAR_BIT(pCAN->IER, CAN_IT_RX_FIFO0_MSG_PENDING);
            // Receive FIFO 0 message pending Callback
            CAN_CB_messageRx(CAN_RX_FIFO_0);
        }
    }

    if (errorCode != HAL_CAN_ERROR_NONE)
    {
        CAN_CB_rxError(CAN_RX_FIFO_0, errorCode);
    }
}

void CAN_RX1_IRQHandler(void)
{
    uint32_t          errorCode  = HAL_CAN_ERROR_NONE;
    volatile uint32_t interrupts = READ_REG(pCAN->IER);
    volatile uint32_t rf1rflags  = READ_REG(pCAN->RF1R);

    UNUSED(errorCode);

    CAN.bit.rx1Interrupt = 1;

    // Receive FIFO 1 overrun interrupt management ****************************
    if ((interrupts & CAN_IT_RX_FIFO1_OVERRUN) != 0U)
    {
        if ((rf1rflags & CAN_RF1R_FOVR1) != 0U)
        {
            // disable interrupts once the fifo is full so we don't get stuck
            CLEAR_BIT(pCAN->IER, CAN_IT_RX_FIFO1_MSG_PENDING | CAN_IT_RX_FIFO1_OVERRUN);

            // Set CAN error code to Rx Fifo 1 overrun error
            errorCode |= HAL_CAN_ERROR_RX_FOV1;

            // Clear FIFO1 Overrun Flag
            __HAL_CAN_CLEAR_FLAG(CAN_FLAG_FOV1);
        }
    }

    // Receive FIFO 1 full interrupt management *******************************
    if ((interrupts & CAN_IT_RX_FIFO1_FULL) != 0U)
    {
        if ((rf1rflags & CAN_RF1R_FULL1) != 0U)
        {
            // disable interrupts once the fifo is full so we don't get stuck
            CLEAR_BIT(pCAN->IER, CAN_IT_RX_FIFO1_FULL);

            // Clear FIFO 1 full Flag
            __HAL_CAN_CLEAR_FLAG(CAN_FLAG_FF1);
        }
    }

    // Receive FIFO 1 message pending interrupt management ********************
    if ((interrupts & CAN_IT_RX_FIFO1_MSG_PENDING) != 0U)
    {
        // Check if message is still pending
        if ((pCAN->RF1R & CAN_RF1R_FMP1) != 0U)
        {
            CLEAR_BIT(pCAN->IER, CAN_IT_RX_FIFO1_MSG_PENDING);

            // Receive FIFO 1 message pending Callback
            CAN_CB_messageRx(CAN_RX_FIFO_1);
        }
    }
    if (errorCode != HAL_CAN_ERROR_NONE)
    {
        CAN_CB_rxError(CAN_RX_FIFO_1, errorCode);
    }
}

void CAN_SCE_IRQHandler(void)
{
    uint32_t          errorCode  = HAL_CAN_ERROR_NONE;
    volatile uint32_t interrupts = READ_REG(pCAN->IER);
    volatile uint32_t msrflags   = READ_REG(pCAN->MSR);
    volatile uint32_t esrflags   = READ_REG(pCAN->ESR);


    // Sleep interrupt management ********************************************
    if ((interrupts & CAN_IT_SLEEP_ACK) != 0U)
    {
        if ((msrflags & CAN_MSR_SLAKI) != 0U)
        {
            // Clear Sleep interrupt Flag
            __HAL_CAN_CLEAR_FLAG(CAN_FLAG_SLAKI);

            // Sleep Callback
            // HAL_CAN_SleepCallback(hcan);
        }
    }

    // WakeUp interrupt management ********************************************
    if ((interrupts & CAN_IT_WAKEUP) != 0U)
    {
        if ((msrflags & CAN_MSR_WKUI) != 0U)
        {
            // Clear WakeUp Flag
            __HAL_CAN_CLEAR_FLAG(CAN_FLAG_WKU);

            // WakeUp Callback
            // HAL_CAN_WakeUpFromRxMsgCallback(hcan);
        }
    }

    // Error interrupts management ********************************************
    if ((interrupts & CAN_IT_ERROR) != 0U)
    {
        errorCode |= CAN_IT_ERROR;
        if ((msrflags & CAN_MSR_ERRI) != 0U)
        {
            // Check Error Warning Flag
            if (((interrupts & CAN_IT_ERROR_WARNING) != 0U) &&
                ((esrflags & CAN_ESR_EWGF) != 0U)
                )
            {
                // Set CAN error code to Error Warning
                errorCode |= HAL_CAN_ERROR_EWG;

                // No need for clear of Error Warning Flag as read-only
            }

            // Check Error Passive Flag
            if (((interrupts & CAN_IT_ERROR_PASSIVE) != 0U) &&
                ((esrflags & CAN_ESR_EPVF) != 0U)
                )
            {
                // Set CAN error code to Error Passive
                errorCode |= HAL_CAN_ERROR_EPV;

                // No need for clear of Error Passive Flag as read-only
            }

            // Check Bus-off Flag
            if (((interrupts & CAN_IT_BUSOFF) != 0U) &&
                ((esrflags & CAN_ESR_BOFF) != 0U)
                )
            {
                // Set CAN error code to Bus-Off
                errorCode |= HAL_CAN_ERROR_BOF;

                // No need for clear of Error Bus-Off as read-only
            }

            // Check Last Error Code Flag
            if (((interrupts & CAN_IT_LAST_ERROR_CODE) != 0U) &&
                ((esrflags & CAN_ESR_LEC) != 0U)
                )
            {
                switch (esrflags & CAN_ESR_LEC)
                {
                    case (CAN_ESR_LEC_0):
                        // Set CAN error code to Stuff error
                        errorCode |= HAL_CAN_ERROR_STF;
                        break;

                    case (CAN_ESR_LEC_1):
                        // Set CAN error code to Form error
                        errorCode |= HAL_CAN_ERROR_FOR;
                        break;

                    case (CAN_ESR_LEC_1 | CAN_ESR_LEC_0):
                        // Set CAN error code to Acknowledgement error
                        errorCode |= HAL_CAN_ERROR_ACK;
                        break;

                    case (CAN_ESR_LEC_2):
                        // Set CAN error code to Bit recessive error
                        errorCode |= HAL_CAN_ERROR_BR;
                        break;

                    case (CAN_ESR_LEC_2 | CAN_ESR_LEC_0):
                        // Set CAN error code to Bit Dominant error
                        errorCode |= HAL_CAN_ERROR_BD;
                        break;

                    case (CAN_ESR_LEC_2 | CAN_ESR_LEC_1):
                        // Set CAN error code to CRC error
                        errorCode |= HAL_CAN_ERROR_CRC;
                        break;

                    default:
                        break;
                }

                // Clear Last error code Flag
                CLEAR_BIT(pCAN->ESR, CAN_ESR_LEC);
            }
        }

        // Clear ERRI Flag
        __HAL_CAN_CLEAR_FLAG(CAN_FLAG_ERRI);
    }

    if (errorCode != HAL_CAN_ERROR_NONE)
    {
        CAN_CB_error(errorCode);
    }
}
