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
#if FEATURE_IS_ENABLED(FEATURE_CELL_BALANCING)
    BMS_BALANCING,
#endif // FEATURE_CELL_BALANCING
    BMS_ERROR,
} BMS_State_E;

typedef enum
{
    BMS_CELL_DISCONNECTED = 0x00,
    BMS_CELL_CONNECTED,
#if FEATURE_IS_ENABLED(BMS_FAULTS)
    BMS_CELL_FAULT_UV,
    BMS_CELL_FAULT_OV,
#endif // FEATURE_BMSW_FAULTS
    BMS_CELL_ERROR,
} BMS_Cell_E;

typedef struct
{
    float32_t  voltage;
    float32_t  parasitic_corr;
    BMS_Cell_E state;
} BMS_Cell_S;

typedef struct
{
    BMS_State_E state;
    bool        delayed_measurement;
    bool        fault;
    uint16_t    balancing_cells;
    BMS_Cell_S  cells[MAX_CELL_COUNT];      // [V], precision 1V
    float32_t   pack_voltage;               // [V], precision 1V
    float32_t   calculated_pack_voltage;    // [V], precision 1V

    struct
    {
        float32_t max;
        float32_t min;
        float32_t avg;
    } voltage;         // [V], precision 1V
} BMS_S;

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern BMS_S BMS;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void BMS_setOutputCell(MAX_selectedCell_E cell);
void BMS_toSleep(void);
void BMS_wakeUp(void);
MAX_selectedCell_E BMS_getCurrentOutputCell(void);
void BMS_measurementComplete(void);
