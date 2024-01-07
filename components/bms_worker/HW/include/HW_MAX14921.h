/**
 * @file HW_MAX14921.h
 * @brief  Header file for MAX14921 Cell Measurement/Balancing IC
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-23
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

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
    PN_14920 = 0b00,
    PN_14921 = 0b10,
    PN_ERROR = 0b11,
} MAX_ProductID_E;

typedef enum {
    PARASITIC_ERROR_CALIBRATION = 0x00,
    AMPLIFIER_SELF_CALIBRATION,
    TEMPERATURE_UNBUFFERED,
    PACK_VOLTAGE,
    TEMPERATURE_BUFFERED,
    CELL_VOLTAGE,
} MAX_AnalogOutput_E;

typedef enum {
    CELL1 = 0x00,
    CELL2,
    CELL3,
    CELL4,
    CELL5,
    CELL6,
    CELL7,
    CELL8,
    CELL9,
    CELL10,
    CELL11,
    CELL12,
    CELL13,
    CELL14,
    CELL15,
    CELL16,
    CELL_COUNT,
} MAX_SelectedCell_E;

typedef enum {
    T1 = 0x01,
    T2,
    T3,
} MAX_SelectedTemp_E;

typedef union {
    MAX_SelectedCell_E cell;
    MAX_SelectedTemp_E temp;
} MAX_SelectedOutput_U;

typedef struct {
    MAX_AnalogOutput_E state;
    MAX_SelectedOutput_U output;
} MAX_Output_S;

typedef struct {
    bool low_power_mode;
    bool diagnostic_enabled;
    bool sampling;
    uint32_t sampling_start_100us;
    uint16_t balancing;
    MAX_Output_S output;
} MAX14921_Config_S;

typedef struct {
    uint16_t cell_undervoltage;
    MAX_ProductID_E ic_id;
    uint8_t die_version;
    bool va_undervoltage;
    bool vp_undervoltage;
    bool ready;
    bool thermal_shutdown;
    uint8_t connected_cells;
} MAX14921_Response_S;

typedef struct {
    HW_SPI_Device_S *dev;
    MAX14921_Config_S config;
    MAX14921_Response_S state;
} MAX14921_S;


/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool MAX_Init(void);
bool MAX_ReadWriteToChip(void);
