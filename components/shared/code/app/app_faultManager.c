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
#include "Utility.h"

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
    FLAG_create(faultBits, MAX_FAULTS);
    FLAG_create(waiting, MAX_FAULTS);
    FLAG_create(buffer, MAX_FAULTS);
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
    FLAG_assign(fm_data.faultBits, fault, faulted);
    FLAG_or(fm_data.waiting, fault, faulted);
}

bool app_faultManager_getFaultState(FM_fault_E fault)
{
    return FLAG_get(fm_data.faultBits, fault);
}

uint8_t* app_faultManager_transmit(void)
{
    for (uint8_t i = 0U; i < WORDS_FROM_COUNT(MAX_FAULTS); i++)
    {
        fm_data.buffer[i] = FLAG_GET_WORD(fm_data.waiting, i) | FLAG_GET_WORD(fm_data.faultBits, i);
        FLAG_clearAll(fm_data.waiting, MAX_FAULTS);
    }

    return (uint8_t*)&fm_data.buffer;
}
