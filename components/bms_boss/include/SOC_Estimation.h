/**
 * @file SOC_Estimation.h
 * @brief  Header file for SOC Estimation
 */

#pragma once

#include "LIB_Types.h"

typedef enum
{
    INIT = 0x00,
    VRC_ESTIMATE,
    RUNNING,
} batteryModelState_E;

typedef struct
{
    batteryModelState_E state;
    float32_t VRC2_estimate;
    float32_t max_current;
    float64_t last_step;
} batteryModel_S;

static struct
{
    float32_t OCV;
    float32_t SOC;
    float32_t Ri;
    float32_t R1;
    float32_t R2;
    float32_t C1;
    float32_t C2;
} Cell_param;

static struct 
{
    float32_t VRC1;
    float32_t VRC2;
    float32_t current; // current draw as RX from boss note: current is negative for discharge
    float32_t voltage;
} Circuit_param;

void SOC_Estimation(float32_t pack_voltage, float32_t pack_current, float64_t time_now);

void SOC_Estimation_init(void);