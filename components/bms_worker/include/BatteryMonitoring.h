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
    BMS_CELL_DISCONNECTED = 0x00,
    BMS_CELL_CONNECTED,
    BMS_CELL_ERROR,
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
    BMS_Cell_S  cells[MAX_CELL_COUNT];      // [mv], precision 1mv
    uint16_t    pack_voltage;               // [mv], precision 1mv
    uint16_t    calculated_pack_voltage;    // [mv], precision 1mv
    struct
    {
        uint16_t max;
        uint16_t min;
        uint16_t avg;
    } voltage;    // [0.1mv], precision 0.1mv
    struct
    {
        uint16_t min;
        uint16_t max;
        uint16_t avg;
    } capacity;    // [0.1mAh], precision 0.1mAh
    uint8_t connected_cells;
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
