/**
 * @file cooling.c
 * @brief  Source code for Cooling Application
 */

/******************************************************************************
 *                             I N C L U D E S make sure driver for pump min is non 0
 ******************************************************************************/

/**< Module header */
#include "cooling.h"
#include "Module.h"
#include "drv_timer.h"
#include "MessageUnpack_generated.h"

static drv_timer_S start_up;

static lib_interpolation_point_S rad_fan_curve[] = {
    {
        .x = 25, // degC
        .y = 0.0f, // duty cycle
    },
    {
        .x = 50, // degC
        .y = 1.0f, // duty cycle
    },
};
min_pump = 0.4f;// minimum pump speed
static lib_interpolation_point_S pump_curve[] = {
    {
        .x = 25, // degC
        .y = min_pump, // duty cycle
    },
    {
        .x = 50, // degC
        .y = 1.0f, // duty cycle
    },
};



/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

 app_cooling_channel_S cooling[COOLING_CHANNEL_COUNT] = {
    [COOLING_CHANNEL_RAD_FAN,] = {
        .config = {
            .input = {
                .type = INPUT_TYPE_TEMPSENSOR,
                .input.tempSensor = ,//what to put here
            },
            .output = {
                .type = OUTPUT_TYPE_PWM,
                .output.pwm = {
                    .tim_port = HW_TIM_PORT_PWM, //Change these??
                    .tim_channel =, //Change these??
                },
            },
            .feedback = { // There is no feedback
                .type = nullptr,
                .feedback.func = nullptr,
                .scale = 0,
            },
            .cooling_map = {
                .points = (lib_interpolation_point_S*)&rad_fan_curve,
                .number_points = COUNTOF(rad_fan_curve),
                .saturate_left = true,
                .saturate_right = true,
            }
        },
    },
    [COOLING_CHANNEL_PUMP] = {
        .config = {
            .input = {
                .type = INPUT_TYPE_TEMPSENSOR,
                .input.tempSensor = ,
            },
            .output = {
                .type = OUTPUT_TYPE_PWM,
                .output.pwm = {
                    .tim_port = HW_TIM_PORT_PWM,//what to set these to
                    .tim_channel = HW_TIM_CHANNEL_2,//what to set these to
                },
            },
            .feedback = { // There is no feedback
                .type = nullptr,
                .feedback.func = nullptr
                .scale = nullptr,
            },
            .cooling_map = {
                .points = (lib_interpolation_point_S*)&pump_curve,
                .number_points = COUNTOF(pump_curve),
                .saturate_left = true,
                .saturate_right = true,
            }
        },
    },

 },

 /******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Cooling Module Init function
 */
static void cooling_init()
{
    for (uint8_t i = 0; i < COOLING_CHANNEL_COUNT_RAD; i++)
    {
        app_cooling_init(&cooling[i]);
    }

    drv_timer_init(&start_up);
    drv_timer_start(&start_up, STARTUP_TIMER);
}
/**
 * 
 */
static void cooling10Hz_PRD(void)
{
    //CODE FOR FANS

    drv_timer_state_E boot_timer_state = drv_timer_getState(&start_up);
    uint8_t percent_override_fan = 0;
    float32_t override_fan = 0.0f;
    if (boot_timer_state == DRV_TIMER_RUNNING)
    {
        const uint32_t time = HW_TIM_getTimeMS();
        if (time<STARTUP_TIMER/4.0F):
            override_fan = 0.5f
    }
    //What is CAN message I need
    else if ((CANRX_get_signal(VEH, TOOLING_, &percent_override_fan) == CANRX_MESSAGE_VALID)){
        override_fan = ((float32_t)percent_override_fan) / 100.0f;

    }
    app_cooling_setOverride(&cooling_rad[0], override_fan);
    app_cooling_runChannel(&cooling_rad[0], (boot_timer_state == DRV_TIMER_EXPIRED) );

    //CODE FOR PUMP
    uint8_t percent_override_pump =0 ;
    float32_t override_pump = min_pump;
    //What is CAN message I need
    else if ((CANRX_get_signal(VEH, TOOLING_, &percent_override_fan) == CANRX_MESSAGE_VALID)){
        override_pump = ((float32_t)percent_override_pump) / 100.0f;
    }
    app_cooling_setOverride(&cooling_rad[1], override_pump);
    app_cooling_runChannel(&cooling_rad[1], (boot_timer_state == DRV_TIMER_EXPIRED) );

}
