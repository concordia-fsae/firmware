/*
 * CANIO-tx.c
 * CAN Transmit
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CAN/CAN.h"

#include "HW_can.h"

#include "FreeRTOS_SWI.h"
#include "MessagePack.h"
#include "ModuleDesc.h"
#include "Screen.h"
#include "VEH_sigTx.h"

#include "Display/Common.h"

#include "string.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define packTableName(bus, name)                            SNAKE3(bus, packTable, name)
#define packTableLength(bus, name)                          SNAKE4(bus, packTable, name, length)
#define packNextBusMessage(bus, name, idx, msg, counter)    packNextMessage(packTableName(bus, name), packTableLength(bus, name), idx, msg, counter)


#define set_raw(msg, bus, node, signal, val)                 SNAKE4(setRaw, bus, node, signal)(msg, val)
#define set_value(msg, bus, node, signal, val)               SNAKE4(set, bus, node, signal)(msg, val)
#define set_scale(msg, bus, node, signal, val, base, off)    SNAKE4(set, bus, node, sig)(msg, val, base, off)
#define unsent_signal(m)                                     UNUSED(m)


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint8_t txBusA10msIdx;
} canio_tx_S;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static canio_tx_S canio_tx;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

// signal packing functions

#define set_switch0Status(m, b, n, s)    set_value(m, b, n, s, 0U);
#define set_switch1Status(m, b, n, s)    set_value(m, b, n, s, 1U);
#define set_switch3Status(m, b, n, s)    set_value(m, b, n, s, 1U);
#define set_switch4Status(m, b, n, s)    set_value(m, b, n, s, 0U);
#define set_button0Status(m, b, n, s)    set_value(m, b, n, s, 0U);
#define set_button1Status(m, b, n, s)    set_value(m, b, n, s, 1U);

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

// TODO: the following file should be auto-generated
// NOTE: must be included after signal packing functions are defined
#include "MessagePack.c"

/**
 * packNextMessage
 * @param packTable packTable to send message from
 * @param packTableLength length of packTable
 * @param index current index
 * @param message message reference
 * @param nextCounter counter value for message
 * @return current packTable entry
 */
static const packTable_S* packNextMessage(const packTable_S *packTable,
                                          const uint8_t packTableLength,
                                          uint8_t *index,
                                          CAN_data_T *message,
                                          uint8_t *nextCounter)
{
    while (*index < packTableLength)
    {
        const packTable_S *entry  = &packTable[(*index)++];
        uint16_t          counter = *nextCounter;
        if (*index == packTableLength)
        {
            (*nextCounter)++;
        }
        message->u64 = 0ULL;
        if ((*entry->pack)(message, counter))
        {
            return entry;
        }
    }
    return NULL;
}


/**
 * CAN_BUS_A_10ms_SWI
 * send BUS_A messages
 */
void CAN_BUS_A_10ms_SWI(void)
{
    // TODO: add overrun detection here

    static uint8_t    counter = 0U;
    CAN_data_T        message;

    const packTable_S *entry = packNextBusMessage(BUS_A, 10ms, &canio_tx.txBusA10msIdx, &message, &counter);

    if (entry != NULL)
    {
        CAN_sendMsgBus0(CAN_TX_PRIO_100HZ, message, entry->id, entry->len);
        return;
    }
}


/**
 * CANIO_tx_100Hz_PRD
 * module 100Hz periodic function
 */
static void CANIO_tx_100Hz_PRD(void)
{
    static uint8_t tim = 0;

    if (tim++ == 9U)
    {
        tim = 0;
        toggleInfoDotState(INFO_DOT_CAN_TX);
    }

    // transmit 100Hz messages
    canio_tx.txBusA10msIdx = 0U;
    SWI_invoke(CAN_BUS_A_10ms_swi);
}

/**
 * CANIO_tx_10Hz_PRD
 *
 */
static void CANIO_tx_10Hz_PRD(void)
{
    // CAN_sendMsgBus0(CAN_TX_PRIO_10HZ, (CAN_data_T){0x01}, 0x101, 8U);
}

/**
 * CANIO_tx_init
 * initialize module
 */
static void CANIO_tx_init(void)
{
    memset(&canio_tx, 0x00, sizeof(canio_tx));
    CAN_Start();    // start CAN
}


const ModuleDesc_S CANIO_tx = {
    .moduleInit        = &CANIO_tx_init,
    .periodic100Hz_CLK = &CANIO_tx_100Hz_PRD,
    .periodic10Hz_CLK  = &CANIO_tx_10Hz_PRD,
};

