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
    uint16_t   relative_SoC;
    BMS_Cell_E state;
} BMS_Cell_S;

typedef struct
{
    BMS_State_E state;
    uint16_t    balancing_cells;
    BMS_Cell_S  cells[MAX_CELL_COUNT];      // [mv], precision 1mv
    uint16_t    pack_voltage;               // [mv], precision 1mv
    uint16_t    calculated_pack_voltage;    // [mv], precision 1mv

    uint8_t     charge_limit;               // [A], precision .1A
    uint8_t     discharge_limit;            // [A], precision .1A

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
    struct
    {
        uint8_t min;
        uint8_t max;
        uint8_t avg;
    } relative_SoC; // [1%], precision 1%
    uint8_t connected_cells;
} BMS_S;


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern BMS_S BMS;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void BMS_setOutputCell(MAX_selectedCell_E cell);
void BMS_measurementComplete(void);

float BMS_minf(float SoCBasedLimit, float heatBasedLimit);

uint8_t BMS_chargeLimit_SoC(uint8_t relativeSoC);
uint8_t BMS_dischargeLimit_SoC(uint8_t relativeSoC);

uint8_t BMS_chargeLimit_heat(int16_t cell_temp);
uint8_t BMS_dischargeLimit_heat(int16_t cell_temp);