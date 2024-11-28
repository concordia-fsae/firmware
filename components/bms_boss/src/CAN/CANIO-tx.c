/*
 * CANIO-tx.c
 * CAN Transmit
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CAN/CAN.h"

#include "HW_can.h"
#include "HW_tim.h"

#include "FreeRTOS_SWI.h"
#include "ModuleDesc.h"

#include "string.h"

// imports for data access
#include "IO.h"
#include "BMS.h"
#include "Sys.h"
#include "IMD.h"
#include "ENV.h"

#include "SigTx.c"
#include "MessageUnpack_generated.h"
#include <stdint.h>

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

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define set_criticalDataCounter(m,b,n,s) set(m,b,n,s, cantx_counter.counter_100Hz)
#define set_packChargeLimit(m,b,n,s) set(m,b,n,s, BMS.pack_charge_limit)
#define set_packDischargeLimit(m,b,n,s) set(m,b,n,s, BMS.pack_discharge_limit)
#define set_packVoltage(m,b,n,s) set(m,b,n,s, BMS.pack_voltage)
#define set_packCurrent(m,b,n,s) set(m,b,n,s, BMS.pack_current)
#define set_packContactorState(m,b,n,s) set(m,b,n,s, CANIO_tx_getContactorState())
#define set_nlg513ControlByte(m,b,n,s) set(m,b,n,s, CANIO_tx_getNLG513ControlByte())
#define set_nlg513MaxMainsCurrent(m,b,n,s) set(m,b,n,s, 16.0f)
#define set_nlg513MaxChargeVoltage(m,b,n,s) set(m,b,n,s, BMS_CONFIGURED_PACK_MAX_VOLTAGE)
#define set_nlg513MaxChargeCurrent(m,b,n,s) set(m,b,n,s, BMS.pack_charge_limit)
#define transmit_BMSB_brusaChargeCommand (SYS_SFT_checkChargerTimeout() == false)

#include "TemporaryStubbing.h"
#include "MessagePack_generated.c"

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
                                          const uint8_t      packTableLength,
                                          uint8_t*           index,
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

