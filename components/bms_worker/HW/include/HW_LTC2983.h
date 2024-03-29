/**
 * @file HW_LTC2983.h
 * @brief  Header file for LTC2983 Thermistor Sensor
 */

#if defined(BMSW_BOARD_VA1)

# pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
# include "stdbool.h"
# include "stdint.h"

// Firmware Includes
# include "HW_gpio.h"
# include "HW_spi.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    THERMOCOUPLE = 0x00,
    DIODE,
    RTD,
    THERMISTOR,
    DIRECTADC_SGL,
} LTC_MeasurementType_E;

typedef enum
{
    LTC_CH1 = 0x00,
    LTC_CH2,
    LTC_CH3,
    LTC_CH4,
    LTC_CH5,
    LTC_CH6,
    LTC_CH7,
    LTC_CH8,
    LTC_CH9,
    LTC_CH10,
    LTC_CH11,
    LTC_CH12,
    LTC_CH13,
    LTC_CH14,
    LTC_CH15,
    LTC_CH16,
    LTC_CH17,
    LTC_CH18,
    LTC_CH19,
    LTC_CH20,
    LTC_CHANNEL_COUNT,
} LTC_Channels_E;

typedef struct
{
    LTC_MeasurementType_E msmnt_type[CHANNEL_COUNT];
    uint32_t              multiple_conversion_flags;

} LTC_Config_S;

typedef struct
{
    bool     hard_fault: 1;
    bool     soft_above: 1;
    bool     soft_below: 1;
    bool     valid     : 1;
    int32_t  result;
    uint32_t raw;
} LTC_ADCResult_S;

typedef struct
{
    HW_SPI_Device_S*    dev;
    HW_GPIO_S*          interrupt;
    HW_GPIO_S*          nrst;
    LTC_Config_S        config;
    bool                measuring;
    LTC_ADCResult_S raw_results[CHANNEL_COUNT];
    int16_t             temps[CHANNEL_COUNT]; /**< Scale of 0.1 deg C */
} LTC_S;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool LTC_init(void);
bool LTC_startMeasurement(void);
bool LTC_getMeasurement(void);

#endif /**< BMSW_BOARD_VA1 */
