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
#include "BatteryMonitoring.h"
#include "Cooling.h"
#include "Environment.h"
#include "IO.h"

#include "SigTx.c"
#include "CANTypes_generated.h"
#include "FeatureDefines_generated.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define set_envFaultFlag(m,b,n,s)                set(m,b,n,s, ENV.state == ENV_FAULT)
#define set_envErrorFlag(m, b, n, s)             set(m,b,n,s, ENV.state == ENV_ERROR)
#define set_faultFlag(m, b, n, s)                set(m,b,n,s, BMS.fault)
#define set_errorFlag(m, b, n, s)                set(m,b,n,s, BMS.state == BMS_ERROR)
#define set_dischargeLimit(m, b, n, s)           set(m,b,n,s, BMS.discharge_limit)
#define set_chargeLimit(m, b, n, s)              set(m,b,n,s, BMS.charge_limit)
#define set_tempMax(m, b, n, s)                  set(m,b,n,s, ENV.values.max_temp)
#define set_segmentVoltage(m, b, n, s)           set(m,b,n,s, BMS.pack_voltage)
#define set_voltageMax(m, b, n, s)               set(m,b,n,s, BMS.voltage.max)
#define set_voltageMin(m, b, n, s)               set(m,b,n,s, BMS.voltage.min)
#define set_cellTemp1(m, b, n, s)                set(m,b,n,s, ENV.values.temps[1].temp)
#define set_cellTemp0(m, b, n, s)                set(m,b,n,s, ENV.values.temps[0].temp)
#define set_socMax(m, b, n, s)                   set(m,b,n,s, BMS.relative_soc.max)
#define set_socAvg(m, b, n, s)                   set(m,b,n,s, BMS.relative_soc.avg)
#define set_socMin(m, b, n, s)                   set(m,b,n,s, BMS.relative_soc.min)
#define set_tempAvg(m, b, n, s)                  set(m,b,n,s, ENV.values.avg_temp)
#define set_cellVoltageAvg(m, b, n, s)           set(m,b,n,s, BMS.voltage.avg)
#define set_cellTemp10(m, b, n, s)               set(m,b,n,s, ENV.values.temps[10].temp)
#define set_cellTemp9(m, b, n, s)                set(m,b,n,s, ENV.values.temps[9].temp)
#define set_cellTemp8(m, b, n, s)                set(m,b,n,s, ENV.values.temps[8].temp)
#define set_cellTemp7(m, b, n, s)                set(m,b,n,s, ENV.values.temps[7].temp)
#define set_cellTemp6(m, b, n, s)                set(m,b,n,s, ENV.values.temps[6].temp)
#define set_cellTemp5(m, b, n, s)                set(m,b,n,s, ENV.values.temps[5].temp)
#define set_cellTemp4(m, b, n, s)                set(m,b,n,s, ENV.values.temps[4].temp)
#define set_cellTemp3(m, b, n, s)                set(m,b,n,s, ENV.values.temps[3].temp)
#define set_cellTemp2(m, b, n, s)                set(m,b,n,s, ENV.values.temps[2].temp)
#define set_cellTemp19(m, b, n, s)               set(m,b,n,s, ENV.values.temps[19].temp)
#define set_cellTemp18(m, b, n, s)               set(m,b,n,s, ENV.values.temps[18].temp)
#define set_cellTemp17(m, b, n, s)               set(m,b,n,s, ENV.values.temps[17].temp)
#define set_cellTemp16(m, b, n, s)               set(m,b,n,s, ENV.values.temps[16].temp)
#define set_cellTemp15(m, b, n, s)               set(m,b,n,s, ENV.values.temps[15].temp)
#define set_cellTemp14(m, b, n, s)               set(m,b,n,s, ENV.values.temps[14].temp)
#define set_cellTemp13(m, b, n, s)               set(m,b,n,s, ENV.values.temps[13].temp)
#define set_cellTemp12(m, b, n, s)               set(m,b,n,s, ENV.values.temps[12].temp)
#define set_cellTemp11(m, b, n, s)               set(m,b,n,s, ENV.values.temps[11].temp)
#define set_cellVoltage5(m, b, n, s)             set(m,b,n,s, BMS.cells[5].voltage)
#define set_cellVoltage4(m, b, n, s)             set(m,b,n,s, BMS.cells[4].voltage)
#define set_cellVoltage3(m, b, n, s)             set(m,b,n,s, BMS.cells[3].voltage)
#define set_cellVoltage2(m, b, n, s)             set(m,b,n,s, BMS.cells[2].voltage)
#define set_cellVoltage1(m, b, n, s)             set(m,b,n,s, BMS.cells[1].voltage)
#define set_cellVoltage0(m, b, n, s)             set(m,b,n,s, BMS.cells[0].voltage)
#define set_cellVoltage11(m, b, n, s)            set(m,b,n,s, BMS.cells[11].voltage)
#define set_cellVoltage10(m, b, n, s)            set(m,b,n,s, BMS.cells[10].voltage)
#define set_cellVoltage9(m, b, n, s)             set(m,b,n,s, BMS.cells[9].voltage)
#define set_cellVoltage8(m, b, n, s)             set(m,b,n,s, BMS.cells[8].voltage)
#define set_cellVoltage7(m, b, n, s)             set(m,b,n,s, BMS.cells[7].voltage)
#define set_cellVoltage6(m, b, n, s)             set(m,b,n,s, BMS.cells[6].voltage)
#define set_segmentVoltageHighRes(m, b, n, s)    set(m,b,n,s, BMS.pack_voltage)
#define set_segmentVoltageHighResCalculated(m, b, n, s)    set(m,b,n,s, BMS.calculated_pack_voltage)
#define set_cellVoltage15(m, b, n, s)            set(m,b,n,s, BMS.cells[15].voltage)
#define set_cellVoltage14(m, b, n, s)            set(m,b,n,s, BMS.cells[14].voltage)
#define set_cellVoltage13(m, b, n, s)            set(m,b,n,s, BMS.cells[13].voltage)
#define set_cellVoltage12(m, b, n, s)            set(m,b,n,s, BMS.cells[12].voltage)
#define set_boardRelativeHumidity(m, b, n, s)    set(m,b,n,s, ENV.values.board.rh)
#define set_boardAmbientTemp(m, b, n, s)         set(m,b,n,s, ENV.values.board.ambient_temp)
#define set_mcuTemp(m, b, n, s)                  set(m,b,n,s, ENV.values.board.mcu_temp)
#define set_boardTemp0(m, b, n, s)               set(m,b,n,s, ENV.values.board.brd_temp[0])
#define set_boardTemp1(m, b, n, s)               set(m,b,n,s, ENV.values.board.brd_temp[1])
#define set_fan1RPM(m, b, n, s)                  set(m,b,n,s, COOL.rpm[1])
#define set_fan0RPM(m, b, n, s)                  set(m,b,n,s, COOL.rpm[0])
#define set_coolPct1(m, b, n, s)                 set(m,b,n,s, COOL.percentage[1])
#define set_coolState1(m, b, n, s)               set(m,b,n,s, (COOL.state[1] != COOL_OFF) ? CAN_OUTPUTSTATE_ON : CAN_OUTPUTSTATE_OFF)
#define set_coolPct0(m, b, n, s)                 set(m,b,n,s, COOL.percentage[0])
#define set_coolState0(m,b,n,s)                  set(m,b,n,s, (COOL.state[0] != COOL_OFF) ? CAN_OUTPUTSTATE_ON : CAN_OUTPUTSTATE_OFF)

