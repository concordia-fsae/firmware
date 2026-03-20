/**
 * @file BMS.h
 * @brief  Header file for BMS manager
 */

#pragma once

#include "stdbool.h"
#include "stdint.h"
#include "lib_nvm.h"
#include "lib_simpleFilter.h"

#include "LIB_Types.h"

#define BMS_MAX_SEGMENTS 8U
_Static_assert(BMS_MAX_SEGMENTS >= BMS_CONFIGURED_SERIES_SEGMENTS);

#define BMS_VPACK_SOURCE BMS.pack_voltage_calculated

// TDK HVC43 series, worst case 200A load at break
// https://www.tdk-electronics.tdk.com/inf/100/ds/HVC43MC_B88269X.pdf
# define BMSB_CONTACTOR_LIFETIME_HVC43            1000U

// Comus 3350 series reed relay, no cycle count published in datasheet
// https://www.comus-intl.com/wp-content/uploads/2017/01/High-Voltage-Reed-Relays.pdf
#define BMSB_CONTACTOR_LIFETIME_3350_PRECHARGE   2000U

typedef enum {
    BMS_INIT = 0x00,
    BMS_RUNNING,
    BMS_FAULT,
} BMS_State_E;

typedef enum {
    BMS_CONTACTORS_OPEN = 0x00,
    BMS_CONTACTORS_PRECHARGE,
    BMS_CONTACTORS_CLOSED,
    BMS_CONTACTORS_HVP_CLOSED,
} BMS_Contactors_E;

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
    BMS_Contactors_E contacts;
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
    float32_t cell_amp_hours[BMS_CONFIGURED_SERIES_SEGMENTS * BMS_CONFIGURED_SERIES_CELLS];
    uint8_t spare[16U];
} LIB_NVM_STORAGE(nvm_bmsData_S);

typedef struct {
    struct{
        uint32_t contactorHvp;
        uint32_t contactorHvn;
        uint32_t precharge;
    } contactorLifetime;
    uint8_t reserved[16U];
} LIB_NVM_STORAGE(nvm_bmsbContactorData_S);

extern nvm_bmsData_S current_data;
extern nvm_bmsbContactorData_S contactor_data;

extern BMSB_S BMS;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

bool BMS_SFT_checkMCTimeout(void);
bool BMS_SFT_checkBrusaChargerTimeout(void);
bool BMS_SFT_checkElconChargerTimeout(void);
void BMS_stopCharging(void);
void BMS_continueCharging(void);
float32_t BMSB_getContactorSohHvp(void);
float32_t BMSB_getContactorSohHvn(void);
float32_t BMSB_getContactorSohPrecharge(void);
uint32_t BMSB_getContactorLifetimeHvp(void);
uint32_t BMSB_getContactorLifetimeHvn(void);
uint32_t BMSB_getContactorLifetimePrecharge(void);
