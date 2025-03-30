/**
 * @file app_cooling.h
 * @brief Header file for cooling driver
 *
 * Setup
 * 1. Declare a app_cooling_channel_S and configure the parameters. See 'lib_interpolate.h'
 *    for a description of the interpolation mapping
 * 2. Initialize the channel with app_cooling_init
 * 3. If required, add an override value to the channel with app_cooling_setOverride.
 *    If the override percentage is higher than the calculated cooling power required,
 *    the override value will be used
 * 4. Run a cycle on the channel with app_cooling_run, updating the required power
 *    output to the cooler, and setting any states required by each type of cooler
 * 5. Set the enable state of the cooler with app_cooling_setEnabled
 *
 * Usage
 * - Get the current output power to a cooling channel (measured in % where 0.0f is 0%
 *   and 1.0f is 100%
 * - Get the current state of the cooler with app_cooling_getState
 * - Get the cooling rate (flow, volume/time, etc) of the cooler. This will return
 *   a value in the units of the specified channel. This could be RPM, LPM, CFM...
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"
#include "HW_tim.h"
#include "drv_tempSensors.h"
#include "lib_interpolation.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    COOLING_INIT = 0x00,
    COOLING_OFF,
    COOLING_ON,
    COOLING_ERROR,
} app_cooling_state_E;

typedef struct
{
    enum
    {
        INPUT_TYPE_TEMPSENSOR = 0x00U,
    } type;
    union
    {
        drv_tempSensors_channel_E tempSensor;
    } input;
} app_cooling_input_S;

typedef struct
{
    enum
    {
        OUTPUT_TYPE_PWM = 0x00U,
        OUTPUT_TYPE_PWM_EN,
    } type;
    union
    {
        HW_TIM_pwmChannel_S pwm;
        struct
        {
            HW_TIM_pwmChannel_S pwm;
            HW_GPIO_pinmux_E    pin;
            bool                inverted_enable;
        } pwm_en;
    } output;
} app_cooling_output_S;

typedef struct
{
    enum
    {
        FEEDBACK_FUNC,
    } type;
    union
    {
        float32_t (*func)(void);
    } feedback;
    float32_t scale;
} app_cooling_feedback_S;

typedef struct
{
    const app_cooling_input_S    input;
    const app_cooling_output_S   output;
    const app_cooling_feedback_S feedback;
    lib_interpolation_mapping_S  cooling_map;
} app_cooling_channelConfig_S;

typedef struct
{
    app_cooling_channelConfig_S config;
    app_cooling_state_E         state;
    bool                        enabled;
    float32_t                   override_percentage;
    float32_t                   power;
} app_cooling_channel_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void                app_cooling_init(app_cooling_channel_S* channel);
void                app_cooling_runChannel(app_cooling_channel_S* channel, bool enabled);
void                app_cooling_setOverride(app_cooling_channel_S* channel, float32_t percentage);
float32_t           app_cooling_getPower(app_cooling_channel_S* channel);
app_cooling_state_E app_cooling_getState(app_cooling_channel_S* channel);
// Returns the units measured depending
// type of cooling (ie: Fan=RPM...)
float32_t           app_cooling_getRate(app_cooling_channel_S* channel);
