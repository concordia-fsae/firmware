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
    adcState_E     adcState;
    uint32_t       adcBuffer[IO_ADC_BUF_LEN];
    simpleFilter_S adcData[ADC_CHANNEL_COUNT];
} io_S;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static io_S io;

HW_GPIO_S TSMS = {
    .pin  = TSMS_CHG_Pin,
    .port = TSMS_CHG_Port,
};

HW_GPIO_S IMD_OK = {
    .pin  = OK_HS_Pin,
    .port = OK_HS_Port,
};

HW_GPIO_S imd_feedback = {
    .pin  = IMD_STATUS_Pin,
    .port = IMD_STATUS_Port,
};

HW_GPIO_S bms_feedback = {
    .pin  = BMS_STATUS_Pin,
    .port = BMS_STATUS_Port,
};

HW_GPIO_S bms_imd_reset = {
    .pin = BMS_IMD_Reset_Pin,
    .port = BMS_IMD_Reset_Port,
};

HW_GPIO_S bms_status_mem = {
    .pin = BMS_STATUS_MEM_Pin,
    .port = BMS_STATUS_MEM_Port,
};


HW_GPIO_S imd_status_mem = {
    .pin = IMD_STATUS_MEM_Pin,
    .port = IMD_STATUS_MEM_Port,
};



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

static void IO100Hz_PRD(void)
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

    IO.master_switch = (HW_GPIO_readPin(&TSMS)) ? true : false;
    IO.imd_ok = (HW_GPIO_readPin(&IMD_OK)) ? true : false;
    IO.feedback_sfty_bms = (HW_GPIO_readPin(&bms_feedback)) ? true : false;
    IO.feedback_sfty_imd = (HW_GPIO_readPin(&imd_feedback)) ? true : false;
    IO.bms_imd_reset = (HW_GPIO_readPin(&bms_imd_reset)) ? true : false;
    IO.bms_status_mem = (HW_GPIO_readPin(&bms_status_mem)) ? true : false;
    IO.imd_status_mem = (HW_GPIO_readPin(&imd_status_mem)) ? true : false;
}

/**
 * @brief  IO Module descriptor
 */
const ModuleDesc_S IO_desc = {
    .moduleInit       = &IO_init,
    .periodic100Hz_CLK = &IO100Hz_PRD,
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
        if (i % 2 == 0)
        {
            io.adcData[ADC_CURRENT_N].raw += io.adcBuffer[i] & 0xffff;
            io.adcData[ADC_CURRENT_P].raw += (io.adcBuffer[i]) >> 16;
            io.adcData[ADC_CURRENT_N].count++;
            io.adcData[ADC_CURRENT_P].count++;
        }
        else
        {
            io.adcData[ADC_MCU_TEMP].raw += io.adcBuffer[i] & 0xffff;
            io.adcData[ADC_MCU_TEMP].count++;
        }
    }

    io.adcData[ADC_CURRENT_N].value = (float32_t)io.adcData[ADC_CURRENT_N].raw / io.adcData[ADC_CURRENT_N].count;
    io.adcData[ADC_CURRENT_P].value = (float32_t)io.adcData[ADC_CURRENT_P].raw / io.adcData[ADC_CURRENT_P].count;
    io.adcData[ADC_MCU_TEMP].value  = (float32_t)io.adcData[ADC_MCU_TEMP].raw / io.adcData[ADC_MCU_TEMP].count;
}
