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
#include "IO.h"
#include "BMS.h"
#include "PACK.h"
#include "Sys.h"
#include "IMD.h"
#include "ENV.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define MSG_packTable_100Hz_SIZE (sizeof(MSG_packTable_100Hz) / sizeof(packTable_S))
#define MSG_packTable_10Hz_SIZE  (sizeof(MSG_packTable_10Hz) / sizeof(packTable_S))
#define MSG_packTable_1Hz_SIZE   (sizeof(MSG_packTable_1Hz) / sizeof(packTable_S))

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

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
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static cantx_S cantx;

static const packTable_S* packNextMessage(const packTable_S* packTable,
                                          const uint8_t      packTableLength,
                                          uint8_t*     index,
                                          CAN_data_T*        message,
                                          uint8_t*           nextCounter);

static bool MSG_pack_BMS_100Hz_Critical(CAN_data_T* message, const uint8_t counter);
static bool MSG_pack_BMS_100Hz_Critical_Charger(CAN_data_T* message, const uint8_t counter);
static bool MSG_pack_BMS_1Hz_Balancing_Command(CAN_data_T* message, const uint8_t counter);
static bool MSG_pack_BMS_1Hz_Environment(CAN_data_T* message, const uint8_t counter);

static const packTable_S MSG_packTable_100Hz[] = {
    { &MSG_pack_BMS_100Hz_Critical, 0x99, 5U },
    { &MSG_pack_BMS_100Hz_Critical_Charger, 0x618, 7U },
};
static const packTable_S MSG_packTable_10Hz[] = {
};
static const packTable_S MSG_packTable_1Hz[] = {
    { &MSG_pack_BMS_1Hz_Balancing_Command, 0x300, 2U },
    { &MSG_pack_BMS_1Hz_Environment, 0x301, 5U },
};

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * CANTX_BUS_A_1ms_SWI
 * send BUS_A messages
 */
