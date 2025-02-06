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
#include "LIB_nvm.h"

// Firmware Includes
#include "HW_adc.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define VREF 3.0F /**< Shunt Diode reference voltage */

#define ADC_MAX_VAL    4095U    // Max integer value of ADC reading (2^12 for this chip)
#define IO_ADC_BUF_LEN 192U      /**< To fit the number of measurements into a time less than 100us  \
                                   the buffer length must be less than 100us / (ADC clock freq *    \
                                   cycles per conversion). For this firmware, there is 14 cycles    \
                                   per conversion. Therefore the max samples per 100us is 57, which \
                                   rounded down to the nearest multiple of 12 is 48 */

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t current;
    float32_t mcu_temp;
    bool master_switch :1;
    bool imd_ok :1;
    bool feedback_sfty_bms :1;
    bool feedback_sfty_imd :1;
} IO_S;

#define IO_CALIBRATIONVALUES_VERSION_0 0U
typedef struct
{
    float32_t current_sense_differential;
    float32_t spare[4];
} LIB_NVM_STORAGE(IO_calibrationValues_V0_S);

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern IO_S IO;
extern IO_calibrationValues_V0_S IO_calibrationValues;
extern const IO_calibrationValues_V0_S IO_calibrationValues_default;

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void IO_stopCalibration_currentSense(void);
