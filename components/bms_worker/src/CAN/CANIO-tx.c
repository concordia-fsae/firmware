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
extern Module_taskStats_S swi_stats;


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

static const packTable_S* packNextMessage(const packTable_S* packTable,
                                          const uint8_t      packTableLength,
                                          uint8_t*     index,
                                          CAN_data_T*        message,
                                          uint8_t*           nextCounter);

static bool MSG_pack_BMS_100Hz_Critical(CAN_data_T* message, const uint8_t counter);
static bool MSG_pack_BMS_1Hz_SOC_Voltage_Temp(CAN_data_T* message, const uint8_t counter);
static bool MSG_pack_BMS_1Hz_Cell_Temp_2_to_10(CAN_data_T* message, const uint8_t counter);
static bool MSG_pack_BMS_1Hz_Cell_Temp_11_to_19(CAN_data_T* message, const uint8_t counter);
static bool MSG_pack_BMS_1Hz_Cell_Voltage_0_to_5(CAN_data_T* message, const uint8_t counter);
static bool MSG_pack_BMS_1Hz_Cell_Voltage_6_to_11(CAN_data_T* message, const uint8_t counter);
static bool MSG_pack_BMS_1Hz_Cell_Voltage_12_to_15(CAN_data_T* message, const uint8_t counter);
static bool MSG_pack_BMS_1Hz_Temperatures_and_Humidity(CAN_data_T* message, const uint8_t counter);
static bool MSG_pack_BMS_1Hz_Fans(CAN_data_T* message, const uint8_t counter);

static const packTable_S MSG_packTable_100Hz[] = {
//    { &MSG_pack_BMS_100Hz_Critical, 0x100, 8U },
};
static const packTable_S MSG_packTable_10Hz[] = {
    { &MSG_pack_BMS_100Hz_Critical, 0x100, 8U },
};
static const packTable_S MSG_packTable_1Hz[] = {
    { &MSG_pack_BMS_1Hz_SOC_Voltage_Temp, 0x700, 8U },
    { &MSG_pack_BMS_1Hz_Cell_Temp_2_to_10, 0x710, 8U },
    { &MSG_pack_BMS_1Hz_Cell_Temp_11_to_19, 0x720, 8U },
    { &MSG_pack_BMS_1Hz_Cell_Voltage_0_to_5, 0x730, 8U },
    { &MSG_pack_BMS_1Hz_Cell_Voltage_6_to_11, 0x740, 8U },
    { &MSG_pack_BMS_1Hz_Cell_Voltage_12_to_15, 0x750, 8U },
    { &MSG_pack_BMS_1Hz_Temperatures_and_Humidity, 0x760, 8U },
    { &MSG_pack_BMS_1Hz_Fans, 0x770, 8U },
};

_Static_assert(MSG_packTable_100Hz_SIZE <= sizeof(MSG_packTable_100Hz) / sizeof(packTable_S), "Size of 100Hz pack table is incorrect.");
_Static_assert(MSG_packTable_10Hz_SIZE <= sizeof(MSG_packTable_10Hz) / sizeof(packTable_S), "Size of 10Hz pack table is incorrect.");
_Static_assert(MSG_packTable_1Hz_SIZE <= sizeof(MSG_packTable_1Hz) / sizeof(packTable_S), "Size of 1Hz pack table is incorrect.");


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
            if (CAN_sendMsgBus0(CAN_TX_PRIO_100HZ, message_100Hz, MSG_UID_SEGMENT(entry_100Hz->id), entry_100Hz->len))
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
            if (CAN_sendMsgBus0(CAN_TX_PRIO_10HZ, message_10Hz, MSG_UID_SEGMENT(entry_10Hz->id), entry_10Hz->len))
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
            if (CAN_sendMsgBus0(CAN_TX_PRIO_1HZ, message_1Hz, MSG_UID_SEGMENT(entry_1Hz->id), entry_1Hz->len))
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
    message->u64  = ((uint64_t)((uint16_t)(BMS.voltage.min * 200) & 0x3FF)) << 53;   //10 bits, 5mV precision, range [0-5115]mV
    message->u64 |= ((uint64_t)((uint16_t)(BMS.voltage.max * 200) & 0x3FF)) << 43;   //10 bits, 5mV precision, range [0-5115]mV
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.min_temp & 0x7F)) << 36;        //7 bits, 1 deg C precision
    message->u64 |= ((uint64_t)ENV.values.max_temp & 0x7F) << 29;                    //7 bits, 1 deg C precision
    message->u64 |= ((uint64_t)BMS.charge_limit & 0x1F) << 24;                       //5 bits, 1A precision
    message->u64 |= ((uint64_t)BMS.discharge_limit & 0xFF) << 16;                    //8 bits, 1A precision
    message->u64 |= (((BMS.state == BMS_ERROR) ? 0x01 << 7 : 0U) |
                    ((BMS.fault) ? 0x01 << 6 : 0U) |
                    ((ENV.state == ENV_ERROR) ? 0x01 << 5 : 0U) |
                    ((ENV.state == ENV_FAULT) ? 0x01 << 4 : 0U)) << 8;              //8 bits, 1 bit per flag, currently 4 unused
    message->u64 |= counter;                                                   //8 bits
    return true; //63 bits used, 1 bit unused
}