void CANTX_BUS_A_SWI(void)
{
    if (cantx.tx_100Hz_msg != MSG_packTable_100Hz_SIZE)
    {
        static uint8_t counter_100Hz = 0U;
        CAN_data_T     message_100Hz = {0};

        const packTable_S* entry_100Hz = packNextMessage((const packTable_S*)&MSG_packTable_100Hz,
                                                        MSG_packTable_100Hz_SIZE,
                                                        &cantx.tx_100Hz_msg,
                                                        &message_100Hz,
                                                        &counter_100Hz);

        if (entry_100Hz != NULL)
        {
            if (CAN_sendMsgBus0(CAN_TX_PRIO_100HZ, message_100Hz, entry_100Hz->id, entry_100Hz->len))
            {
                cantx.tx_100Hz_msg++;
            }
        }
        
        if (cantx.tx_100Hz_msg == MSG_packTable_100Hz_SIZE)
        {
            counter_100Hz++;
        }
    }

    if (cantx.tx_10Hz_msg != MSG_packTable_10Hz_SIZE)
    {
        static uint8_t counter_10Hz = 0U;
        CAN_data_T     message_10Hz = {0};

        const packTable_S* entry_10Hz = packNextMessage((const packTable_S*)&MSG_packTable_10Hz,
                                                        MSG_packTable_10Hz_SIZE,
                                                        &cantx.tx_10Hz_msg,
                                                        &message_10Hz,
                                                        &counter_10Hz);

        if (entry_10Hz != NULL)
        {
            if (CAN_sendMsgBus0(CAN_TX_PRIO_10HZ, message_10Hz, entry_10Hz->id, entry_10Hz->len))
            {
                cantx.tx_10Hz_msg++;
            }
        }
        
        if (cantx.tx_10Hz_msg == MSG_packTable_10Hz_SIZE)
        {
            counter_10Hz++;
        }
    }
    
    if (cantx.tx_1Hz_msg != MSG_packTable_1Hz_SIZE)
    {
        static uint8_t counter_1Hz = 0U;
        CAN_data_T     message_1Hz = {0};

        const packTable_S* entry_1Hz = packNextMessage((const packTable_S*)&MSG_packTable_1Hz,
                                                    MSG_packTable_1Hz_SIZE,
                                                    &cantx.tx_1Hz_msg,
                                                    &message_1Hz,
                                                    &counter_1Hz);

        if (entry_1Hz != NULL)
        {
            if (CAN_sendMsgBus0(CAN_TX_PRIO_1HZ, message_1Hz, entry_1Hz->id, entry_1Hz->len))
            {
                cantx.tx_1Hz_msg++;
            }
        }
        
        if (cantx.tx_1Hz_msg == MSG_packTable_1Hz_SIZE)
        {
            counter_1Hz++;
        }
    }
}


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static const packTable_S* packNextMessage(const packTable_S* packTable,
                                          const uint8_t      packTableLength,
                                          uint8_t*     index,
                                          CAN_data_T*        message,
                                          uint8_t*           nextCounter)
{
    while (*index < packTableLength)
    {
        const packTable_S* entry   = &packTable[*index];
        uint16_t           counter = *nextCounter;

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

static bool MSG_pack_BMS_100Hz_Critical(CAN_data_T* message, const uint8_t counter)
{
    message->u64 = 0x00;
    message->u8[0] = counter;
    message->u8[1] = BMS.pack_charge_limit;
    message->u8[2] = BMS.pack_discharge_limit;
    message->u8[3] = BMS.pack_voltage * 10;
    message->u8[4] = (uint16_t)(BMS.pack_voltage * 10) >> 8;
    return true;
}

static bool MSG_pack_BMS_100Hz_Critical_Charger(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    message->u64 = 0x00;
    static bool started = false;

    if (SYS_SFT_checkChargerTimeout() || BMS.charging_paused)
    {
        started = false;
	    return false;
    }

    switch (SYS.contacts)
    {
	case SYS_CONTACTORS_HVP_CLOSED:
	    message->u8[0] = (started) ? 0x80 : 0x40;
	    message->u8[1] = (uint16_t)(160) >> 8;
	    message->u8[2] = (uint16_t)(160);
	    message->u8[3] = (uint16_t)(BMS_PACK_VOLTAGE_MAX * 10) >> 8;
	    message->u8[4] = (uint16_t)(BMS_PACK_VOLTAGE_MAX * 10) & 0xff;
	    message->u8[5] = (uint16_t)(BMS.pack_charge_limit * 10) >> 8;
	    message->u8[6] = (uint16_t)(BMS.pack_charge_limit * 10) & 0xff;
        started = true;
	    return true;
	default:
        started = false;
	    return false;
    }
}

static bool MSG_pack_BMS_1Hz_Balancing_Command(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    message->u64 = 0x00;
    
    return false;

    if (SYS_SFT_checkChargerTimeout())
    {
	return false;
    }

    switch (SYS.contacts)
    {
	case SYS_CONTACTORS_HVP_CLOSED:
	    message->u16[0] = BMS.voltages.min * 200;
	    return true;
	default:
	    return false;
    }
}

static bool MSG_pack_BMS_1Hz_Environment(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    message->u64 = 0x00;
    
    message->u16[0] = IMD_getIsolation();
    message->u8[2] = ENV.board.rh;
    message->u8[3] = ENV.board.ambient_temp;
    message->u8[4] = ENV.board.mcu_temp;

    return true;
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
static void CANIO_tx_100Hz_PRD(void)
{
    if (cantx.tx_100Hz_msg < MSG_packTable_100Hz_SIZE) {
        // all the message weren't sent. TO-DO: error handling
    }
    cantx.tx_100Hz_msg = 0U;
}

/**
 * CANIO_tx_10Hz_PRD
 *
 */
static void CANIO_tx_10Hz_PRD(void)
{
    if (cantx.tx_10Hz_msg < MSG_packTable_10Hz_SIZE) {
        // all the message weren't sent. TO-DO: error handling
    }
    cantx.tx_10Hz_msg = 0U;
}

static void CANIO_tx_1Hz_PRD(void)
{
    if (cantx.tx_1Hz_msg < MSG_packTable_1Hz_SIZE) {
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
    HW_CAN_start();    // start CAN
}


const ModuleDesc_S CANIO_tx = {
    .moduleInit        = &CANIO_tx_init,
    .periodic1kHz_CLK  = &CANIO_tx_1kHz_PRD,
    .periodic100Hz_CLK = &CANIO_tx_100Hz_PRD,
    .periodic10Hz_CLK  = &CANIO_tx_10Hz_PRD,
    .periodic1Hz_CLK   = &CANIO_tx_1Hz_PRD,
};

