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

#include "SigTx.c"
#include "MessageUnpack_generated.h"
#include "CANTypes_generated.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint8_t tx_100Hz_msg;
} cantx_S;

typedef struct
{
    uint8_t counter_100Hz;
} cantx_counter_S;

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

static const packTable_S* packNextMessage(const packTable_S* packTable,
                                          const uint8_t      packTableLength,
                                          uint8_t*           index,
                                          CAN_data_T*        message,
                                          uint8_t*           nextCounter);
static uint8_t CANIO_tx_getNLG513ControlByte(void);
static CAN_prechargeContactorState_E CANIO_tx_getContactorState(void);

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static cantx_S cantx;
static cantx_counter_S cantx_counter;

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define set_criticalDataCounter(m,b,n,s) set(m,b,n,s, cantx_counter.counter_100Hz)
#define set_packChargeLimit(m,b,n,s) set(m,b,n,s, BMS.pack_charge_limit)
#define set_packDischargeLimit(m,b,n,s) set(m,b,n,s, BMS.pack_discharge_limit)
#define set_packVoltage(m,b,n,s) set(m,b,n,s, BMS.pack_voltage)
#define set_packCurrent(m,b,n,s) set(m,b,n,s, BMS.pack_current)
#define set_packContactorState(m,b,n,s) set(m,b,n,s, CANIO_tx_getContactorState())
#define set_bmsIMDResetState(m,b,n,s) set(m,b,n,s, (IO.bms_imd_reset ? CAN_SWITCHSTATUS_ON : CAN_SWITCHSTATUS_OFF))
#define set_bmsStatusMem(m,b,n,s) set(m,b,n,s, (IO.bms_status_mem ? CAN_FAULTSTATUS_OK : CAN_FAULTSTATUS_FAULT))
#define set_imdStatusMem(m,b,n,s) set(m,b,n,s, (IO.imd_status_mem ? CAN_FAULTSTATUS_OK : CAN_FAULTSTATUS_FAULT))
#define set_nlg513ControlByte(m,b,n,s) set(m,b,n,s, CANIO_tx_getNLG513ControlByte())
#define set_nlg513MaxMainsCurrent(m,b,n,s) set(m,b,n,s, 16.0f)
#define set_nlg513MaxChargeVoltage(m,b,n,s) set(m,b,n,s, BMS_PACK_VOLTAGE_MAX)
#define set_nlg513MaxChargeCurrent(m,b,n,s) set(m,b,n,s, BMS.pack_charge_limit)
#define transmit_BMSB_brusaChargeCommand (SYS_SFT_checkChargerTimeout())

#include "TemporaryStubbing.h"
#include "MessagePack_generated.c"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * CANTX_BUS_A_1ms_SWI
 * send BUS_A messages
 */
void CANTX_BUS_A_SWI(void)
{
    if (cantx.tx_100Hz_msg < VEH_packTable_10ms_length)
    {
        CAN_data_T     message_100Hz = {0};

        const packTable_S* entry_100Hz = packNextMessage((const packTable_S*)&VEH_packTable_10ms,
                                                        VEH_packTable_10ms_length,
                                                        &cantx.tx_100Hz_msg,
                                                        &message_100Hz,
                                                        &cantx_counter.counter_100Hz);

        if (entry_100Hz != NULL)
        {
            if (CAN_sendMsgBus0(CAN_TX_PRIO_100HZ, message_100Hz, entry_100Hz->id, entry_100Hz->len))
            {
                cantx.tx_100Hz_msg++;
            }
            memset(&message_100Hz, 0, sizeof(message_100Hz));
        }
    }
    if (cantx.tx_100Hz_msg == VEH_packTable_10ms_length)
    {
        if (cantx_counter.counter_100Hz != 255U)
        {
            cantx_counter.counter_100Hz++;
        }
        else
        {
            cantx_counter.counter_100Hz = 0U;
        }
        cantx.tx_100Hz_msg++;
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
        const packTable_S* entry   = &packTable[*index];
        uint8_t           counter = *nextCounter;

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

    return NULL;
}

static uint8_t CANIO_tx_getNLG513ControlByte(void)
{
    uint8_t ret = 0x00;
    switch (SYS.contacts)
    {
        case SYS_CONTACTORS_CLOSED:
            ret = 0x40;
            break;

        case SYS_CONTACTORS_HVP_CLOSED:
            ret = 0x80;
            break;

        default:
            ret = 0x00;
            break;
    }

    return ret;
}

static CAN_prechargeContactorState_E CANIO_tx_getContactorState(void)
{
    CAN_prechargeContactorState_E ret = CAN_PRECHARGECONTACTORSTATE_CONTACTORS_SNA;

    switch (SYS.contacts)
    {
        case SYS_CONTACTORS_OPEN:
            ret = CAN_PRECHARGECONTACTORSTATE_CONTACTORS_OPEN;
            break;

        case SYS_CONTACTORS_PRECHARGE:
            ret = CAN_PRECHARGECONTACTORSTATE_CONTACTORS_PRECHARGE_CLOSED;
            break;

        case SYS_CONTACTORS_CLOSED:
            ret = CAN_PRECHARGECONTACTORSTATE_CONTACTORS_PRECHARGE_HVP_CLOSED;
            break;

        case SYS_CONTACTORS_HVP_CLOSED:
            ret = CAN_PRECHARGECONTACTORSTATE_CONTACTORS_HVP_CLOSED;
            break;
    }

    return ret;
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
    if (cantx.tx_100Hz_msg < VEH_packTable_10ms_length) {
        // all the message weren't sent. TO-DO: error handling
    }
    cantx.tx_100Hz_msg = 0U;
}

/**
 * CANIO_tx_init
 * initialize module
 */
static void CANIO_tx_init(void)
{
    memset(&cantx, 0x00, sizeof(cantx));
    memset(&cantx_counter, 0x00, sizeof(cantx));
    HW_CAN_start();    // start CAN
}


const ModuleDesc_S CANIO_tx = {
    .moduleInit        = &CANIO_tx_init,
    .periodic1kHz_CLK  = &CANIO_tx_1kHz_PRD,
    .periodic100Hz_CLK = &CANIO_tx_100Hz_PRD,
};

