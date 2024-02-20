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
#include "HW_adc.h"

// Other Includes
#include "Environment.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#if defined(BMSW_BOARD_VA1)
# define VREF 3.3F            // [V] Bluepill reference voltage
#elif defined(BMSW_BOARD_VA3) /**< BMSW_BOARD_VA1 */
# define VREF 3.0F            /**< Shunt Diode reference voltage */
#endif                        /**< BMSW_BOARD_VA3 */

#define ADC_MAX_VAL    4095U    // Max integer value of ADC reading (2^12 for this chip)
#define IO_ADC_BUF_LEN 48U     /**< To fit the number of measurements into a time less than 100us
                                  the buffer length must be less than 100us / (ADC clock freq * 
                                  cycles per conversion). For this firmware, there is 14 cycles
                                  per conversion. Therefore the max samples per 100us is 57, which
                                  rounded down to the nearest multiple of 12 is 48 */

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t current;
} IO_S;


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern IO_S IO;
