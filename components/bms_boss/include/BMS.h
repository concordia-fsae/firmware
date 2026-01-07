/**
 * @file BMS.h
 * @brief  Header file for BMS manager
 */

#pragma once

#include "stdbool.h"
#include "stdint.h"
#include "lib_nvm.h"
#include "lib_simpleFilter.h"

#include "FloatTypes.h"

#define BMS_MAX_SEGMENTS 8U
_Static_assert(BMS_MAX_SEGMENTS >= BMS_CONFIGURED_SERIES_SEGMENTS);

#define BMS_VPACK_SOURCE BMS.pack_voltage_calculated

typedef enum {
    BMS_INIT = 0x00,
    BMS_RUNNING,
    BMS_FAULT,
} BMS_State_E;

typedef struct {
    bool fault :1;
    bool timeout :1;
    uint32_t last_message; // [ms] precision 1ms
    float32_t segment_voltage; // [V] precision 1mv
    float32_t charge_limit; // [A] precision 1A
    float32_t discharge_limit; // [A] precision 1A
    float32_t max_temp; // [deg C] precision 1degC
    struct
    {
        float32_t max; // [V] precision 1mv
        float32_t min; // [V] precision 1mv
    } voltages;
} BMSW_S;

typedef struct {
    bool fault :1;
    bool pack_voltage_sense_fault :1;
    bool charging_paused :1;
    uint8_t connected_segments;
    float32_t pack_charge_limit; // [A] precision 1A
    float32_t pack_discharge_limit; // [A] precision 1A
    float32_t pack_voltage_calculated;
    float32_t pack_voltage_measured;
    float32_t pack_current;
    float32_t packCurrentRaw;
    float32_t packPowerKW;
    float32_t max_temp; // [deg C] precision 1degC
    struct
    {
        bool reset :1;
        uint64_t last_step_us;
        float32_t amp_hr;
    } counted_coulombs;
    struct
    {
        float32_t max; // [V] precision 1mv
        float32_t min; // [V] precision 1mv
    } voltages;

    lib_simpleFilter_lpf_S lpfCurrent;
} BMSB_S;

typedef struct
{
    float32_t pack_amp_hours;
    uint8_t spare[16U];
} LIB_NVM_STORAGE(nvm_bms_data_S);
extern nvm_bms_data_S current_data;

extern BMSB_S BMS;