#include "TemporaryStubbing.h"
#include "MessagePack_generated.c"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

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
void CANTX_BUS_VEH_SWI(void)
{
    CAN_bus_E bus = CAN_BUS_VEH;

    for (uint8_t table = 0U; table < CAN_table[bus].busTableLength; table++)
    {
        for (uint8_t pack = CAN_table[bus].busTable[table].index; pack < CAN_table[bus].busTable[table].packTableLength; pack++)
        {
            CAN_data_T     message = {0};

            const packTable_S* entry = packNextMessage((const packTable_S*)CAN_table[bus].busTable[table].packTable,
                                                       CAN_table[bus].busTable[table].packTableLength,
                                                       &CAN_table[bus].busTable[table].index,
                                                       &message,
                                                       &CAN_table[bus].busTable[table].counter);
            if (entry != NULL)
            {
                bool transmitFailed = true;
                uint8_t mailbox = CAN_TX_MAILBOX_0;
                for (; mailbox < CAN_TX_MAILBOX_COUNT; mailbox++)
                {
                    if (CAN_sendMsgBus0(mailbox, message, entry->id, entry->len))
                    {
                        CAN_table[bus].busTable[table].index++;
                        transmitFailed = false;
                        break;
                    }
                }
                if (transmitFailed)
                {
                    return;
                }
            }
            if (CAN_table[bus].busTable[table].index == CAN_table[bus].busTable[table].packTableLength)
            {
                if (CAN_table[bus].busTable[table].counter != 255U)
                {
                    CAN_table[bus].busTable[table].counter++;
                }
                else
                {
                    CAN_table[bus].busTable[table].counter = 0U;
                }
            }
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
        uint8_t            counter = *nextCounter;

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

static void CANIO_tx_1kHz_PRD(void)
{
    for (CAN_bus_E bus = 0U; bus < CAN_BUS_COUNT; bus++)
    {
        for (uint8_t table = 0U; table < CAN_table[bus].busTableLength; table++)
        {
            if (HW_TIM_getTimeMS() - CAN_table[bus].busTable[table].lastTimestamp < CAN_table[bus].busTable[table].period)
            {
                continue;
            }

            CAN_table[bus].busTable[table].lastTimestamp = HW_TIM_getTimeMS();

            if (CAN_table[bus].busTable[table].index < CAN_table[bus].busTable[table].packTableLength) {
                // all the message weren't sent. TO-DO: error handling
            }
            CAN_table[bus].busTable[table].index = 0U;
        }
    }
#if FEATURE_IS_ENABLED(FEATURE_CANTX_SWI)
    SWI_invoke(CANTX_BUS_VEH_swi);
#else // FEATURE_CANTX_SWI
    CANTX_BUS_VEH_SWI();
#endif // not FEATURE_CANTX_SWI
}

/**
 * CANIO_tx_init
 * initialize module
 */
static void CANIO_tx_init(void)
{
    HW_CAN_start();    // start CAN
}


const ModuleDesc_S CANIO_tx = {
    .moduleInit        = &CANIO_tx_init,
    .periodic1kHz_CLK  = &CANIO_tx_1kHz_PRD,
};
