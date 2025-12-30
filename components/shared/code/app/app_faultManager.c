/**
 * app_faultManager.c
 * @brief Fault Manager fource code
 *
 * Usage
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "app_faultManager.h"

_Static_assert(MAX_FAULTS > FM_FAULT_COUNT, "Number of faults must be less than 64");

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define BUFFERSIZE (MAX_FAULTS / 32U)

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint32_t faultBits[BUFFERSIZE];
    uint32_t waiting[BUFFERSIZE];
    uint32_t buffer[BUFFERSIZE];
} fm_data_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static fm_data_S fm_data;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void app_faultManager_setFaultState(FM_fault_E fault, bool faulted)
{
    const uint32_t bit = 1 << (fault % 32U);
    fm_data.faultBits[fault / 32U] = faulted ? fm_data.faultBits[fault / 32U] | bit :
                                               fm_data.faultBits[fault / 32U] & ~bit;
    fm_data.waiting[fault / 32U] |= faulted ? bit : 0U;
}

bool app_faultManager_getFaultState(FM_fault_E fault)
{
    return (fm_data.faultBits[fault / 32U] & (1U << (fault % 32U))) != 0U;
}

uint32_t* app_faultManager_transmit(void)
{
    for (uint8_t i = 0U; i < BUFFERSIZE; i++)
    {
        fm_data.buffer[i] = fm_data.waiting[i] | fm_data.faultBits[i];
        fm_data.waiting[i] = 0U;
    }

    return (uint32_t*)&fm_data.buffer;
}
