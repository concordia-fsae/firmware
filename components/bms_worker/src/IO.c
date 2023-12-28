/**
 * @file IO.c
 * @brief  Source code for IO Module
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-28
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module Header */
#include "IO.h"

/**< System Includes*/
#include <string.h>

/**< HW Includes */
#include "HW_adc.h"
#include "HW_dma.h"
#include "HW_gpio.h"

/**< Other Includes */
#include "FeatureDefs.h"
#include "ModuleDesc.h"
#include "Types.h"
#include "Utility.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#if defined(BMSW_BOARD_VA1)
#define VREF                     3.3F    // [V] ADC reference voltage
#endif /**< BMSW_BOARD_VA1 */

#define ADC_BUF_LEN              64U    // number of samples to fill with DMA,
                                         // processed when half full and again when completely full
#define ADC_MAX_VAL              4095U   // Max integer value of ADC reading (2^12 for this chip)
#define TEMP_CHIP_V_PER_DEG_C    0.0043F // [V/degC] slope of built-in temp sensor
#define TEMP_CHIP_V_AT_25_C      1.43F   // [V] voltage at 25 degC

#define TEMP_CHIP_FROM_V(v)      (((v - TEMP_CHIP_V_AT_25_C) / TEMP_CHIP_V_PER_DEG_C) + 25.0F)


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
    ADC_CHANNEL_TEMP_MCU,
    ADC_CHANNEL_COUNT,
} AdcChannels_E;
// _Static_assert(ADC_CHANNEL_COUNT == hadc1.Init.NbrOfConversion);
_Static_assert(ADC_BUF_LEN % ADC_CHANNEL_COUNT == 0, "ADC Buffer Length should be a multiple of the number of ADC channels");
_Static_assert((ADC_BUF_LEN / 2) % ADC_CHANNEL_COUNT == 0, "ADC Buffer Length divided by two should be a multiple of the number of ADC channels");

typedef struct
{
    uint32_t  raw;
    float32_t value;
    uint16_t  count;
} simpleFilter_S;

typedef struct
{
    adcState_E     adcState;
    uint32_t       adcBuffer[ADC_BUF_LEN];
    simpleFilter_S adcData[ADC_CHANNEL_COUNT];
    struct
    {
        uint32_t mcu;
    } tempRaw;
} io_S;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static io_S io;

/**< GPIO descriptors */
HW_GPIO_S A0 = {
    .port = A0_GPIO_Port,
    .pin = A0_Pin,
};

HW_GPIO_S A1 = {
    .port = A1_GPIO_Port,
    .pin = A1_Pin,
};

HW_GPIO_S A2 = {
    .port = A2_GPIO_Port,
    .pin = A2_Pin,
};

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/**
 * @brief  Stores the public IO struct
 */
IO_S IO;


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


/**
 * @brief  1kHz IO periodic function
 */
static void IO1kHz_PRD(void)
{
    if (io.adcState == ADC_STATE_RUNNING)
    {
        for (uint8_t i = 0U; i < ADC_CHANNEL_COUNT; i++)
        {
            io.adcData[i].value  = ((float32_t)io.adcData[i].raw) / io.adcData[i].count;
            io.adcData[i].raw    = 0;
            io.adcData[i].count  = 0;
            io.adcData[i].value *= VREF / ADC_MAX_VAL;
        }

        IO.temp.mcu   = TEMP_CHIP_FROM_V(io.adcData[ADC_CHANNEL_TEMP_MCU].value);
    }
    else
    {
        if (io.adcState == ADC_STATE_INIT)
        {
            IO.addr |= ((HW_GPIO_ReadPin(&A0)) ? 0x01 : 0x00) << 0;
            IO.addr |= ((HW_GPIO_ReadPin(&A1)) ? 0x01 : 0x00) << 1;
            IO.addr |= ((HW_GPIO_ReadPin(&A2)) ? 0x01 : 0x00) << 2;
            
            io.adcState = ADC_STATE_CALIBRATION;
            if (HW_ADC_Calibrate(&hadc1))
            {
                HW_ADC_Start_DMA(&hadc1, (uint32_t*)&io.adcBuffer, ADC_BUF_LEN);
                io.adcState = ADC_STATE_RUNNING;
            }
            else
            {
                io.adcState = ADC_STATE_CALIBRATION_FAILED;
            }
        }
        else
        {
            // adc calibration failed
            // what do now?
        }
    }
}


/**
 * @brief  IO Module descriptor
 */
const ModuleDesc_S IO_desc = {
    .moduleInit       = &IO_init,
    .periodic1kHz_CLK = &IO1kHz_PRD,
};


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Unpack's the IO ADC Buffer
 * @param half upper or lower half of the buffer
 */
void IO_UnpackAdcBuffer(bufferHalf_E half)
{
    uint16_t startIndex = (half == BUFFER_HALF_LOWER) ? 0U : ADC_BUF_LEN / 2U;
    uint16_t endIndex   = startIndex + (ADC_BUF_LEN / 2U);

    for (uint16_t i = startIndex; i < endIndex; i++)
    {
        uint8_t channelIndex = i % ADC_CHANNEL_COUNT;
        io.adcData[channelIndex].raw += io.adcBuffer[i];
        io.adcData[channelIndex].count++;
    }
}
