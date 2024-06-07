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
 *                              D E F I N E S
 ******************************************************************************/

#ifndef BMS_CONFIGURED_SERIES_CELLS
# define BMS_CONFIGURED_SERIES_CELLS 14
#endif

#ifndef BMS_CONFIGURED_PARALLEL_CELLS
# define BMS_CONFIGURED_PARALLEL_CELLS 5
#endif


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
    BMS_SLEEPING,
    BMS_ERROR,
} BMS_State_E;

typedef enum
{
    BMS_CELL_DISCONNECTED = 0x00,
    BMS_CELL_CONNECTED,
    BMS_CELL_FAULT_UV,
    BMS_CELL_FAULT_OV,
    BMS_CELL_ERROR,
} BMS_Cell_E;

typedef struct
{
    float32_t  voltage;
    float32_t  parasitic_corr;
    float32_t  relative_soc;
    BMS_Cell_E state;
} BMS_Cell_S;

typedef struct
{
    BMS_State_E state;
    bool        fault;
    uint16_t    balancing_cells;
    BMS_Cell_S  cells[MAX_CELL_COUNT];      // [V], precision 1V
    float32_t   pack_voltage;               // [V], precision 1V
    float32_t   calculated_pack_voltage;    // [V], precision 1V
    uint8_t     connected_cells;

    float32_t charge_limit;
    float32_t discharge_limit;

    struct
    {
        bool requested :1;
        uint32_t last_request;
        float32_t target_v;
    }balancing;

    struct
    {
        float32_t max;
        float32_t min;
        float32_t avg;
    } voltage;         // [V], precision 1V
    struct
    {
        float32_t min;
        float32_t max;
        float32_t avg;
    } relative_soc;    // [100-0], precision 0.1%
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
void BMS_measurementComplete(void);
void BMS_setBalancing(float32_t target_v);
