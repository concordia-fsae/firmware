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
#include "HW_adc.h"

/**< Other Includes */
#include "FloatTypes.h"
#include "Types.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define IO_ADC_BUF_LEN              300U    // number of samples to fill with DMA,
                                         // processed when half full and again when completely full

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

#if defined (BMSW_BOARD_VA3)
typedef enum
{
    MUX1 = 0x00,
    MUX2,
    MUX3,
    MUX4,
    MUX5,
    MUX6,
    MUX7,
    MUX8,
    MUX_COUNT,
} MUXChannel_E;

typedef enum
{
    BRD1 = 0x00,
    BRD2,
    BRD_COUNT,
} BRDChannels_E;
#endif /**< BMSW_BOARD_VA3 */

typedef struct
{
    struct
    {
        float32_t mcu;
#if defined (BMSW_BOARD_VA3)
        float32_t mux1[MUX_COUNT];
        float32_t mux2[MUX_COUNT];
        float32_t mux3[MUX_COUNT];
        float32_t board[BRD_COUNT];
#endif /**< BMSW_BOARD_VA3 */
    } temp;

    uint8_t addr;
} IO_S;


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void IO_UnpackAdcBuffer(bufferHalf_E);
