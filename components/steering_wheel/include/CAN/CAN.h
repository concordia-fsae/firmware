/**
 * CAN.h
 * Header file for CAN implementation
 */

#pragma once

#include "FreeRTOS_SWI.h"

extern RTOS_swiHandle_T *CAN_BUS_A_10ms_swi;

extern void CAN_BUS_A_10ms_SWI(void);
