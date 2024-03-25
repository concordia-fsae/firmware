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
#include "ModuleDesc.h"

#include "string.h"

// imports for data access
#include "BatteryMonitoring.h"
#include "IO.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint8_t tx_1Hz_msg;
    uint8_t tx_10Hz_msg;
    uint8_t tx_100Hz_msg;
} cantx_S;


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define MSG_packTable_100Hz_SIZE (sizeof(MSG_packTable_100Hz) / sizeof(packTable_S))
#define MSG_packTable_10Hz_SIZE  (sizeof(MSG_packTable_10Hz) / sizeof(packTable_S))
#define MSG_packTable_1Hz_SIZE   (sizeof(MSG_packTable_1Hz) / sizeof(packTable_S))

#define MSG_UID_SEGMENT(id) (id + IO.addr)


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static cantx_S cantx;

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

static const packTable_S* packNextMessage(const packTable_S* packTable,
                                          const uint8_t      packTableLength,
                                          uint8_t*           index,
                                          CAN_data_T*        message,
                                          uint8_t*           nextCounter);

static bool MSG_pack_BMS_100Hz(CAN_data_T* message, const uint8_t counter);
static bool MSG_pack_BMS_100Hz1(CAN_data_T* message, const uint8_t counter);
//static bool MSG_pack_BMS_10Hz(CAN_data_T* message, const uint8_t counter);
//static bool MSG_pack_BMS_1Hz(CAN_data_T* message, const uint8_t counter);

static const packTable_S MSG_packTable_100Hz[] = {
    //{ &MSG_pack_BMS_100Hz, 0x100, 8U },
    //{ &MSG_pack_BMS_100Hz1, 0x110, 8U },
};
static const packTable_S MSG_packTable_10Hz[] = {
    //{ &MSG_pack_BMS_10Hz, 0x10, 8U },
};
static const packTable_S MSG_packTable_1Hz[] = {
    //{ &MSG_pack_BMS_1Hz, 0x1, 8U },
    { &MSG_pack_BMS_100Hz, 0x100, 8U },
    { &MSG_pack_BMS_100Hz1, 0x110, 8U },
};


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * CANTX_BUS_A_10ms_SWI
 * send BUS_A messages
 */
void CANTX_BUS_A_100Hz_SWI(void)
{
    // TODO: add overrun detection here
    if (cantx.tx_100Hz_msg == MSG_packTable_100Hz_SIZE)
    {
        return;
    }

    static uint8_t counter = 0U;
    CAN_data_T     message;

    const packTable_S* entry = packNextMessage((const packTable_S*)&MSG_packTable_100Hz,
                                               MSG_packTable_100Hz_SIZE,
                                               &cantx.tx_100Hz_msg,
                                               &message,
                                               &counter);

    if (entry != NULL)
    {
        CAN_sendMsgBus0(CAN_TX_PRIO_100HZ, message, MSG_UID_SEGMENT(entry->id), entry->len);
        return;
    }
}

void CANTX_BUS_A_10Hz_SWI(void)
{
    // TODO: add overrun detection here
    if (cantx.tx_10Hz_msg == MSG_packTable_10Hz_SIZE)
    {
        return;
    }

    static uint8_t counter = 0U;
    CAN_data_T     message;

    const packTable_S* entry = packNextMessage((const packTable_S*)&MSG_packTable_10Hz,
                                               MSG_packTable_10Hz_SIZE,
                                               &cantx.tx_10Hz_msg,
                                               &message,
                                               &counter);

    if (entry != NULL)
    {
        CAN_sendMsgBus0(CAN_TX_PRIO_10HZ, message, MSG_UID_SEGMENT(entry->id), entry->len);
        return;
    }
}

void CANTX_BUS_A_1Hz_SWI(void)
{
    // TODO: add overrun detection here
    if (cantx.tx_1Hz_msg == MSG_packTable_1Hz_SIZE)
    {
        return;
    }

    static uint8_t counter = 0U;
    CAN_data_T     message;

    const packTable_S* entry = packNextMessage((const packTable_S*)&MSG_packTable_1Hz,
                                               MSG_packTable_1Hz_SIZE,
                                               &cantx.tx_1Hz_msg,
                                               &message,
                                               &counter);

    if (entry != NULL)
    {
        CAN_sendMsgBus0(CAN_TX_PRIO_1HZ, message, MSG_UID_SEGMENT(entry->id), entry->len);
        return;
    }
}

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static const packTable_S* packNextMessage(const packTable_S* packTable,
                                          const uint8_t      packTableLength,
                                          uint8_t*           index,
                                          CAN_data_T*        message,
                                          uint8_t*           nextCounter)
{
    while (*index < packTableLength)
    {
        const packTable_S* entry   = &packTable[(*index)++];
        uint16_t           counter = *nextCounter;
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

static bool MSG_pack_BMS_100Hz(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    message->u16[0] = BMS.voltage.min;
    message->u16[1] = BMS.voltage.max;
    message->u16[2] = BMS.voltage.avg;
    return true;
}

static bool MSG_pack_BMS_100Hz1(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    message->u16[0] = BMS.relativeSoC.min;
    message->u16[1] = BMS.relativeSoC.max;
    message->u16[2] = BMS.relativeSoC.avg;
    return true;
}

//static bool MSG_pack_BMS_10Hz(CAN_data_T* message, const uint8_t counter)
//{
//    message->u64    = 0x10;
//    message->u32[1] = counter;
//    return true;
//}
//
//static bool MSG_pack_BMS_1Hz(CAN_data_T* message, const uint8_t counter)
//{
//    message->u64    = 0x1;
//    message->u32[1] = counter;
//    return true;
//}

static void CANIO_tx_1kHz_PRD(void)
{
    // transmit 100Hz messages
    cantx.tx_100Hz_msg = 0U;
    SWI_invoke(CANTX_BUS_A_100Hz_swi);
}

/**
 * CANIO_tx_100Hz_PRD
 * module 100Hz periodic function
 */
static void CANIO_tx_100Hz_PRD(void)
{
    // transmit 100Hz messages
    cantx.tx_100Hz_msg = 0U;
    SWI_invoke(CANTX_BUS_A_100Hz_swi);
}

/**
 * CANIO_tx_10Hz_PRD
 *
 */
static void CANIO_tx_10Hz_PRD(void)
{
    cantx.tx_10Hz_msg = 0U;
    SWI_invoke(CANTX_BUS_A_10Hz_swi);
}

static void CANIO_tx_1Hz_PRD(void)
{
    cantx.tx_1Hz_msg = 0U;
    SWI_invoke(CANTX_BUS_A_1Hz_swi);
}

/**
 * CANIO_tx_init
 * initialize module
 */
static void CANIO_tx_init(void)
{
    memset(&cantx, 0x00, sizeof(cantx));
    HW_CAN_start();    // start CAN
}


const ModuleDesc_S CANIO_tx = {
    .moduleInit        = &CANIO_tx_init,
    .periodic1kHz_CLK = &CANIO_tx_1kHz_PRD,
    .periodic100Hz_CLK = &CANIO_tx_100Hz_PRD,
    .periodic10Hz_CLK  = &CANIO_tx_10Hz_PRD,
    .periodic1Hz_CLK   = &CANIO_tx_1Hz_PRD,
};
