/**
 * @file SOC_Estimation.h
 * @brief  Header file for SOC Estimation
 */

#pragma once

#include "FloatTypes.h"
#include "stdint.h"
#include "BMS.h"

static struct
{
    float32_t voltage;
    float32_t OCV;
    float32_t SOC;
    float32_t Ri;
    float32_t R1;
    float32_t C1;
} Cell_param;

static struct 
{
    float32_t VRC1; // voltage drop over RC network 1
    float32_t dVRC1; //change in voltage drop over RC network 1
    float32_t VR1; //voltage drop over resistor 1
    float32_t current; // current draw as RX from boss note: current is negative for discharge
    float32_t voltage;
    int64_t last_step_us;
} Circuit_param;

void SOCestimation();

void socEstimation_init(void);