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
#include "HW_tim.h"

/**< Other Includes */
#include "ModuleDesc.h"
#include "Utility.h"
#include "FeatureDefines_generated.h"
#include "LIB_simpleFilter.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define ADC_VOLTAGE_DIVISION    2U /**< Voltage division for cell voltage output */

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
    ADC_CHANNEL_TEMP_MCU = 0x00,
    ADC_CHANNEL_TEMP_MUX1,
    ADC_CHANNEL_TEMP_MUX2,
    ADC_CHANNEL_TEMP_MUX3,
    ADC_CHANNEL_TEMP_BRD1,
    ADC_CHANNEL_TEMP_BRD2,
    ADC_CHANNEL_COUNT,
} adcChannels_E;
_Static_assert(IO_ADC_BUF_LEN % ADC_CHANNEL_COUNT == 0,       "ADC Buffer Length should be a multiple of the number of ADC channels");
_Static_assert((IO_ADC_BUF_LEN / 2) % ADC_CHANNEL_COUNT == 0, "ADC Buffer Length divided by two should be a multiple of the number of ADC channels");

typedef struct
{
    adcState_E         adcState;
    uint32_t           adcBuffer[IO_ADC_BUF_LEN];
    LIB_simpleFilter_S adcData[ADC_CHANNEL_COUNT];
    LIB_simpleFilter_S bmsData;
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

void IO_Temps_unpackADCBuffer(void);
void IO_Cells_unpackADCBuffer(void);


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

    NX3L_init();
    NX3L_enableMux();
    NX3L_setMux(NX3L_MUX1);
}

static void IO10Hz_PRD(void)
{
    static NX3L_MUXChannel_E current_sel = NX3L_MUX1;

    if (io.adcState != ADC_STATE_RUNNING)
    {
        if (io.adcState == ADC_STATE_CALIBRATION_FAILED)
        {
            // adc calibration failed
            // what do now?
        }
    }
    else
    {
        IO_Temps_unpackADCBuffer();

        for (uint8_t i = 0; i < ADC_CHANNEL_COUNT; i++)
        {
            io.adcData[i].value  = (io.adcData[i].count != 0) ? ((float32_t)io.adcData[i].raw) / io.adcData[i].count : 0;
            io.adcData[i].value *= ADC_REF_VOLTAGE / ADC_MAX_VAL;
        }

        IO.temp.mcu               = io.adcData[ADC_CHANNEL_TEMP_MCU].value;
        IO.temp.board[BRD1]       = io.adcData[ADC_CHANNEL_TEMP_BRD1].value;
        IO.temp.board[BRD2]       = io.adcData[ADC_CHANNEL_TEMP_BRD2].value;

        IO.temp.mux1[current_sel] = io.adcData[ADC_CHANNEL_TEMP_MUX1].value;
        IO.temp.mux2[current_sel] = io.adcData[ADC_CHANNEL_TEMP_MUX2].value;
        IO.temp.mux3[current_sel] = io.adcData[ADC_CHANNEL_TEMP_MUX3].value;

        if (++current_sel == NX3L_MUX_COUNT)
        {
            current_sel = NX3L_MUX1;
        }

        NX3L_setMux(current_sel);
    }
}

#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK
/**
 * @brief  10kHz IO periodic function
 */
static void IO10kHz_PRD(void)
#else // FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK
/**
 * @brief  10kHz IO periodic function
 */
void IO10kHz_CB(void)
#endif // not FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK
{
    if (io.adcState == ADC_STATE_RUNNING)
    {
        if ((BMS.state == BMS_HOLDING) || (BMS.state == BMS_PARASITIC_MEASUREMENT))
        {
            const MAX_selectedCell_E current_cell = BMS_getCurrentOutputCell();

            IO_Cells_unpackADCBuffer();

            io.bmsData.value      = (io.bmsData.count != 0) ? ((float32_t)io.bmsData.raw) / io.bmsData.count : 0;
            io.bmsData.value     *= ADC_VOLTAGE_DIVISION * ADC_REF_VOLTAGE / ADC_MAX_VAL;

            IO.cell[current_cell] = io.bmsData.value;

            if (current_cell == MAX_CELL1)
            {
                BMS_measurementComplete();
            }
            else
            {
                BMS_setOutputCell(current_cell - 1);
#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
                HW_TIM_10kHz_timerStart();
#endif // FEATUFEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
            }
        }
        else if (BMS.state == BMS_SAMPLING)
        {
            IO_Cells_unpackADCBuffer();
            if (io.bmsData.count != 0)
            {
                io.bmsData.value  = ((float32_t)io.bmsData.raw / io.bmsData.count);
                io.bmsData.value *= (ADC_VOLTAGE_DIVISION * ADC_REF_VOLTAGE) / ADC_MAX_VAL;
            }

            IO.segment        = io.bmsData.value;
        }
    }
}


/**
 * @brief  IO Module descriptor
 */
const ModuleDesc_S IO_desc = {
    .moduleInit       = &IO_init,
    .periodic10Hz_CLK = &IO10Hz_PRD,
#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK
    .periodic10kHz_CLK = &IO10kHz_PRD,
#endif // FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK
};

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Unpack ADC buffer for the thermistor measurements
 */
void IO_Temps_unpackADCBuffer(void)
{
    for (uint8_t i = 0; i < ADC_CHANNEL_COUNT; i++)
    {
        LIB_simpleFilter_clear(&io.adcData[i]);
    }

    for (uint16_t i = 0; i < IO_ADC_BUF_LEN; i++)
    {
        LIB_simpleFilter_increment(&io.adcData[i % ADC_CHANNEL_COUNT], (float32_t)(io.adcBuffer[i] & 0xffff));
    }
}

/**
 * @brief  Unpack ADC buffer for the cell measurements
 */
void IO_Cells_unpackADCBuffer(void)
{
    LIB_simpleFilter_clear(&io.bmsData);

    for (uint16_t i = 0; i < IO_ADC_BUF_LEN; i++)
    {
        LIB_simpleFilter_increment(&io.bmsData, (float32_t)(io.adcBuffer[i] >> 16U));
    }
}
