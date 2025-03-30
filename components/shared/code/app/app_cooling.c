/**
 * @file app_cooling.c
 * @brief Source code for the cooling driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "app_cooling.h"
#include "drv_tempSensors.h"
#include "lib_utility.h"

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Return the current temperature of a channel to cool
 * @param channel The channel to retrieve
 * @return The current temperature of the channel
 */
static float32_t getTemperature(app_cooling_channel_S* channel)
{
    float32_t ret = 0;

    switch (channel->config.input.type)
    {
        case INPUT_TYPE_TEMPSENSOR:
            ret = drv_tempSensors_getChannelTemperatureDegC(channel->config.input.input.tempSensor);
            break;
    }

    return ret;
}

/**
 * @brief Output the channels power
 * @param channel Channel to output
 * @note The output power is saturated between 0 and 100 percent (0.0f-1.0f)
 */
static void outputPower(app_cooling_channel_S* channel)
{
    switch (channel->config.output.type)
    {
        case OUTPUT_TYPE_PWM_EN:
            {
                HW_GPIO_writePin(channel->config.output.output.pwm_en.pin,
                                (channel->config.output.output.pwm_en.inverted_enable) ?
                                channel->enabled == false :
                                channel->enabled);
                const float32_t power = (channel->state == COOLING_ON) ? SATURATE(0.0f, channel->power, 1.0f) : 0.0f;
                HW_TIM_setDuty(channel->config.output.output.pwm_en.pwm.tim_port,
                               channel->config.output.output.pwm_en.pwm.tim_channel,
                               power);
            }
            break;
        case OUTPUT_TYPE_PWM:
            {
                const float32_t power = (channel->state == COOLING_ON) ? SATURATE(0.0f, channel->power, 1.0f) : 0.0f;
                HW_TIM_setDuty(channel->config.output.output.pwm.tim_port,
                               channel->config.output.output.pwm.tim_channel,
                               power);
            }
            break;
    }
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Initialize a cooling channel
 * @param channel The channel to initialize
 */
void app_cooling_init(app_cooling_channel_S* channel)
{
    channel->state = COOLING_OFF;
    channel->enabled = false;
    channel->override_percentage = 0;
    channel->power = 0;
}

/**
 * @brief Run a cycle on a cooler. This will update the temperature, interpolate a new power output,
 *        and output the power to the cooler
 * @param channel The channel to update
 * @param enabled True if the channel is enabled, false otherwise
 */
void app_cooling_runChannel(app_cooling_channel_S* channel, bool enabled)
{
    const float32_t temperature = getTemperature(channel);
    channel->enabled = enabled;
    channel->power = lib_interpolation_interpolate(&channel->config.cooling_map, temperature);
    switch (channel->state)
    {
        case COOLING_INIT:
            channel->state = COOLING_OFF;
            break;
        case COOLING_OFF:
            if (channel->enabled || (channel->override_percentage > 0))
            {
                channel->state = COOLING_ON;
            }
            else
            {
                break;
            }
        case COOLING_ON:
            if ((channel->enabled == false) && (channel->override_percentage < 0.01f))
            {
                channel->state = COOLING_OFF;
                channel->power = 0;
            }
            else if (channel->override_percentage > channel->power)
            {
                channel->power = channel->override_percentage;
            }
            outputPower(channel);
            break;
        case COOLING_ERROR:
        default:
            channel->power = 1.0f;
            outputPower(channel);
            break;
    }
}

/**
 * @brief Set the override percentage of a cooling channel
 * @param channel The channel to set the override value of
 * @param percentage The percent to override. This should be between 0.0f and 1.0f
 */
void app_cooling_setOverride(app_cooling_channel_S* channel, float32_t percentage)
{
    channel->override_percentage = percentage;
}

/**
 * @brief Get the current power output of a cooling channel
 * @param channel The channel to retrieve
 * @return The percentage power output between 0.0f and 1.0f
 */
float32_t app_cooling_getPower(app_cooling_channel_S* channel)
{
    return channel->power;
}

/**
 * @brief Get the current state of a cooling channel
 * @param channel The channel to retrieve
 * @return The current state of the cooling channel
 */
app_cooling_state_E app_cooling_getState(app_cooling_channel_S* channel)
{
    return channel->state;
}

/**
 * @brief Get the rate of cooling of a given channel
 * @note This function returns a value relative to the units the channel is specified in.
 *       A pump with feedback (ie: some tachometer) will provide a unit of Hz which will convert
 *       to some rate of flow. A fan with a tachometer will give some frequency which will convert to
 *       some RPM.
 * @param channel The channel to retrieve
 * @return The rate of flow in the units specified by the channel configuration
 */
float32_t app_cooling_getRate(app_cooling_channel_S* channel)
{
    float32_t ret = 0;

    switch (channel->config.feedback.type)
    {
        case FEEDBACK_FUNC:
            ret = channel->config.feedback.feedback.func() * channel->config.feedback.scale;
            break;
    }

    return ret;
}
