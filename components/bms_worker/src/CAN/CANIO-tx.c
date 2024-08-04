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
#include "HW_tim.h"
#include "Module.h"
#include "ModuleDesc.h"

// imports for data access
#include "BuildDefines_generated.h"
#include "MessagePack_generated.c"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint8_t tx_1Hz_msg;
    uint8_t tx_10Hz_msg;
} cantx_S;


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define MSG_UID_SEGMENT(id) (id + CAN_BASE_OFFSET)

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static cantx_S            cantx;
extern Module_taskStats_S swi_stats;


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

static const packTable_S* packNextMessage(const packTable_S* packTable,
                                          const uint8_t    packTableLength,
                                          uint8_t          * index,
                                          CAN_data_T       * message,
                                          uint8_t          * nextCounter);

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * CANTX_BUS_A_1ms_SWI
 * send BUS_A messages
 */
void CANTX_BUS_A_SWI(void)
{
    static uint8_t   counter_100Hz = 0U;
    static uint8_t   counter_1Hz = 0U;
    CAN_data_T       message = { 0U };

    if (cantx.tx_10Hz_msg != VEH_packTable_10_length)
    {

        const packTable_S* entry_100Hz = packNextMessage((const packTable_S*)&VEH_packTable_10ms,
                                                         VEH_packTable_10_length,
                                                         &cantx.tx_10Hz_msg,
                                                         &message,
                                                         &counter_100Hz);

        if (entry_100Hz != NULL)
        {
            if (CAN_sendMsgBus0(CAN_TX_PRIO_100HZ, message, MSG_UID_SEGMENT(entry_100Hz->id), entry_100Hz->len))
            {
                cantx.tx_10Hz_msg++;
            }
            memset(&message, 0, sizeof(message));
        }
    }

    if (cantx.tx_1Hz_msg != VEH_packTable_1000_length)
    {

        const packTable_S* entry_1Hz = packNextMessage((const packTable_S*)&VEH_packTable_1000ms,
                                                       VEH_packTable_1000_length,
                                                       &cantx.tx_1Hz_msg,
                                                       &message,
                                                       &counter_1Hz);

        if (entry_1Hz != NULL)
        {
            if (CAN_sendMsgBus0(CAN_TX_PRIO_1HZ, message, MSG_UID_SEGMENT(entry_1Hz->id), entry_1Hz->len))
            {
                cantx.tx_1Hz_msg++;
            }
            memset(&message, 0, sizeof(message));
        }
    }
}


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static const packTable_S* packNextMessage(const packTable_S* packTable,
                                          const uint8_t    packTableLength,
                                          uint8_t          * index,
                                          CAN_data_T       * message,
                                          uint8_t          * nextCounter)
{
    while (*index < packTableLength)
    {
        const packTable_S* entry = &packTable[*index];
        uint16_t         counter = *nextCounter;

        message->u64 = 0ULL;
        if ((*entry->pack)(message, counter))
        {
            return entry;
        }
        else
        {
            (*index)++;
        }
    }

    if (*index == packTableLength)
    {
        (*nextCounter)++;
    }

    return NULL;
}

static void CANIO_tx_1kHz_PRD(void)
{
    //SWI_invoke(CANTX_BUS_A_swi);
    CANTX_BUS_A_SWI();
}

/**
 * CANIO_tx_100Hz_PRD
 * module 100Hz periodic function
 */
static void CANIO_tx_10Hz_PRD(void)
{
    if (cantx.tx_10Hz_msg < VEH_packTable_10_length)
    {
        // all the message weren't sent. TO-DO: error handling
    }
    cantx.tx_10Hz_msg = 0U;
}

/**
 * CANIO_tx_1Hz_PRD
 *
 */
static void CANIO_tx_1Hz_PRD(void)
{
    if (cantx.tx_1Hz_msg < VEH_packTable_1000_length)
    {
        // all the message weren't sent. TO-DO: error handling
    }
    cantx.tx_1Hz_msg = 0U;
}

/**
 * CANIO_tx_init
 * initialize module
 */
static void CANIO_tx_init(void)
{
    memset(&cantx, 0x00, sizeof(cantx));
    cantx.tx_1Hz_msg   = VEH_packTable_1000_length;
    cantx.tx_10Hz_msg = VEH_packTable_10_length;
    HW_CAN_start();    // start CAN
}


const ModuleDesc_S CANIO_tx = {
    .moduleInit        = &CANIO_tx_init,
 // .periodic10kHz_CLK = &CANIO_tx_10kHz_PRD,
    .periodic1kHz_CLK  = &CANIO_tx_1kHz_PRD,
    .periodic10Hz_CLK = &CANIO_tx_10Hz_PRD,
    .periodic1Hz_CLK   = &CANIO_tx_1Hz_PRD,
};
