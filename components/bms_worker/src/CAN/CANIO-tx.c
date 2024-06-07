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

#include "BMS_msgTx.h"

// imports for data access
#include "BuildDefines_generated.h"
#include "BatteryMonitoring.h"
#include "Cooling.h"
#include "Environment.h"
#include "IO.h"


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

#define VEH_packTable_10ms_SIZE    (sizeof(VEH_packTable_10ms) / sizeof(packTable_S))
#define VEH_packTable_1s_SIZE      (sizeof(VEH_packTable_1s) / sizeof(packTable_S))

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

static bool pack_VEH_BMS_criticalData_10ms(CAN_data_T* message, const uint8_t counter);
static bool pack_VEH_BMS_averagesSOCcellTemps_1s(CAN_data_T* message, const uint8_t counter);
static bool pack_VEH_BMS_cellTemp2To10_1s(CAN_data_T* message, const uint8_t counter);
static bool pack_VEH_BMS_cellTemp11To19_1s(CAN_data_T* message, const uint8_t counter);
static bool pack_VEH_BMS_cellVoltage0To5_1s(CAN_data_T* message, const uint8_t counter);
static bool pack_VEH_BMS_cellVoltage6To11_1s(CAN_data_T* message, const uint8_t counter);
static bool pack_VEH_BMS_cellVoltage12To15_1s(CAN_data_T* message, const uint8_t counter);
static bool pack_VEH_BMS_tempHumidity_1s(CAN_data_T* message, const uint8_t counter);
static bool pack_VEH_BMS_fans_1s(CAN_data_T* message, const uint8_t counter);

static const packTable_S VEH_packTable_10ms[] = {
    { &pack_VEH_BMS_criticalData_10ms, 0x100, 8U },
};

