/**
 * @file SYS_Vehicle.h
 * @brief  Header file for the Vehicle's Safety State Machine
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2023-01-16
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Types.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
  IMD_STATUS = 0x00,
  BMS_STATUS,
  BMS_DISCHARGE_STATUS,
  BMS_CHARGE_STATUS,
  BMS_ERROR_STATUS,
  TSMS_STATUS,
  TS_STATUS,
  STATUS_COUNT,
} STATUS_INDEX_E;

typedef enum
{
  OFF = 0x00,
  ON,
} STATUS_E;

typedef enum
{
  PRECHARGE_CONTACTOR = 0x00,
  MAIN_CONTACTOR,
} CONTACTORS_E;

typedef enum
{
  VEHICLE_INIT = 0x00,
  VEHICLE_READY,
  CONTACTOR_PRECHARGE_CLOSED,
  CONTACTOR_PRECHARGE_MAIN_CLOSED,
  CONTACTOR_MAIN_CLOSED,
  VEHICLE_FAULT,
} VEHICLE_STATES_E;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void SYS_SAFETY_Init(void);
void SYS_CONTACTORS_OpenAll(void);
bool SYS_SAFETY_GetStatus(STATUS_INDEX_E status);
void SYS_SAFETY_SetStatus(STATUS_INDEX_E status, STATUS_E state);
void SYS_CONTACTORS_Switch(CONTACTORS_E contactor, STATUS_E state);
void SYS_SAFETY_SetBatteryVoltage(int16_t v);
void SYS_SAFETY_SetBusVoltage(int16_t v);
void SYS_SAFETY_CycleState(void);
void SYS_SAFETY_SetIsolation(uint8_t duty, uint8_t freq);

