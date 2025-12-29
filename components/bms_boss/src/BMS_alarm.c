/**
 * @file BMS_alarm.c
 * @brief  Source code for pack alarms
 */

#include "BMS_alarm.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "MessageUnpack_generated.h"

/******************************************************************************
*                              D E F I N E S
******************************************************************************/

#define ALARMTEMP 55.0f
#define ALARM_OFF_TEMP  53.0f

typedef struct {
    float32_t W0_temp;
    float32_t W1_temp;
    float32_t W2_temp;
    float32_t W3_temp;
    float32_t W4_temp;
    float32_t W5_temp;
    float32_t max_pack_temp;
} BMSW_Temps_S;

typedef enum {
    INIT = 0x00,
    ALARM,
    NO_ALARM,
} Alarm_State_E;

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

static Alarm_State_E alarm_state;
static BMSW_Temps_S bmsw_temps;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

bool Return_AlarmState(void)
{
    if (alarm_state == ALARM){
        return true;
    }
    else{
        return false;
    }
}

float32_t Return_MaxPackTemp(void)
{
    return bmsw_temps.max_pack_temp;
}

static void Max_Packtemp(void){
    bmsw_temps.max_pack_temp = bmsw_temps.W0_temp;
    if (bmsw_temps.W1_temp > bmsw_temps.max_pack_temp) bmsw_temps.max_pack_temp = bmsw_temps.W1_temp;
    if (bmsw_temps.W2_temp > bmsw_temps.max_pack_temp) bmsw_temps.max_pack_temp = bmsw_temps.W2_temp;
    if (bmsw_temps.W3_temp > bmsw_temps.max_pack_temp) bmsw_temps.max_pack_temp = bmsw_temps.W3_temp;
    if (bmsw_temps.W4_temp > bmsw_temps.max_pack_temp) bmsw_temps.max_pack_temp = bmsw_temps.W4_temp;
    if (bmsw_temps.W5_temp > bmsw_temps.max_pack_temp) bmsw_temps.max_pack_temp = bmsw_temps.W5_temp;
}
static void BMS_AlarmTemp(void)
{
    if (alarm_state == INIT) {
        alarm_state = (bmsw_temps.max_pack_temp >= ALARMTEMP) ? ALARM: NO_ALARM;
        return;
    }
    if (alarm_state != ALARM) {
        if (bmsw_temps.max_pack_temp >= ALARMTEMP) {
            alarm_state = ALARM;
        }
    } else {
        if (bmsw_temps.max_pack_temp < ALARM_OFF_TEMP) {
            alarm_state = NO_ALARM;
        }
    }
}

static void BMS_Alarm_init(void)
{
    alarm_state = INIT;
    bmsw_temps.W0_temp = 0;
    bmsw_temps.W1_temp = 0;
    bmsw_temps.W2_temp = 0;
    bmsw_temps.W3_temp = 0;
    bmsw_temps.W4_temp = 0;
    bmsw_temps.W5_temp = 0;
    bmsw_temps.max_pack_temp = 0;
}

static void BMSIMPORTTEMP_periodic_1Hz(void)
{
    const bool BMSW0_Valid = CANRX_get_signalDuplicate(VEH, BMSW_tempMax, &bmsw_temps.W0_temp, 0) == CANRX_MESSAGE_VALID;
    const bool BMSW1_Valid = CANRX_get_signalDuplicate(VEH, BMSW_tempMax, &bmsw_temps.W1_temp, 1) == CANRX_MESSAGE_VALID;
    const bool BMSW2_Valid = CANRX_get_signalDuplicate(VEH, BMSW_tempMax, &bmsw_temps.W2_temp, 2) == CANRX_MESSAGE_VALID;
    const bool BMSW3_Valid = CANRX_get_signalDuplicate(VEH, BMSW_tempMax, &bmsw_temps.W3_temp, 3) == CANRX_MESSAGE_VALID;
    const bool BMSW4_Valid = CANRX_get_signalDuplicate(VEH, BMSW_tempMax, &bmsw_temps.W4_temp, 4) == CANRX_MESSAGE_VALID;
    const bool BMSW5_Valid = CANRX_get_signalDuplicate(VEH, BMSW_tempMax, &bmsw_temps.W5_temp, 5) == CANRX_MESSAGE_VALID;
    if (BMSW0_Valid && BMSW1_Valid && BMSW2_Valid && BMSW3_Valid && BMSW4_Valid && BMSW5_Valid){
        Max_Packtemp();
        BMS_AlarmTemp();
    }
}

const ModuleDesc_S BMS_Alarm_desc = {
    .moduleInit = &BMS_Alarm_init,
    .periodic1Hz_CLK = &BMSIMPORTTEMP_periodic_1Hz,
};