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

#include "string.h"

// imports for data access
#include "BuildDefines_generated.h"
#include "BatteryMonitoring.h"
#include "Cooling.h"
#include "Environment.h"
#include "IO.h"

#include "SigTx.c"

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

#define set_envFaultFlag(m,b,n,s)                set(m,b,n,s, ENV.state == ENV_FAULT)
#define set_envErrorFlag(m, b, n, s)             set(m,b,n,s, ENV.state == ENV_ERROR)
#define set_faultFlag(m, b, n, s)                set(m,b,n,s, 0); unsent_signal(m)
#define set_errorFlag(m, b, n, s)                set(m,b,n,s, 0); unsent_signal(m)
#define set_dischargeLimit(m, b, n, s)           set(m,b,n,s, 0); unsent_signal(m)
#define set_chargeLimit(m, b, n, s)              set(m,b,n,s, 0); unsent_signal(m)
#define set_tempMax(m, b, n, s)                  set(m,b,n,s, 0); unsent_signal(m)
#define set_segmentVoltage(m, b, n, s)           set(m,b,n,s, 0); unsent_signal(m)
#define set_voltageMax(m, b, n, s)               set(m,b,n,s, 0); unsent_signal(m)
#define set_voltageMin(m, b, n, s)               set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp1(m, b, n, s)                set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp0(m, b, n, s)                set(m,b,n,s, 0); unsent_signal(m)
#define set_socMax(m, b, n, s)                   set(m,b,n,s, 0); unsent_signal(m)
#define set_socAvg(m, b, n, s)                   set(m,b,n,s, 0); unsent_signal(m)
#define set_socMin(m, b, n, s)                   set(m,b,n,s, 0); unsent_signal(m)
#define set_tempAvg(m, b, n, s)                  set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltageAvg(m, b, n, s)           set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp10(m, b, n, s)               set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp9(m, b, n, s)                set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp8(m, b, n, s)                set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp7(m, b, n, s)                set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp6(m, b, n, s)                set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp5(m, b, n, s)                set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp4(m, b, n, s)                set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp3(m, b, n, s)                set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp2(m, b, n, s)                set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp19(m, b, n, s)               set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp18(m, b, n, s)               set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp17(m, b, n, s)               set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp16(m, b, n, s)               set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp15(m, b, n, s)               set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp14(m, b, n, s)               set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp13(m, b, n, s)               set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp12(m, b, n, s)               set(m,b,n,s, 0); unsent_signal(m)
#define set_cellTemp11(m, b, n, s)               set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltage5(m, b, n, s)             set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltage4(m, b, n, s)             set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltage3(m, b, n, s)             set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltage2(m, b, n, s)             set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltage1(m, b, n, s)             set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltage0(m, b, n, s)             set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltage11(m, b, n, s)            set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltage10(m, b, n, s)            set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltage9(m, b, n, s)             set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltage8(m, b, n, s)             set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltage7(m, b, n, s)             set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltage6(m, b, n, s)             set(m,b,n,s, 0); unsent_signal(m)
#define set_segmentVoltageHighRes(m, b, n, s)    set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltage15(m, b, n, s)            set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltage14(m, b, n, s)            set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltage13(m, b, n, s)            set(m,b,n,s, 0); unsent_signal(m)
#define set_cellVoltage12(m, b, n, s)            set(m,b,n,s, 0); unsent_signal(m)
#define set_boardRelativeHumidity(m, b, n, s)    set(m,b,n,s, 0); unsent_signal(m)
#define set_boardAmbientTemp(m, b, n, s)         set(m,b,n,s, 0); unsent_signal(m)
#define set_mcuTemp(m, b, n, s)                  set(m,b,n,s, 0); unsent_signal(m)
#define set_boardTemp0(m, b, n, s)               set(m,b,n,s, 0); unsent_signal(m)
#define set_boardTemp1(m, b, n, s)               set(m,b,n,s, 0); unsent_signal(m)
#define set_fan1RPM(m, b, n, s)                  set(m,b,n,s, 0); unsent_signal(m)
#define set_fan0RPM(m, b, n, s)                  set(m,b,n,s, 0); unsent_signal(m)
#define set_coolPct1(m, b, n, s)                 set(m,b,n,s, 0); unsent_signal(m)
#define set_coolState1(m, b, n, s)               set(m,b,n,s, 0); unsent_signal(m)
#define set_coolPct0(m, b, n, s)                 set(m,b,n,s, 0); unsent_signal(m)
#define set_coolState0(m,b,n,s)                  set(m,b,n,s, 0); unsent_signal(m)

#include "TemporaryStubbing.h"
#include "MessagePack_generated.c"

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
            if (CAN_sendMsgBus0(CAN_TX_PRIO_100HZ, message, entry_100Hz->id, entry_100Hz->len))
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
            if (CAN_sendMsgBus0(CAN_TX_PRIO_1HZ, message, entry_1Hz->id, entry_1Hz->len))
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
        \

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
