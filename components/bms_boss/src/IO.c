/**
 * @file IO.c
 * @brief  Source code for IO Module
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module Header */
#include "IO.h"

/**< System Includes*/
#include "FeatureDefs.h"
#include "Types.h"
#include <stdint.h>
#include <string.h>

/**< Firmware Includes */
#include "HW.h"
#include "HW_adc.h"
#include "HW_dma.h"
#include "HW_gpio.h"

/**< Other Includes */
#include "ModuleDesc.h"
#include "Utility.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define ADC_VOLTAGE_DIVISION 2U /**< Voltage division for cell voltage output */


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    ADC_STATE_INIT = 0,
    ADC_STATE_CALIBRATION,
    ADC_STATE_RUNNING,
    ADC_STATE_CALIBRATION_FAILED,
    ADC_STATE_COUNT,
} adcState_E;

typedef enum
{
    ADC_CURRENT_P = 0x00,
    ADC_CURRENT_N,
    ADC_CHANNEL_COUNT,
} adcChannels_E;
_Static_assert(IO_ADC_BUF_LEN % ADC_CHANNEL_COUNT == 0, "ADC Buffer Length should be a multiple of the number of ADC channels");
_Static_assert((IO_ADC_BUF_LEN / 2) % ADC_CHANNEL_COUNT == 0, "ADC Buffer Length divided by two should be a multiple of the number of ADC channels");

typedef struct
{
    adcState_E     adcState;
    uint32_t       adcBuffer[IO_ADC_BUF_LEN];
    simpleFilter_S adcData[ADC_CHANNEL_COUNT];
} io_S;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static io_S io;

/**< GPIO descriptors */
#if defined(BMSW_BOARD_VA3)
HW_GPIO_S A3 = {
    .port = A3_GPIO_Port,
    .pin  = A3_Pin,
};
#endif /**< BMSW_BOARD_VA3 */

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/**
 * @brief  Stores the public IO struct
 */
IO_S IO;


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void IO_unpackADCBuffer(void);


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  IO Module Init function
 */
static void IO_init(void)
{
    memset(&io, 0x00, sizeof(io));
    memset(&IO, 0x00, sizeof(IO));
}

static void IO10Hz_PRD(void)
{
}

/**
 * @brief  IO Module descriptor
 */
const ModuleDesc_S IO_desc = {
    .moduleInit       = &IO_init,
    .periodic10Hz_CLK = &IO10Hz_PRD,
};

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Unpack ADC buffer
 */
void IO_unpackADCBuffer(void)
{
    for (uint8_t i = 0; i < ADC_CHANNEL_COUNT; i++)
    {
        io.adcData[i].raw   = 0;
        io.adcData[i].count = 0;
    }

    for (uint16_t i = 0; i < IO_ADC_BUF_LEN; i++)
    {
        io.adcData[i % ADC_CHANNEL_COUNT].raw += io.adcBuffer[i] & 0xffff;
        io.adcData[i % ADC_CHANNEL_COUNT].count++;
    }
}
