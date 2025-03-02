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
#include "LIB_simpleFilter.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

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
    ADC_CURRENT_N = 0x00,
    ADC_CURRENT_P,
    ADC_MCU_TEMP,
    ADC_CHANNEL_COUNT,
} adcChannels_E;
_Static_assert(IO_ADC_BUF_LEN % ADC_CHANNEL_COUNT == 0, "ADC Buffer Length should be a multiple of the number of ADC channels");
_Static_assert((IO_ADC_BUF_LEN / 2) % ADC_CHANNEL_COUNT == 0, "ADC Buffer Length divided by two should be a multiple of the number of ADC channels");

typedef struct
{
    adcState_E         adcState;
    uint32_t           adcBuffer[IO_ADC_BUF_LEN];
    LIB_simpleFilter_S adcData[ADC_CHANNEL_COUNT];
} io_S;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static io_S io;

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
    if (io.adcState == ADC_STATE_INIT)
    {
        io.adcState = ADC_STATE_CALIBRATION;
        if (HW_ADC_calibrate(&hadc1) && HW_ADC_calibrate(&hadc2))
        {
            HW_ADC_startDMA(&hadc1, (uint32_t*)&io.adcBuffer, IO_ADC_BUF_LEN);
            io.adcState = ADC_STATE_RUNNING;
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
    else if (io.adcState == ADC_STATE_RUNNING)
    {
        IO_unpackADCBuffer();
        IO.current  = (io.adcData[ADC_CURRENT_P].value - io.adcData[ADC_CURRENT_N].value) / ADC_MAX_VAL * VREF;
        IO.mcu_temp = (io.adcData[ADC_MCU_TEMP].value / ADC_MAX_VAL) * VREF;
    }

    IO.master_switch = (HW_GPIO_readPin(HW_GPIO_TSMS_CHG)) ? true : false;
    IO.imd_ok = (HW_GPIO_readPin(HW_GPIO_OK_HS)) ? true : false;
    IO.feedback_sfty_bms = (HW_GPIO_readPin(HW_GPIO_BMS_STATUS_MEM)) ? true : false;
    IO.feedback_sfty_imd = (HW_GPIO_readPin(HW_GPIO_IMD_STATUS_MEM)) ? true : false;
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
        LIB_simpleFilter_clear(&io.adcData[i]);
    }

    for (uint16_t i = 0; i < IO_ADC_BUF_LEN; i++)
    {
        if (i % 2 == 0)
        {
            LIB_simpleFilter_increment(&io.adcData[ADC_CURRENT_N], (float32_t)(io.adcBuffer[i] & 0xffff));
            LIB_simpleFilter_increment(&io.adcData[ADC_CURRENT_P], (float32_t)(io.adcBuffer[i] >> 16U));
        }
        else
        {
            LIB_simpleFilter_increment(&io.adcData[ADC_MCU_TEMP], (float32_t)(io.adcBuffer[i] & 0xffff));
        }
    }

    LIB_simpleFilter_average(&io.adcData[ADC_CURRENT_N]);
    LIB_simpleFilter_average(&io.adcData[ADC_CURRENT_P]);
    LIB_simpleFilter_average(&io.adcData[ADC_MCU_TEMP]);
}
