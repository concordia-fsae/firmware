/**
 * @file HW_MAX14921.h
 * @brief  Header file for MAX14921 Cell Measurement/Balancing IC
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "stdbool.h"
#include "stdint.h"

// Firmware Includes
#include "HW_spi.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    MAX_PN_14920 = 0b00,
    MAX_PN_14921 = 0b10,
    MAX_PN_ERROR = 0b11,
} MAX_productID_E;

typedef enum
{
    MAX_PARASITIC_ERROR_CALIBRATION = 0x00,
    MAX_AMPLIFIER_SELF_CALIBRATION,
    MAX_TEMPERATURE_UNBUFFERED,
    MAX_PACK_VOLTAGE,
    MAX_TEMPERATURE_BUFFERED,
    MAX_CELL_VOLTAGE,
} MAX_analogOutput_E;

typedef enum
{
    MAX_CELL1 = 0x00,
    MAX_CELL2,
    MAX_CELL3,
    MAX_CELL4,
    MAX_CELL5,
    MAX_CELL6,
    MAX_CELL7,
    MAX_CELL8,
    MAX_CELL9,
    MAX_CELL10,
    MAX_CELL11,
    MAX_CELL12,
    MAX_CELL13,
    MAX_CELL14,
    MAX_CELL15,
    MAX_CELL16,
    MAX_CELL_COUNT,
} MAX_selectedCell_E;

typedef enum
{
    MAX_T1 = 0x01,
    MAX_T2,
    MAX_T3,
} MAX_selectedTemp_E;

typedef struct
{
    bool     low_power_mode    : 1;
    bool     diagnostic_enabled: 1;
    bool     sampling          : 1;
    uint32_t sampling_start_100us;
    uint16_t balancing;
    struct
    {
        MAX_analogOutput_E state;
        union
        {
            MAX_selectedCell_E cell;
            MAX_selectedTemp_E temp;
        } output;
    } output;
} MAX_config_S;

typedef struct
{
    uint16_t        cell_undervoltage;
    MAX_productID_E ic_id;
    uint8_t         die_version;
    bool            va_undervoltage;
    bool            vp_undervoltage;
    bool            ready;
    bool            thermal_shutdown;
    uint8_t         connected_cells;
} MAX_response_S;

typedef struct
{
    HW_SPI_Device_S* dev;
    MAX_config_S     config;
    MAX_response_S   state;
} MAX_S;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool MAX_init(void);
bool MAX_readWriteToChip(void);
