/**
 * @file cooling.c
 * @brief  Source code for Cooling Application
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module header */
#include "cooling.h"
#include "Module.h"
#include "drv_timer.h"

#include "MessageUnpack_generated.h"

#define STARTUP_TIMER 10000

static drv_timer_S start_up;

static lib_interpolation_point_S fan_curve[] = {
    {
        .x = 35, // degC
        .y = 0.0f, // duty cycle
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
    [COOLING_CHANNEL_FAN1] = {
        .config = {
            .input = {
                .type = INPUT_TYPE_TEMPSENSOR,
                .input.tempSensor = DRV_TEMPSENSORS_CHANNEL_SEGMENT_MAX,
            },
            .output = {
                .type = OUTPUT_TYPE_PWM,
                .output.pwm = {
                    .tim_port = HW_TIM_PORT_PWM,
                    .tim_channel = HW_TIM_CHANNEL_1,
                },
            },
            .feedback = { // Feedback in units of RPM
                .type = FEEDBACK_FUNC,
                .feedback.func = &HW_TIM1_getFreqCH1,
                .scale = 60,
            },
            .cooling_map = {
                .points = (lib_interpolation_point_S*)&fan_curve,
                .number_points = COUNTOF(fan_curve),
                .saturate_left = true,
                .saturate_right = true,
            }
        },
    },
    [COOLING_CHANNEL_FAN2] = {
        .config = {
            .input = {
                .type = INPUT_TYPE_TEMPSENSOR,
                .input.tempSensor = DRV_TEMPSENSORS_CHANNEL_SEGMENT_MAX,
            },
            .output = {
                .type = OUTPUT_TYPE_PWM,
                .output.pwm = {
                    .tim_port = HW_TIM_PORT_PWM,
                    .tim_channel = HW_TIM_CHANNEL_2,
                },
            },
            .feedback = { // Feedback in units of RPM
                .type = FEEDBACK_FUNC,
                .feedback.func = &HW_TIM1_getFreqCH2,
                .scale = 60,
            },
            .cooling_map = {
                .points = (lib_interpolation_point_S*)&fan_curve,
                .number_points = COUNTOF(fan_curve),
                .saturate_left = true,
                .saturate_right = true,
            }
        },
    },
};

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Cooling Module Init function
 */
static void cooling_init()
{
    for (uint8_t i = 0; i < COOLING_CHANNEL_COUNT; i++)
    {
        app_cooling_init(&cooling[i]);
    }

    drv_timer_init(&start_up);
    drv_timer_start(&start_up, STARTUP_TIMER);
}

/**
 * @brief  Cooling Module 1Hz periodic function
 */
static void cooling10Hz_PRD(void)
{
    drv_timer_state_E boot_timer_state = drv_timer_getState(&start_up);
    uint8_t percent_beans = 0;
    float32_t override = 0.0f;

    if (boot_timer_state == DRV_TIMER_RUNNING)
    {
        // Ramp the fans up and back down on boot
        const uint32_t time = HW_TIM_getTimeMS();
        override = (time < (STARTUP_TIMER / 2.0f)) ? ((float32_t)time) / (STARTUP_TIMER / 2.0f) :
                                                     (STARTUP_TIMER - (float32_t)time) / (STARTUP_TIMER / 2.0f);
    }
    else if (CANRX_get_signal(VEH, TOOLING_commandedFansDutyCycle, &percent_beans) == CANRX_MESSAGE_VALID)
    {
        // Store canbus fan duty cycle override if present
        override = ((float32_t)percent_beans) / 100.0f;
    }

    for (uint8_t i = 0; i < COOLING_CHANNEL_COUNT; i++)
    {
        // Update the states of the cooler, disabling if not required
        app_cooling_setOverride(&cooling[i], override);
        app_cooling_runChannel(&cooling[i], (boot_timer_state == DRV_TIMER_EXPIRED) &&
                                             app_cooling_getPower(&cooling[i]) > 0.01f);
    }
}

/**
 * @brief  Cooling Module descriptor
 */
const ModuleDesc_S cooling_desc = {
    .moduleInit       = &cooling_init,
    .periodic10Hz_CLK = &cooling10Hz_PRD,
};
