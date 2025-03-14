/*
 * CANIO-tx.c
 * CAN Transmit
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CAN/CAN.h"
#include "CAN/CanTypes.h"
#include "HW_can.h"
#include "FreeRTOS_SWI.h"
#include "ModuleDesc.h"
#include "FeatureDefines_generated.h"
#include "CANIO_componentSpecific.h"
#include "MessagePack_generated.c"

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

static const packTable_S* packNextMessage(const packTable_S* packTable,
                                          const uint8_t      packTableLength,
                                          uint8_t*           index,
                                          CAN_data_T*        message,
                                          uint8_t*           nextCounter);

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * CANTX_BUS_A_1ms_SWI
 * send BUS_A messages
 */
void CANTX_SWI(void)
{
    for (CAN_bus_E bus = 0U; bus < CAN_BUS_COUNT; bus++)
    {
        for (uint8_t table = 0U; table < CAN_table[bus].busTableLength; table++)
        {
            for (uint8_t pack = CAN_table[bus].busTable[table].index; pack < CAN_table[bus].busTable[table].packTableLength; pack++)
            {
                CAN_data_T     message = {0};

                const packTable_S* entry = packNextMessage((const packTable_S*)CAN_table[bus].busTable[table].packTable,
                                                                CAN_table[bus].busTable[table].packTableLength,
                                                                &pack,
                                                                &message,
                                                                &CAN_table[bus].busTable[table].counter);
                if (entry != NULL)
                {
                    if (HW_CAN_sendMsg(bus, message, entry->id, entry->len))
                    {
                        CAN_table[bus].busTable[table].index = pack + 1;
                    }
                    else
                    {
                        CAN_table[bus].busTable[table].index = pack;
                        return;
                    }
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
    bool txRequest = false;

    for (CAN_bus_E bus = 0U; bus < CAN_BUS_COUNT; bus++)
    {
        for (uint8_t table = 0U; table < CAN_table[bus].busTableLength; table++)
        {
            if ((CAN_table[bus].busTable[table].packTableLength == 0U) ||
                (CANIO_getTimeMs() - CAN_table[bus].busTable[table].lastTimestamp < CAN_table[bus].busTable[table].period))
            {
                continue;
            }

            if (CAN_table[bus].busTable[table].index < CAN_table[bus].busTable[table].packTableLength) {
                // all the message weren't sent. TO-DO: error handling
            }

            CAN_table[bus].busTable[table].index = 0U;
            if (CAN_table[bus].busTable[table].counter != 255U)
            {
                CAN_table[bus].busTable[table].counter++;
            }
            else
            {
                CAN_table[bus].busTable[table].counter = 0U;
            }
            CAN_table[bus].busTable[table].lastTimestamp = CANIO_getTimeMs();
        }
    }
    for (CAN_bus_E bus = 0U; bus < CAN_BUS_COUNT; bus++)
    {
        for (uint8_t table = 0U; table < CAN_table[bus].busTableLength; table++)
        {
            if (CAN_table[bus].busTable[table].index < CAN_table[bus].busTable[table].packTableLength) {
                // all the message haven't been sent yet
                txRequest = true;
            }
        }
    }

    if (txRequest)
    {
#if FEATURE_IS_ENABLED(FEATURE_CANTX_SWI)
        SWI_invoke(CANTX_swi);
#else // FEATURE_CANTX_SWI
        CANTX_SWI();
#endif // not FEATURE_CANTX_SWI
    }
}

/**
 * CANIO_tx_init
 * initialize module
 */
static void CANIO_tx_init(void)
{
    for (CAN_bus_E bus = 0U; bus < CAN_BUS_COUNT; bus++)
    {
        HW_CAN_start(bus);    // start CAN peripheral
    }
}


const ModuleDesc_S CANIO_tx = {
    .moduleInit        = &CANIO_tx_init,
    .periodic1kHz_CLK  = &CANIO_tx_1kHz_PRD,
};