static bool MSG_pack_BMS_1Hz_SOC_Voltage_Temp(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    message->u64  = ((uint64_t)((uint16_t)(BMS.voltage.avg / 3) & 0x3FF)) << 54;        //10 bits, 3mv precisiion
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.avg_temp & 0x7F)) << 47;            //7 bits, 1 deg C precission
    message->u64 |= ((uint64_t)((uint16_t)(BMS.relative_soc.min * 10) & 0x3FF)) << 37;   //10 bits, 0.1% precision, range [0-102.3]%
    message->u64 |= ((uint16_t)(BMS.relative_soc.avg * 10) & 0x3FF) << 27;              //10 bits, 0.1% precision, range [0-102.3]%
    message->u64 |= ((uint16_t)(BMS.relative_soc.max * 10) & 0x3FF) << 17;              //10 bits, 0.1% precision, range [0-102.3]%
    message->u64 |= ((uint8_t)ENV.values.temps[0].temp & 0x7F) << 10;                   //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint8_t)ENV.values.temps[1].temp & 0x7F) << 3;                    //7 bits, 1 deg C precision, range [0-127]deg C
    return true; //61 bits used, 3 bits unused
}

static bool MSG_pack_BMS_1Hz_Cell_Temp_2_to_10(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    message->u64  = ((uint64_t)((uint8_t)ENV.values.temps[2].temp & 0x7F)) << 57;   //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.temps[3].temp & 0x7F)) << 50;   //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.temps[4].temp & 0x7F)) << 43;   //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.temps[5].temp & 0x7F)) << 36;   //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.temps[6].temp & 0x7F)) << 29;   //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.temps[7].temp & 0x7F)) << 22;   //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.temps[8].temp & 0x7F)) << 15;   //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.temps[9].temp & 0x7F)) << 8;    //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.temps[10].temp & 0x7F)) << 1;   //7 bits, 1 deg C precision, range [0-127]deg C
    return true; //63 bits used, 1 bit unused
}


static bool MSG_pack_BMS_1Hz_Cell_Temp_11_to_19(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    message->u64  = ((uint64_t)((uint8_t)ENV.values.temps[11].temp & 0x7F)) << 57;   //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.temps[12].temp & 0x7F)) << 50;   //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.temps[13].temp & 0x7F)) << 43;   //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.temps[14].temp & 0x7F)) << 36;   //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.temps[15].temp & 0x7F)) << 29;   //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.temps[16].temp & 0x7F)) << 22;   //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.temps[17].temp & 0x7F)) << 15;   //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.temps[18].temp & 0x7F)) << 8;    //7 bits, 1 deg C precision, range [0-127]deg C
    message->u64 |= ((uint64_t)((uint8_t)ENV.values.temps[19].temp & 0x7F)) << 1;   //7 bits, 1 deg C precision, range [0-127]deg C
    return true; //63 bits used, 1 bit unused
}

static bool MSG_pack_BMS_1Hz_Cell_Voltage_0_to_5(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    message->u64  = ((uint64_t)((uint16_t)(BMS.cells[0].voltage / 5) & 0x3FF)) << 54;   //10 bits, 5mV precision, range [0-5120]mV
    message->u64 |= ((uint64_t)((uint16_t)(BMS.cells[1].voltage / 5) & 0x3FF)) << 44;   //10 bits, 5mV precision, range [0-5120]mV
    message->u64 |= ((uint64_t)((uint16_t)(BMS.cells[2].voltage / 5) & 0x3FF)) << 34;   //10 bits, 5mV precision, range [0-5120]mV
    message->u64 |= ((uint64_t)((uint16_t)(BMS.cells[3].voltage / 5) & 0x3FF)) << 24;   //10 bits, 5mV precision, range [0-5120]mV
    message->u64 |= ((uint64_t)((uint16_t)(BMS.cells[4].voltage / 5) & 0x3FF)) << 14;   //10 bits, 5mV precision, range [0-5120]mV
    message->u64 |= ((uint64_t)((uint16_t)(BMS.cells[5].voltage / 5) & 0x3FF)) << 4;    //10 bits, 5mV precision, range [0-5120]mV
    return true; //60 bits used, 4 unused
}

