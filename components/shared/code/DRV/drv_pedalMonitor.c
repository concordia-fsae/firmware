/**
 * @file drv_pedalMonitor.c
 * @brief Source file for pedal monitor
 * @note Pedal positon is a float percentage between 0.0f and 1.0f where 
 *       0.0f is 0% and 1.0f is 100%
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_pedalMonitor.h"
#include "drv_inputAD.h"
#include "lib_utility.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern drv_pedalMonitor_channelConfig_S drv_pedalMonitor_channels[DRV_PEDALMONITOR_CHANNEL_COUNT];

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t                voltage;
    float32_t                percentage;
    drv_pedalMonitor_state_E state;
} drv_pedalMonitor_channelData_S;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static drv_pedalMonitor_channelData_S pedals[DRV_PEDALMONITOR_CHANNEL_COUNT];

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Initialize all pedal monitor channels
 * @note All channels initialize to INIT and 0% pedal position
 */
void drv_pedalMonitor_init(void)
{
    for (uint8_t i = 0U; i < DRV_PEDALMONITOR_CHANNEL_COUNT; i++)
    {
        switch (drv_pedalMonitor_channels[i].type)
        {
            case DRV_PEDALMONITOR_TYPE_ANALOG:
                lib_interpolation_init(&drv_pedalMonitor_channels[i].input.analog.pedal_map,
                                       0.0f);
                break;
            default:
                break;
        }

        pedals[i].percentage = 0.0f;
        pedals[i].state = DRV_PEDALMONITOR_INIT;
    }
}

/**
 * @brief Run the pedal monitor on all pedal monitor channels
 */
void drv_pedalMonitor_run(void)
{
    for (uint8_t i = 0U; i < DRV_PEDALMONITOR_CHANNEL_COUNT; i++)
    {
        drv_pedalMonitor_state_E state = DRV_PEDALMONITOR_FAULT;
        float32_t percentage = 0.0f;

        switch (drv_pedalMonitor_channels[i].type)
        {
            case DRV_PEDALMONITOR_TYPE_ANALOG:
                {
                    pedals[i].voltage = drv_inputAD_getAnalogVoltage(drv_pedalMonitor_channels[i].input.analog.channel);
                    if ((pedals[i].voltage <= drv_pedalMonitor_channels[i].input.analog.fault_high) &&
                        (pedals[i].voltage >= drv_pedalMonitor_channels[i].input.analog.fault_low))
                    {
                        state = DRV_PEDALMONITOR_OK;
                        percentage = lib_interpolation_interpolate(&drv_pedalMonitor_channels[i].input.analog.pedal_map,
                                                                   pedals[i].voltage);
                    }
                    else if (pedals[i].voltage > drv_pedalMonitor_channels[i].input.analog.fault_high)
                    {
                        state = DRV_PEDALMONITOR_FAULT_DISCONNECTED;
                    }
                    else if (pedals[i].voltage < drv_pedalMonitor_channels[i].input.analog.fault_low)
                    {
                        state = DRV_PEDALMONITOR_FAULT_SHORTED;
                    }
                }
                break;
            case DRV_PEDALMONITOR_TYPE_CAN:
                {
                    float32_t percentage_temp = 0.0f; // CAN percentages are 0-100
                    if (drv_pedalMonitor_channels[i].input.canrx_getPedalPosition(&percentage_temp) == CANRX_MESSAGE_VALID)
                    {
                        state = DRV_PEDALMONITOR_OK;
                        percentage = percentage_temp / 100.0f;
                    }
                }
                break;
        }

        pedals[i].state = state;
        pedals[i].percentage = SATURATE(0.0f, percentage, 1.0f);
    }
}

/**
 * @brief Get the pedal position of a channel
 * @param channel The channel to retrieve
 * @return The pedal position mapped from 0.0f to 1.0f where 0.0f is 0% and 1.0f is 100%
 */
float32_t drv_pedalMonitor_getPedalPosition(drv_pedalMonitor_channel_E channel)
{
    return (pedals[channel].state == DRV_PEDALMONITOR_OK) ? pedals[channel].percentage : 0.0f;
}

/**
 * @brief Get the pedal state of a channel
 * @param channel The channel to retrieve
 * @return The current state of the pedal channel 
 */
drv_pedalMonitor_state_E drv_pedalMonitor_getPedalState(drv_pedalMonitor_channel_E channel)
{
    return pedals[channel].state;
}

/**
 * @brief Get the pedal CAN state of a channel
 * @param channel The channel to retrieve
 * @return The current state of the pedal channel 
 */
CAN_pedalState_E drv_pedalMonitor_getPedalStateCAN(drv_pedalMonitor_channel_E channel)
{
    CAN_pedalState_E ret = CAN_PEDALSTATE_SNA;

    switch (pedals[channel].state)
    {
        case DRV_PEDALMONITOR_OK:
            ret = CAN_PEDALSTATE_OK;
            break;
        case DRV_PEDALMONITOR_FAULT_SHORTED:
            ret = CAN_PEDALSTATE_SHORTED;
            break;
        case DRV_PEDALMONITOR_FAULT_DISCONNECTED:
            ret = CAN_PEDALSTATE_DISCONNECTED;
            break;
        default:
            break;
    }

    return ret;
}

/**
 * @brief Get the pedal sensor voltage
 * @param channel The channel to retrieve
 * @return The voltage of the sensor
 */
float32_t drv_pedalMonitor_getPedalVoltage(drv_pedalMonitor_channel_E channel)
{
    return pedals[channel].voltage;
}
