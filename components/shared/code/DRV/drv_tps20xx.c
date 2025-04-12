/**
 * @file drv_tps20xx.c
 * @brief  Source file for the TI TPS20xx* driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_tps20xx.h"
#include "drv_io.h"
#include "drv_outputAD.h"
#include "string.h"
#include "drv_timer.h"

#define DEFAULT_FAULT_RETRY_TIME 1000U // Default to waiting 1 second between retries if not specified

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    drv_tps20xx_state_E state;
    bool                enable_request;
    drv_timer_S         retry_timer;
} drv_tps20xx_data_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static drv_tps20xx_data_S drv_tps20xx_data[DRV_TPS20XX_CHANNEL_COUNT];

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

static void drv_tps20xx_private_setICEnabled(drv_tps20xx_channel_E channel, bool enabled);
static bool drv_tps20xx_private_getICFaulted(drv_tps20xx_channel_E channel);

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Initialize all of the TPS20XX channels
 * @note All channels are initialized off.
 */
HW_StatusTypeDef_E drv_tps20xx_init(void)
{
    memset(&drv_tps20xx_data, 0x00U, sizeof(drv_tps20xx_data));

    for (uint8_t channel = 0U; channel < DRV_TPS20XX_CHANNEL_COUNT; channel++)
    {
        drv_tps20xx_private_setICEnabled(channel, false);
        drv_tps20xx_data[channel].state = DRV_TPS20XX_STATE_INIT;
        drv_tps20xx_data[channel].enable_request = false;
        drv_timer_init(&drv_tps20xx_data[channel].retry_timer);
    }

    for (uint8_t channel = 0U; channel < DRV_TPS20XX_CHANNEL_COUNT; channel++)
    {
        drv_tps20xx_data[channel].state = (drv_tps20xx_private_getICFaulted(channel)) ?
                                            DRV_TPS20XX_STATE_ERROR:
                                            DRV_TPS20XX_STATE_OFF;
    }

    return HW_OK;
}

/**
 * @brief De-initialize all TPS20XX channels
 * @note This will disable and shut off the channels.
 */
HW_StatusTypeDef_E drv_tps20xx_deInit(void)
{
    for (uint8_t channel = 0U; channel < DRV_TPS20XX_CHANNEL_COUNT; channel++)
    {
        drv_tps20xx_setEnabled(channel, false);
    }

    return HW_OK;
}

/**
 * @brief Run the periodic TPS20XX driver. This function will iterate across all
 * channels and will update their state accordingly.
 */