static const packTable_S VEH_packTable_1s[]   = {
    { &pack_VEH_BMS_averagesSOCcellTemps_1s, 0x700, 8U },
    { &pack_VEH_BMS_cellTemp2To10_1s,        0x710, 8U },
    { &pack_VEH_BMS_cellTemp11To19_1s,       0x720, 8U },
    { &pack_VEH_BMS_cellVoltage0To5_1s,      0x730, 8U },
    { &pack_VEH_BMS_cellVoltage6To11_1s,     0x740, 8U },
    { &pack_VEH_BMS_cellVoltage12To15_1s,    0x750, 7U },
    { &pack_VEH_BMS_tempHumidity_1s,         0x760, 5U },
    { &pack_VEH_BMS_fans_1s,                 0x770, 6U },
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
    static uint8_t   counter_100Hz = 0U;
    static uint8_t   counter_1Hz = 0U;
    CAN_data_T       message = { 0U };

    if (cantx.tx_10Hz_msg != VEH_packTable_10ms_SIZE)
    {

        const packTable_S* entry_100Hz = packNextMessage((const packTable_S*)&VEH_packTable_10ms,
                                                         VEH_packTable_10ms_SIZE,
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

        if (cantx.tx_10Hz_msg != VEH_packTable_10ms_SIZE)
        {
            cantx.tx_10Hz_msg++;
        }
    }

    if (cantx.tx_1Hz_msg != VEH_packTable_1s_SIZE)
    {

        const packTable_S* entry_1Hz = packNextMessage((const packTable_S*)&VEH_packTable_1s,
                                                       VEH_packTable_1s_SIZE,
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
    else
    {
        cantx.tx_1Hz_msg++;
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

static bool pack_VEH_BMS_criticalData_10ms(CAN_data_T* message, const uint8_t counter)
{
    set_BMSVoltageMin(message, BMS.voltage.min);
    set_BMSVoltageMax(message, BMS.voltage.max);
    set_ENVPackVoltage(message, BMS.pack_voltage);
    set_ENVTempMax(message, ENV.values.max_temp);
    set_BMSChargeLimit(message, BMS.charge_limit);
    set_BMSDischargeLimit(message, BMS.discharge_limit);
    set_BMSErrorFlag(message, BMS.state == BMS_ERROR);
    set_BMSFaultFlag(message, BMS.fault);
    set_ENVErrorFlag(message, ENV.state == ENV_ERROR);
    set_ENVFaultFlag(message, ENV.state == ENV_FAULT);
    set_Counter(message, counter);
    return true;
}

static bool pack_VEH_BMS_averagesSOCcellTemps_1s(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    set_BMSVoltageAvg(message, BMS.voltage.avg);
    set_ENVAvgTemp(message, ENV.values.avg_temp);
    set_BMSSOCMin(message, BMS.relative_soc.min);
    set_BMSSOCAAvg(message, BMS.relative_soc.avg);
    set_BMSSOCMax(message, BMS.relative_soc.max);
    set_CellTemp0(message, ENV.values.temps[0].temp);
    set_CellTemp1(message, ENV.values.temps[1].temp);
    return true;
}

static bool pack_VEH_BMS_cellTemp2To10_1s(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    set_CellTemp2(message, ENV.values.temps[2].temp);
    set_CellTemp3(message, ENV.values.temps[3].temp);
    set_CellTemp4(message, ENV.values.temps[4].temp);
    set_CellTemp5(message, ENV.values.temps[5].temp);
    set_CellTemp6(message, ENV.values.temps[6].temp);
    set_CellTemp7(message, ENV.values.temps[7].temp);
    set_CellTemp8(message, ENV.values.temps[8].temp);
    set_CellTemp9(message, ENV.values.temps[9].temp);
    set_CellTemp10(message, ENV.values.temps[10].temp);
    return true;
}


static bool pack_VEH_BMS_cellTemp11To19_1s(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    set_CellTemp11(message, ENV.values.temps[11].temp);
    set_CellTemp12(message, ENV.values.temps[12].temp);
    set_CellTemp13(message, ENV.values.temps[13].temp);
    set_CellTemp14(message, ENV.values.temps[14].temp);
    set_CellTemp15(message, ENV.values.temps[15].temp);
    set_CellTemp16(message, ENV.values.temps[16].temp);
    set_CellTemp17(message, ENV.values.temps[17].temp);
    set_CellTemp18(message, ENV.values.temps[18].temp);
    set_CellTemp19(message, ENV.values.temps[19].temp);
    return true;
}

static bool pack_VEH_BMS_cellVoltage0To5_1s(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    set_CellVoltage0(message, BMS.cells[0].voltage);
    set_CellVoltage1(message, BMS.cells[1].voltage);
    set_CellVoltage2(message, BMS.cells[2].voltage);
    set_CellVoltage3(message, BMS.cells[3].voltage);
    set_CellVoltage4(message, BMS.cells[4].voltage);
    set_CellVoltage5(message, BMS.cells[5].voltage);
    return true;
}

static bool pack_VEH_BMS_cellVoltage6To11_1s(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    set_CellVoltage6(message, BMS.cells[6].voltage);
    set_CellVoltage7(message, BMS.cells[7].voltage);
    set_CellVoltage8(message, BMS.cells[8].voltage);
    set_CellVoltage9(message, BMS.cells[9].voltage);
    set_CellVoltage10(message, BMS.cells[10].voltage);
    set_CellVoltage11(message, BMS.cells[11].voltage);
    return true;
}

static bool pack_VEH_BMS_cellVoltage12To15_1s(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    set_CellVoltage12(message, BMS.cells[12].voltage);
    set_CellVoltage13(message, BMS.cells[13].voltage);
    set_CellVoltage14(message, BMS.cells[14].voltage);
    set_CellVoltage15(message, BMS.cells[15].voltage);
    set_PackVoltage(message, BMS.pack_voltage);
    return true;
}

static bool pack_VEH_BMS_tempHumidity_1s(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    set_BoardTemp0(message, ENV.values.board.brd_temp[0]);
    set_BoardTemp1(message, ENV.values.board.brd_temp[1]);
    set_MCUTemp(message, ENV.values.board.mcu_temp);
    set_BoardAmbientTemp(message, ENV.values.board.ambient_temp);
    set_BoardRelativeHumidity(message, ENV.values.board.rh);
    return true;
}

static bool pack_VEH_BMS_fans_1s(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    set_CoolState0(message, COOL.state[0] == COOL_ON);
    set_CoolPercentage0(message, COOL.percentage[0]);
    set_CoolState1(message, COOL.state[1] == COOL_ON);
    set_CoolPercentage1(message, COOL.percentage[1]);
    set_FanRPM0(message, COOL.rpm[0]);
    set_FanRPM1(message, COOL.rpm[1]);
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
static void CANIO_tx_10Hz_PRD(void)
{
    if (cantx.tx_10Hz_msg < VEH_packTable_10ms_SIZE)
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
    if (cantx.tx_1Hz_msg < VEH_packTable_1s_SIZE)
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
    cantx.tx_1Hz_msg   = VEH_packTable_1s_SIZE;
    cantx.tx_10Hz_msg = VEH_packTable_10ms_SIZE;
    HW_CAN_start();    // start CAN
}


const ModuleDesc_S CANIO_tx = {
    .moduleInit        = &CANIO_tx_init,
 // .periodic10kHz_CLK = &CANIO_tx_10kHz_PRD,
    .periodic1kHz_CLK  = &CANIO_tx_1kHz_PRD,
    .periodic10Hz_CLK = &CANIO_tx_10Hz_PRD,
    .periodic1Hz_CLK   = &CANIO_tx_1Hz_PRD,
};
