/**
 * @file IO.c
 * @brief  Source code for IO Module
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module Header */
#include "drv_inputAD_private.h"

/**< System Includes*/
#include <string.h>

/**< Firmware Includes */
#include "HW.h"
#include "HW_adc.h"
#include "HW_dma.h"
#include "HW_gpio.h"

/**< Other Includes */
#include "ModuleDesc.h"
#include "LIB_simpleFilter.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    HW_adc_state_E     adcState;
    uint32_t           adcBuffer[DRV_INPUTAD_ADC_BUF_LEN];
    LIB_simpleFilter_S adcData_bank1[ADC_BANK1_CHANNEL_COUNT];
    LIB_simpleFilter_S adcData_bank2[ADC_BANK2_CHANNEL_COUNT];
} io_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

drv_inputAD_configDigital_S drv_inputAD_configDigital[DRV_INPUTAD_DIGITAL_COUNT] = {
    [DRV_INPUTAD_DIGITAL_TSMS_CHG] = {
        .pin = HW_GPIO_TSMS_CHG,
    },
    [DRV_INPUTAD_DIGITAL_OK_HS] = {
        .pin = HW_GPIO_OK_HS,
    },
    [DRV_INPUTAD_DIGITAL_BMS_IMD_RESET] = {
        .pin = HW_GPIO_BMS_STATUS_MEM,
    },
    [DRV_INPUTAD_DIGITAL_IMD_STATUS_MEM] = {
        .pin = HW_GPIO_IMD_STATUS_MEM,
    },
    [DRV_INPUTAD_DIGITAL_BMS_STATUS_MEM] = {
        .pin = HW_GPIO_BMS_STATUS_MEM,
    },
};
static io_S io;

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void drv_inputAD_private_unpackADCBuffer(void);

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  IO Module Init function
 */
static void drv_inputAD_init_componentSpecific(void)
{
    memset(&io, 0x00, sizeof(io));

    drv_inputAD_private_init();

    if ((HW_ADC_calibrate(&hadc1) == HW_OK) && (HW_ADC_calibrate(&hadc2) == HW_OK))
    {
        HW_ADC_startDMA(&hadc1, (uint32_t*)&io.adcBuffer, DRV_INPUTAD_ADC_BUF_LEN);
        io.adcState = ADC_STATE_RUNNING;
    }
    else
    {
        io.adcState = ADC_STATE_CALIBRATION_FAILED;
    }
}

static void drv_inputAD_1kHz_PRD(void)
{
    if (io.adcState == ADC_STATE_CALIBRATION_FAILED)
    {
        // adc calibration failed
        // what do now?
    }
    else if (io.adcState == ADC_STATE_RUNNING)
    {
        drv_inputAD_private_unpackADCBuffer();
        drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_MCU_TEMP,
                                    HW_ADC_getVFromCount((uint16_t)io.adcData_bank1[ADC_BANK1_CHANNEL_MCU_TEMP].value));

        const float32_t differential = HW_ADC_getVFromCount((uint16_t)(io.adcData_bank2[ADC_BANK2_CHANNEL_CS_P].value - io.adcData_bank1[ADC_BANK1_CHANNEL_CS_N].value));
        drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_CS, differential);
    }

    drv_inputAD_private_run();
}

/**
 * @brief  IO Module descriptor
 */
const ModuleDesc_S IO_desc = {
    .moduleInit       = &drv_inputAD_init_componentSpecific,
    .periodic1kHz_CLK = &drv_inputAD_1kHz_PRD,
};

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Unpack ADC buffer
 */
void drv_inputAD_private_unpackADCBuffer(void)
{
    for (uint8_t i = 0; i < ADC_BANK1_CHANNEL_COUNT; i++)
    {
        LIB_simpleFilter_clear(&io.adcData_bank1[i]);
    }
    for (uint8_t i = 0; i < ADC_BANK2_CHANNEL_COUNT; i++)
    {
        LIB_simpleFilter_clear(&io.adcData_bank2[i]);
    }

    for (uint16_t i = 0; i < DRV_INPUTAD_ADC_BUF_LEN; i++)
    {
        LIB_simpleFilter_increment(&io.adcData_bank1[i % ADC_BANK1_CHANNEL_COUNT], (io.adcBuffer[i] & 0xffff));
        LIB_simpleFilter_increment(&io.adcData_bank2[i % ADC_BANK2_CHANNEL_COUNT], (io.adcBuffer[i] >> 16U));
    }

    for (uint8_t i = 0; i < ADC_BANK1_CHANNEL_COUNT; i++)
    {
        LIB_simpleFilter_average(&io.adcData_bank1[i]);
    }
    for (uint8_t i = 0; i < ADC_BANK2_CHANNEL_COUNT; i++)
    {
        LIB_simpleFilter_average(&io.adcData_bank2[i]);
    }
}
