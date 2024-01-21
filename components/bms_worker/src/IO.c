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

/**< Firmware Includes */
#include "HW.h"
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
# define VREF 3.3F            // [V] Bluepill reference voltage
#elif defined(BMSW_BOARD_VA3) /**< BMSW_BOARD_VA1 */
# define VREF 3.0F            /**< Shunt Diode reference voltage */
#endif                        /**< BMSW_BOARD_VA3 */

#define ADC_MAX_VAL           4095U      // Max integer value of ADC reading (2^12 for this chip)
#define TEMP_CHIP_V_PER_DEG_C 0.0043F    // [V/degC] slope of built-in temp sensor
#define TEMP_CHIP_V_AT_25_C   1.43F      // [V] voltage at 25 degC

#define TEMP_CHIP_FROM_V(v) (((v - TEMP_CHIP_V_AT_25_C) / TEMP_CHIP_V_PER_DEG_C) + 25.0F)


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
#if defined(BMSW_BOARD_VA3)
    A1_INDEX = 0x00,
    A2_INDEX,
    A3_INDEX,
#elif defined(BMSW_BOARD_VA1) /**< BMSW_BOARD_VA3 */
    A0_INDEX = 0x00,
    A1_INDEX,
    A2_INDEX,
#endif
} AddressIndex_E;

typedef enum
{
    ADC_STATE_INIT = 0,
    ADC_STATE_CALIBRATION,
    ADC_STATE_RUNNING,
    ADC_STATE_MEASURING,
    ADC_STATE_MEASURED,
    ADC_STATE_CALIBRATION_FAILED,
    ADC_STATE_COUNT,
} adcState_E;

typedef enum
{
    ADC_CHANNEL_TEMP_MCU = 0x00,
    ADC_CHANNEL_TEMP_MUX1,
    ADC_CHANNEL_TEMP_MUX2,
    ADC_CHANNEL_TEMP_MUX3,
    ADC_CHANNEL_TEMP_BRD1,
    ADC_CHANNEL_TEMP_BRD2,
    ADC_CHANNEL_COUNT,
} AdcChannels_E;
// _Static_assert(ADC_CHANNEL_COUNT == hadc1.Init.NbrOfConversion);
_Static_assert(IO_ADC_BUF_LEN % ADC_CHANNEL_COUNT == 0, "ADC Buffer Length should be a multiple of the number of ADC channels");
_Static_assert((IO_ADC_BUF_LEN / 2) % ADC_CHANNEL_COUNT == 0, "ADC Buffer Length divided by two should be a multiple of the number of ADC channels");

typedef struct
{
    adcState_E     adcState;
    uint32_t       adcBuffer[IO_ADC_BUF_LEN];
    simpleFilter_S adcData[ADC_CHANNEL_COUNT];
    struct
    {
        uint32_t mcu;
        uint32_t brd1;
        uint32_t brd2;
        uint32_t mux1;
        uint32_t mux2;
        uint32_t mux3;
    } tempRaw;
} io_S;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static io_S io;

/**< GPIO descriptors */
#if defined(BMSW_BOARD_VA1)
HW_GPIO_S A0 = {
    .port = A0_GPIO_Port,
    .pin  = A0_Pin,
};
#endif /**< BMSW_BOARD_VA1 */
HW_GPIO_S A1 = {
    .port = A1_GPIO_Port,
    .pin  = A1_Pin,
};

HW_GPIO_S A2 = {
    .port = A2_GPIO_Port,
    .pin  = A2_Pin,
};
#if defined(BMSW_BOARD_VA3)
HW_GPIO_S A3 = {
    .port = A3_GPIO_Port,
    .pin  = A3_Pin,
};
HW_GPIO_S S1 = {
    .port = MUX_SEL1_Port,
    .pin  = MUX_SEL1_Pin,
};
HW_GPIO_S S2 = {
    .port = MUX_SEL2_Port,
    .pin  = MUX_SEL2_Pin,
};
HW_GPIO_S S3 = {
    .port = MUX_SEL3_Port,
    .pin  = MUX_SEL3_Pin,
};
HW_GPIO_S MUX_NEn = {
    .port = NX3_NEN_Port,
    .pin  = NX3_NEN_Pin,
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

#if defined(BMSW_BOARD_VA3)
void IO_SetMux(MUXChannel_E);
void IO_EnableMux(void);
#endif /**< BMSW_BOARD_VA3 */


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

#if defined(BMSW_BOARD_VA1)
    IO.addr |= ((HW_GPIO_ReadPin(&A0)) ? 0x01 : 0x00) << 0;
    IO.addr |= ((HW_GPIO_ReadPin(&A1)) ? 0x01 : 0x00) << 1;
    IO.addr |= ((HW_GPIO_ReadPin(&A2)) ? 0x01 : 0x00) << 2;
#elif defined(BMSW_BOARD_VA3)
    IO.addr |= ((HW_GPIO_ReadPin(&A1)) ? 0x01 : 0x00) << 0;
    IO.addr |= ((HW_GPIO_ReadPin(&A2)) ? 0x01 : 0x00) << 1;
    IO.addr |= ((HW_GPIO_ReadPin(&A3)) ? 0x01 : 0x00) << 2;
#endif /**< BMSW_BOARD_VA3 */

    IO_EnableMux();
    IO_SetMux(MUX1);
}

