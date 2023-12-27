/**
 * @file HW_LTC2983.h
 * @brief  Header file for LTC2983 Thermistor Sensor
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-26
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_gpio.h"
#include "HW_spi.h"
#include "stdbool.h"
#include "stdint.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum {
    THERMOCOUPLE = 0x00,
    DIODE,
    RTD,
    THERMISTOR,
    DIRECTADC_SGL,
} LTC_MeasurementType_E;

typedef enum {
    CH1 = 0x00,
    CH2,
    CH3,
    CH4,
    CH5,
    CH6,
    CH7,
    CH8,
    CH9,
    CH10,
    CH11,
    CH12,
    CH13,
    CH14,
    CH15,
    CH16,
    CH17,
    CH18,
    CH19,
    CH20,
    CHANNEL_COUNT,
} LTC_Channels_E;

typedef struct {
    LTC_MeasurementType_E msmnt_type[CHANNEL_COUNT];
    uint32_t multiple_conversion_flags;

} LTC_Config_S;

typedef struct {
    bool hard_fault;
    bool soft_above;
    bool soft_below;
    bool valid;
    int32_t result;
    uint32_t raw;
} LTC2983_ADCResult_S;

typedef struct {
    HW_SPI_Device_S *dev;
    HW_GPIO_S *interrupt;
    HW_GPIO_S *nrst;
    LTC_Config_S config;
    bool measuring;
    LTC2983_ADCResult_S raw_results[CHANNEL_COUNT];
    int16_t temps[CHANNEL_COUNT]; /**< Scale of 0.1 deg C */
} LTC2983_S;


/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool LTC_Init(void);
bool LTC_StartMeasurement(void);
bool LTC_GetMeasurement(void);
