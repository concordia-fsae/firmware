/**
 * @file IO.h
 * @brief  Header file for IO Module
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-28
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Firmware includes */
#include "HW_NX3L4051PW.h"
#include "HW_adc.h"

/**< Other Includes */
#include "BatteryMonitoring.h"
#include "Environment.h"
#include "FloatTypes.h"
#include "Types.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#if defined(BMSW_BOARD_VA1)
# define VREF 3.3F            // [V] Bluepill reference voltage
#elif defined(BMSW_BOARD_VA3) /**< BMSW_BOARD_VA1 */
# define VREF 3.0F            /**< Shunt Diode reference voltage */
#endif                        /**< BMSW_BOARD_VA3 */

#define ADC_MAX_VAL    4095U    // Max integer value of ADC reading (2^12 for this chip)
#define IO_ADC_BUF_LEN 96U      // number of samples to fill with DMA,
                                // processed when half full and again when completely full

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    struct
    {
        float32_t mcu;
#if defined(BMSW_BOARD_VA3)
        float32_t mux1[MUX_COUNT];
        float32_t mux2[MUX_COUNT];
        float32_t mux3[MUX_COUNT];
        float32_t board[BRD_COUNT];
#endif /**< BMSW_BOARD_VA3 */
    } temp;

    float32_t cell[CELL_COUNT];
    float32_t segment;

    uint8_t addr;
} IO_S;


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern IO_S IO;

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/
