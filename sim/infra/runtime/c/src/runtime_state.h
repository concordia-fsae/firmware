#pragma once

#include "FreeRTOS_SWI.h"
#include "HW_adc.h"
#include "HW_gpio.h"
#include "LIB_Types.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    float32_t bank1[ADC_BANK1_CHANNEL_COUNT];
    float32_t bank2[ADC_BANK2_CHANNEL_COUNT];
    bool      gpio[HW_GPIO_COUNT];
    uint64_t  time_ns;
} rig_runtime_state_S;

extern rig_runtime_state_S rig_runtime;
extern RTOS_swiHandle_T    rig_runtime_swi_handles[RTOS_SWI_PRI_COUNT][RTOS_SWI_MAX_PER_PRI];
extern uint8_t             rig_runtime_swi_count[RTOS_SWI_PRI_COUNT];