void drv_tps20xx_run(void)
{
    for (uint8_t channel = 0U; channel < DRV_TPS20XX_CHANNEL_COUNT; channel++)
    {
        const bool timer_expired = drv_timer_getState(&drv_tps20xx_data[channel].retry_timer) == DRV_TIMER_EXPIRED;
        const bool ic_faulted = drv_tps20xx_private_getICFaulted(channel);

        switch (drv_tps20xx_data[channel].state)
        {
            case DRV_TPS20XX_STATE_OFF:
                if (drv_tps20xx_data[channel].enable_request)
                {
                    drv_tps20xx_data[channel].state = DRV_TPS20XX_STATE_ENABLED;
                    drv_tps20xx_private_setICEnabled(channel, true);
                }
                break;
            case DRV_TPS20XX_STATE_ENABLED:
                if (drv_tps20xx_private_getICFaulted(channel))
                {
                    drv_tps20xx_data[channel].state = DRV_TPS20XX_STATE_FAULTED_OC;
                    drv_tps20xx_private_setICEnabled(channel, false);

                    uint32_t retry_time = (drv_tps20xx_channels[channel].retry_wait_ms != 0U) ?
                                           drv_tps20xx_channels[channel].retry_wait_ms :
                                           DEFAULT_FAULT_RETRY_TIME; // If nothing configured, use default

                    drv_timer_start(&drv_tps20xx_data[channel].retry_timer,retry_time);
                }
                break;
            case DRV_TPS20XX_STATE_FAULTED_OC:
            case DRV_TPS20XX_STATE_FAULTED_OT:
                {
                    drv_tps20xx_private_setICEnabled(channel, false);
                    if ((drv_tps20xx_data[channel].state != DRV_TPS20XX_STATE_FAULTED_OT) && ic_faulted)
                    {
                            drv_tps20xx_data[channel].state = DRV_TPS20XX_STATE_FAULTED_OT;
                    }
                    else if (timer_expired && (ic_faulted == false))
                    {
                        drv_timer_stop(&drv_tps20xx_data[channel].retry_timer);

                        if (drv_tps20xx_channels[channel].auto_reset)
                        {
                            // Only other way to recover from faulted is the application
                            // idsabling and re-enabling the channel
                            drv_tps20xx_private_setICEnabled(channel, true);
                            drv_tps20xx_data[channel].state = DRV_TPS20XX_STATE_RETRY;
                        }
                    }

                    if ((ic_faulted == false) && (drv_tps20xx_data[channel].enable_request == false))
                    {
                        drv_tps20xx_data[channel].state = DRV_TPS20XX_STATE_OFF;
                    }
                }
                break;
            case DRV_TPS20XX_STATE_RETRY:
                if (drv_tps20xx_private_getICFaulted(channel))
                {
                    drv_tps20xx_data[channel].state = DRV_TPS20XX_STATE_FAULTED_OC;
                    drv_tps20xx_private_setICEnabled(channel, false);
                    uint32_t retry_time = (drv_tps20xx_channels[channel].retry_wait_ms != 0U) ?
                                           drv_tps20xx_channels[channel].retry_wait_ms :
                                           DEFAULT_FAULT_RETRY_TIME; // If nothing configured, use default

                    drv_timer_start(&drv_tps20xx_data[channel].retry_timer,retry_time);
                }
                else
                {
                    drv_tps20xx_data[channel].state = DRV_TPS20XX_STATE_ENABLED;
                }
                break;

            case DRV_TPS20XX_STATE_INIT:
            case DRV_TPS20XX_STATE_ERROR:
            default:
                {
                    drv_tps20xx_private_setICEnabled(channel, false);

                    if (drv_timer_getState(&drv_tps20xx_data[channel].retry_timer) == DRV_TIMER_STOPPED)
                    {
                        drv_timer_start(&drv_tps20xx_data[channel].retry_timer, DEFAULT_FAULT_RETRY_TIME);
                    }
                    else if (timer_expired && (ic_faulted == false))
                    {
                        drv_tps20xx_data[channel].state = DRV_TPS20XX_STATE_OFF;
                        drv_timer_stop(&drv_tps20xx_data[channel].retry_timer);
                    }
                    else if (timer_expired && ic_faulted)
                    {
                        drv_tps20xx_data[channel].state = DRV_TPS20XX_STATE_ERROR;
                    }
                }
                break;
        }
    }
}

/**
 * @brief Get the state of the given TPS20XX channel.
 * @param channel The channel to retrieve the state of.
 * @return Return the drv_tps20xx_state_E of the channel.
 */
drv_tps20xx_state_E drv_tps20xx_getState(drv_tps20xx_channel_E channel)
{
    drv_tps20xx_state_E ret = drv_tps20xx_data[channel].state;
    if (drv_tps20xx_data[channel].state == DRV_TPS20XX_STATE_ENABLED)
    {
        if (drv_tps20xx_private_getICFaulted(channel))
        {
            ret = DRV_TPS20XX_STATE_FAULTED_OC;
        }
    }

    return ret;
}

/**
 * @brief Set the state of a channel. 
 * @note If a channel is off and it is set to on, the driver will enable
 * it the next time the periodic is called.
 * @param channel The channel to enable
 * @param enabled True for on, false for off
 */
void drv_tps20xx_setEnabled(drv_tps20xx_channel_E channel, bool enabled)
{
    drv_tps20xx_data[channel].enable_request = enabled;

    if (enabled == false)
    {
        drv_tps20xx_private_setICEnabled(channel, false);

        if (drv_tps20xx_private_getICFaulted(channel) == false)
        {
            drv_tps20xx_data[channel].state = DRV_TPS20XX_STATE_OFF;
        }
    }
}

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Set the phsysical state of the channel enable pin.
 * @note This function will handle the difference in push pull or open drain outputs.
 * @param channel The channel to enable
 * @param enabled True for on, false for off
 */
static void drv_tps20xx_private_setICEnabled(drv_tps20xx_channel_E channel, bool enabled)
{
    const drv_io_activeState_E state_to_set = (enabled) ? DRV_IO_ACTIVE : DRV_IO_INACTIVE;
    drv_outputAD_setDigitalActiveState(drv_tps20xx_channels[channel].enable, state_to_set);
}

/**
 * @brief Get the current channel fault state.
 * @param channel The channel to retrieve the current fault state of.
 * @return True if the channel is presently faulted, false otherwise.
 */
static bool drv_tps20xx_private_getICFaulted(drv_tps20xx_channel_E channel)
{
    return drv_inputAD_getDigitalActiveState(drv_tps20xx_channels[channel].fault) == DRV_IO_ACTIVE;
}
