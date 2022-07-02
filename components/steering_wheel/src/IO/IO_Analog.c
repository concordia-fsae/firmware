/**
 * IO_Analog.c
 * The Analog IO module controls interactions with the chip analog IO
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Module header
#include "IO/IO_Analog.h"

// System includes
#include <string.h>

// HW includes
#include "HW_adc.h"
#include "HW_dma.h"
#include "HW_gpio.h"
//
// other includes
#include "Display/Common.h"
#include "FeatureDefs.h"
#include "ModuleDesc.h"
#include "Types.h"
#include "Utility.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define VREF                     3.3F    // [V] ADC reference voltage
#define ADC_BUF_LEN              264U    // number of samples to fill with DMA,
                                         // processed when half full and again when completely full
#define ADC_MAX_VAL              4095U   // Max integer value of ADC reading (2^12 for this chip)
#define SENSE_RESISTOR_OHMS      0.05F   // [Ohms] sense resistor resistance, for measuring current consumption
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
    ADC_CHANNEL_CURRENT_SENSE = 0U,
    ADC_CHANNEL_TEMP_BOARD,
    ADC_CHANNEL_TEMP_GPU,
    ADC_CHANNEL_PADDLE_LEFT,
    ADC_CHANNEL_PADDLE_RIGHT,
    ADC_CHANNEL_TEMP_MCU,
    ADC_CHANNEL_COUNT,
} AdcChannels_E;
// _Static_assert(ADC_CHANNEL_COUNT == hadc1.Init.NbrOfConversion);
_Static_assert(ADC_BUF_LEN % ADC_CHANNEL_COUNT == 0, "ADC Buffer Length should be a multiple of the number of ADC channels");
_Static_assert((ADC_BUF_LEN / 2) % ADC_CHANNEL_COUNT == 0, "ADC Buffer Length divided by two should be a multiple of the number of ADC channels");

typedef enum
{
    BUFFER_HALF_LOWER = 0U,
    BUFFER_HALF_UPPER,
} bufferHalf_E;

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

    uint32_t       instCurrentRaw;
    struct
    {
        uint32_t board;
        uint32_t gpu;
        uint32_t mcu;
    } tempRaw;

#if FTR_HALL_EFFECT_PADDLES
    struct
    {
        float32_t left;
        float32_t right;
    } paddlesRaw;
#endif // if FTR_HALL_EFFECT_PADDLES
} io_analog_S;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static io_analog_S io_analog;


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

IO_Analog_S IO_ANALOG;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * unpackAdcBuffer
 * @param half upper or lower half of the buffer
 */
static void unpackAdcBuffer(bufferHalf_E half)
{
    uint16_t startIndex = (half == BUFFER_HALF_LOWER) ? 0U : ADC_BUF_LEN / 2U;
    uint16_t endIndex   = startIndex + (ADC_BUF_LEN / 2U);

    for (uint16_t i = startIndex; i < endIndex; i++)
    {
        uint8_t channelIndex = i % ADC_CHANNEL_COUNT;
        io_analog.adcData[channelIndex].raw += io_analog.adcBuffer[i];
        io_analog.adcData[channelIndex].count++;
    }
}


/**
 * IO_init
 *
 */
static void IO_Analog_init(void)
{
    // initialize structs
    memset(&io_analog, 0x00, sizeof(io_analog));
    memset(&IO_ANALOG, 0x00, sizeof(IO_ANALOG));
}


/**
 * IO_ANALOG_1kHz_PRD
 *
 */
static void IO_Analog_1kHz_PRD(void)
{
    if (io_analog.adcState == ADC_STATE_RUNNING)
    {
        for (uint8_t i = 0U; i < ADC_CHANNEL_COUNT; i++)
        {
            io_analog.adcData[i].value  = ((float32_t)io_analog.adcData[i].raw) / io_analog.adcData[i].count;
            io_analog.adcData[i].raw    = 0;
            io_analog.adcData[i].count  = 0;
            io_analog.adcData[i].value *= VREF / ADC_MAX_VAL;
        }

        // this doesn't work on board rev 1 because I designed it wrong
        IO_ANALOG.instCurrent = io_analog.adcData[ADC_CHANNEL_CURRENT_SENSE].value / SENSE_RESISTOR_OHMS;

        IO_ANALOG.temp.board = io_analog.adcData[ADC_CHANNEL_TEMP_BOARD].value;
        IO_ANALOG.temp.gpu   = io_analog.adcData[ADC_CHANNEL_TEMP_GPU].value;
        IO_ANALOG.temp.mcu   = TEMP_CHIP_FROM_V(io_analog.adcData[ADC_CHANNEL_TEMP_MCU].value);

#if FTR_HALL_EFFECT_PADDLES
        io_analog.paddlesRaw.left  = io_analog.adcData[ADC_CHANNEL_PADDLE_LEFT].value;
        io_analog.paddlesRaw.right = io_analog.adcData[ADC_CHANNEL_PADDLE_RIGHT].value;
#endif

        static uint8_t tim = 0;
        if (++tim == 100U)
        {
            toggleInfoDotState(INFO_DOT_ADC_CONV);
            tim = 0;
        }
    }
    else
    {
        if (io_analog.adcState == ADC_STATE_INIT)
        {
            io_analog.adcState = ADC_STATE_CALIBRATION;
            if (HAL_ADCEx_Calibration_Start(&hadc1) == HAL_OK)
            {
                HAL_ADC_Start_DMA(&hadc1, (uint32_t*)io_analog.adcBuffer, ADC_BUF_LEN);
                io_analog.adcState = ADC_STATE_RUNNING;
            }
            else
            {
                io_analog.adcState = ADC_STATE_CALIBRATION_FAILED;
            }
        }
        else
        {
            // adc calibration failed
            // what do now?
        }
    }
}


// module description
const ModuleDesc_S IO_Analog_desc = {
    .moduleInit       = &IO_Analog_init,
    .periodic1kHz_CLK = &IO_Analog_1kHz_PRD,
};


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

// Called when first half of buffer is filled
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
    UNUSED(hadc);
    unpackAdcBuffer(BUFFER_HALF_LOWER);
}

// Called when buffer is completely filled
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    UNUSED(hadc);
    unpackAdcBuffer(BUFFER_HALF_UPPER);
}
