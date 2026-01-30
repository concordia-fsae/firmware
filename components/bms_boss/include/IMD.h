/**
 * @file IMD.h
 * @brief  Header file for IMD manager
 */

#pragma once

#include "LIB_Types.h"

typedef enum {
    IMD_INIT = 0x00,
    IMD_HEALTHY,
    IMD_UNHEALTHY,
    IMD_FAULT,
    IMD_ERROR,
} IMD_State_E;

typedef enum {
    IMD_SST_BAD = 0x00,
    IMD_SST_GOOD,
} IMD_SST_E;


void IMD_init(void);
void IMD_setMlsMeasurement(uint32_t freq, uint32_t duty);
void IMD_setSST(bool good);
void IMD_setFault(bool fault);
bool IMD_timeout(void);
IMD_State_E IMD_getState(void);
IMD_SST_E IMD_getSST(void);
float32_t IMD_getIsolation(void);