/**
 * @brief  1kHz IO periodic function
 */
static void IO100Hz_PRD(void)
{
    if (io.adcState == ADC_STATE_RUNNING)
    {
        static MUXChannel_E current_sel = MUX1;
        for (uint8_t i = 0U; i < ADC_CHANNEL_COUNT; i++)
        {
            io.adcData[i].value = ((float32_t)io.adcData[i].raw) / io.adcData[i].count;
            io.adcData[i].raw   = 0;
            io.adcData[i].count = 0;
            io.adcData[i].value *= VREF / ADC_MAX_VAL;
        }

        IO.temp.mcu         = TEMP_CHIP_FROM_V(io.adcData[ADC_CHANNEL_TEMP_MCU].value);
        IO.temp.board[BRD1] = io.adcData[ADC_CHANNEL_TEMP_BRD1].value;
        IO.temp.board[BRD2] = io.adcData[ADC_CHANNEL_TEMP_BRD2].value;

        IO.temp.mux1[current_sel] = io.adcData[ADC_CHANNEL_TEMP_MUX1].value;
        IO.temp.mux2[current_sel] = io.adcData[ADC_CHANNEL_TEMP_MUX2].value;
        IO.temp.mux3[current_sel] = io.adcData[ADC_CHANNEL_TEMP_MUX3].value;

        IO_SetMux(++current_sel % MUX_COUNT);

        if (HW_ADC_Request_DMA(ADC_REQUEST_IO, (uint32_t*)&io.adcBuffer))
            io.adcState = ADC_STATE_MEASURING;
    }
    else if (io.adcState == ADC_STATE_MEASURED)
    {
        io.adcState = ADC_STATE_RUNNING;
    }
    else
    {
        if (io.adcState == ADC_STATE_INIT)
        {
            io.adcState = ADC_STATE_CALIBRATION;
            if (HW_ADC_Calibrate(&hadc1))
            {
                // HW_ADC_Start_DMA(&hadc1, (uint32_t*)&io.adcBuffer, IO_ADC_BUF_LEN);
                HW_ADC_Request_DMA(ADC_REQUEST_IO, (uint32_t*)&io.adcBuffer);
                io.adcState = ADC_STATE_MEASURING;
            }
            else
            {
                io.adcState = ADC_STATE_CALIBRATION_FAILED;
            }
        }
        else if (io.adcState == ADC_STATE_CALIBRATION_FAILED)
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
    .periodic100Hz_CLK = &IO100Hz_PRD,
};

/**
 * @brief Unpack's the IO ADC Buffer
 * @param half upper or lower half of the buffer
 */
void IO_UnpackAdcBuffer(bufferHalf_E half)
{
    uint16_t startIndex = (half == BUFFER_HALF_LOWER) ? 0U : IO_ADC_BUF_LEN / 2U;
    uint16_t endIndex   = startIndex + (IO_ADC_BUF_LEN / 2U);

    for (uint16_t i = startIndex; i < endIndex; i++)
    {
        uint8_t channelIndex = i % ADC_CHANNEL_COUNT;
        io.adcData[channelIndex].raw += io.adcBuffer[i];
        io.adcData[channelIndex].count++;
    }

    if (half == BUFFER_HALF_UPPER)
        io.adcState = ADC_STATE_MEASURED;
}


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

#if defined(BMSW_BOARD_VA3)
void IO_SetMux(MUXChannel_E chn)
{
    HW_GPIO_WritePin(&S1, (chn & 0x01) ? true : false);
    HW_GPIO_WritePin(&S2, (chn & (0x01 << 1)) ? true : false);
    HW_GPIO_WritePin(&S3, (chn & (0x01 << 2)) ? true : false);
}

void IO_EnableMux(void)
{
    HW_GPIO_WritePin(&MUX_NEn, false);
}
#endif /**< BMSW_BOARD_VA3 */
