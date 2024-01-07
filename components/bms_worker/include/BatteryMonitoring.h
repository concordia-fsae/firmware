/**
 * @file BatteryMonitoring.h
 * @brief  Header file for Battery Monitoring Application
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-27
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< System Includes */
#include "stdint.h"
#include "stdbool.h"

/**< Firmware Includes */
#include "HW_adc.h"

/**< Driver Includes */
#include "HW_MAX14921.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define BMS_ADC_BUF_LEN 264

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum {
    BMS_INIT = 0x00,
    BMS_WAITING,
    BMS_SAMPLING,
    BMS_DIAGNOSTIC,
    BMS_BALANCING,
    BMS_ERROR,
} BMS_State_E;

typedef enum
{
    ADC_CELL_MEASUREMENT = 0x00,
    ADC_CHANNEL_COUNT,
} AdcChannels_E;

typedef struct {
    bool measurement_complete;
    uint32_t adc_buffer[BMS_ADC_BUF_LEN];
    simpleFilter_S cell_voltages[ADC_CHANNEL_COUNT];
} BMS_Data_S;

typedef struct {
    BMS_State_E state;
    uint16_t balancing_cells;
    uint16_t cell_faults;
    uint16_t cell_voltages[CELL_COUNT];
    uint16_t max_voltage;
    uint16_t min_voltage;
    uint16_t avg_voltage;
    uint16_t min_capacity;
    uint16_t max_capacity;
    uint16_t avg_capacity;
    BMS_Data_S data;
} BMS_S;

/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void BMS_UnpackADCBuffer(bufferHalf_E);