static bool MSG_pack_BMS_1Hz_Cell_Voltage_6_to_11(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    message->u64  = ((uint64_t)((uint16_t)(BMS.cells[6].voltage / 5) & 0x3FF)) << 54;   //10 bits, 5mV precision, range [0-5120]mV
    message->u64 |= ((uint64_t)((uint16_t)(BMS.cells[7].voltage / 5) & 0x3FF)) << 44;   //10 bits, 5mV precision, range [0-5120]mV
    message->u64 |= ((uint64_t)((uint16_t)(BMS.cells[8].voltage / 5) & 0x3FF)) << 34;   //10 bits, 5mV precision, range [0-5120]mV
    message->u64 |= ((uint64_t)((uint16_t)(BMS.cells[9].voltage / 5) & 0x3FF)) << 24;   //10 bits, 5mV precision, range [0-5120]mV
    message->u64 |= ((uint64_t)((uint16_t)(BMS.cells[10].voltage / 5) & 0x3FF)) << 14;  //10 bits, 5mV precision, range [0-5120]mV
    message->u64 |= ((uint64_t)((uint16_t)(BMS.cells[11].voltage / 5) & 0x3FF)) << 4;   //10 bits, 5mV precision, range [0-5120]mV
    return true; //60 bits used, 4 unused
}

static bool MSG_pack_BMS_1Hz_Cell_Voltage_12_to_15(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    message->u64  = ((uint64_t)((uint16_t)(BMS.cells[12].voltage / 5) & 0x3FF)) << 54;  //10 bits, 5mV precision, range [0-5120]mV
    message->u64 |= ((uint64_t)((uint16_t)(BMS.cells[13].voltage / 5) & 0x3FF)) << 44;  //10 bits, 5mV precision, range [0-5120]mV
    message->u64 |= ((uint64_t)((uint16_t)(BMS.cells[14].voltage / 5) & 0x3FF)) << 34;  //10 bits, 5mV precision, range [0-5120]mV
    message->u64 |= ((uint64_t)((uint16_t)(BMS.cells[15].voltage / 5) & 0x3FF)) << 24;  //10 bits, 5mV precision, range [0-5120]mV
    message->u64 |= ((uint64_t)((uint16_t)(BMS.pack_voltage / 10) & 0x1FFF)) << 11;     //13 bits, 10mV precision, range [0-81910]mV
    return true; //53 bits used, 11 unused
}

static bool MSG_pack_BMS_1Hz_Temperatures_and_Humidity(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    message->u8[0] = (uint8_t)(ENV.values.board.brd_temp[0] / 10);  //8 bits, 1 deg C precision, range [0-255]deg C
    message->u8[1] = (uint8_t)(ENV.values.board.brd_temp[1] / 10);  //8 bits, 1 deg C precision, range [0-255]deg C
    message->u8[2] = (uint8_t)(ENV.values.board.mcu_temp / 10);     //8 bits, 1 deg C precision, range [0-255]deg C
    message->u8[3] = (uint8_t)(ENV.values.board.ambient_temp / 10); //8 bits, 1 deg C precision, range [0-255]deg C
    message->u8[4] = (uint8_t)(ENV.values.board.rh / 100);          //8 bits, 1% precision, range [0-255]%
    return true; //40 bits used, 24 unused
}

static bool MSG_pack_BMS_1Hz_Fans(CAN_data_T* message, const uint8_t counter)
{
    UNUSED(counter);
    message->u8[0]  = COOL.percentage[0];   //8 bits, 1% precision, range [0-255]%
    message->u8[1]  = COOL.percentage[1];   //8 bits, 1% precision, range [0-255]%
    message->u16[1] = COOL.rpm[0];          //16 bits, 1 rpm precision, range [0-65525]rpm
    message->u16[2] = COOL.rpm[1];          //16 bits, 1 rpm precision, range [0-65525]rpm
    return true; //48 bits used, 16 unused
}

//static void CANIO_tx_10kHz_PRD(void)
//{
//    SWI_invoke(CANTX_BUS_A_swi);
//}

static void CANIO_tx_1kHz_PRD(void)
{
    SWI_invoke(CANTX_BUS_A_swi);
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
    cantx.tx_1Hz_msg = MSG_packTable_1Hz_SIZE;
    cantx.tx_10Hz_msg = MSG_packTable_10Hz_SIZE;
    cantx.tx_100Hz_msg = MSG_packTable_100Hz_SIZE;
    HW_CAN_start();    // start CAN
}


const ModuleDesc_S CANIO_tx = {
    .moduleInit        = &CANIO_tx_init,
    //.periodic10kHz_CLK = &CANIO_tx_10kHz_PRD,
    .periodic1kHz_CLK  = &CANIO_tx_1kHz_PRD,
    .periodic100Hz_CLK = &CANIO_tx_100Hz_PRD,
    .periodic10Hz_CLK  = &CANIO_tx_10Hz_PRD,
    .periodic1Hz_CLK   = &CANIO_tx_1Hz_PRD,
};
