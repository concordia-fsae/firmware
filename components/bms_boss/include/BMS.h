/**
 * @file BMS.h
 * @brief  Header file for BMS manager
 */

#include "stdbool.h"
#include "stdint.h"

#include "FloatTypes.h"

#include "PACK.h"

#define BMS_MAX_SEGMENTS 8U

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
    uint8_t connected_segments;
    float32_t pack_charge_limit; // [A] precision 1A
    float32_t pack_discharge_limit; // [A] precision 1A
    BMSW_S workers[BMS_MAX_SEGMENTS];
    float32_t pack_voltage;
    float32_t max_temp; // [deg C] precision 1degC
    struct
    {
        float32_t max; // [V] precision 1mv
        float32_t min; // [V] precision 1mv
    } voltages;
} BMSB_S;

extern BMSB_S BMS;

void BMS_setSegmentStats(uint8_t seg_id, const BMSW_S* seg);
