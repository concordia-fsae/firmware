/**
 * @file IO.h
 * @brief  Header file for IO Module
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "FloatTypes.h"
#include "Types.h"

// Firmware Includes
#include "HW_NX3L4051PW.h"
#include "HW_adc.h"

// Other Includes
#include "BatteryMonitoring.h"
#include "Environment.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define IO_ADC_BUF_LEN 48U      // number of samples to fill with DMA,
                                // processed when half full and again when completely full
                                // (13.5+3) cycles * 48 samples @ 8MHz -> 99us < 200us - 15us 

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    struct
    {
        float32_t mcu;
        float32_t mux1[NX3L_MUX_COUNT];
        float32_t mux2[NX3L_MUX_COUNT];
        float32_t mux3[NX3L_MUX_COUNT];
        float32_t board[BRD_COUNT];
    } temp;

    float32_t cell[MAX_CELL_COUNT];
    float32_t segment;
} IO_S;


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern IO_S IO;

#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
void IO10kHz_CB(void);
#endif // not FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK
