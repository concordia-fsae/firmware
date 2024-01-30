/**
 * @file BatteryMonitoring.h
 * @brief  Header file for Battery Monitoring Application
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< System Includes */
#include "stdbool.h"
#include "stdint.h"

/**< Firmware Includes */
#include "HW_adc.h"

/**< Driver Includes */
#include "HW_MAX14921.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    BMS_INIT = 0x00,
    BMS_CALIBRATING,
    BMS_PARASITIC,
    BMS_PARASITIC_MEASUREMENT,
    BMS_HOLDING,
    BMS_WAITING,
    BMS_SAMPLING,
    BMS_DIAGNOSTIC,
    BMS_BALANCING,
    BMS_ERROR,
} BMS_State_E;

typedef enum
{
    CELL_DISCONNECTED = 0x00,
    CELL_CONNECTED,
    CELL_ERROR,
} BMS_Cell_E;

typedef struct
{
    uint16_t   voltage;
    uint16_t   capacity;
    uint16_t   parasitic_corr;
    BMS_Cell_E state;
} BMS_Cell_S;

typedef struct
{
    BMS_State_E state;
    uint16_t    balancing_cells;
    BMS_Cell_S  cells[CELL_COUNT];       /**< Stored in 0.0001 units */
    uint16_t    pack_voltage;            /**< Stored in 1mV units */
    uint16_t    calculated_pack_voltage; /**< Stored in 1mV units */
    uint16_t    max_voltage;             /**< Stored in 0.0001 units */
    uint16_t    min_voltage;             /**< Stored in 0.0001 units */
    uint16_t    avg_voltage;             /**< Stored in 0.0001 units */
    uint16_t    min_capacity;            /**< Stored in 0.0001 units */
    uint16_t    max_capacity;            /**< Stored in 0.0001 units */
    uint16_t    avg_capacity;            /**< Stored in 0.0001 units */
    uint8_t     connected_cells;
} BMS_S;


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern BMS_S BMS;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void BMS_SetOutputCell(MAX_SelectedCell_E cell);
void BMS_MeasurementComplete(void);
