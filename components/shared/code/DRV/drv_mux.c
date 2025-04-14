/**
 * @file drv_mux.c
 * @brief Source file for a generic multiplexer driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_mux.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Initialize a multiplexer
 * @param mux The multiplexer to initialize
 */
void drv_mux_init(drv_mux_channel_S * mux)
{
    if (mux == NULL) return;

    // --- Compute and store bit width based on max output channels ---
    uint8_t width = 0;
    uint8_t max_channel = mux->config.max_output_channel;
    while ((1U << width) < max_channel)
    {
        width++;
    }
    mux->data.bit_width = width;

    // --- Set all mux selector pins to inactive ---
    for (uint8_t bit = 0; bit < width; ++bit)
    {
        drv_outputAD_channelDigital_E pin = 0U;

        switch (mux->type)
        {
            case DRV_MUX_TYPE_GPIO:
                pin = (drv_outputAD_channelDigital_E)(mux->config.outputs.gpio.pin_first + bit);
                break;
            case DRV_MUX_TYPE_GPIO_EN:
                pin = (drv_outputAD_channelDigital_E)(mux->config.outputs.gpio_en.pin_first + bit);
                break;
        }

        drv_outputAD_setDigitalActiveState(pin, DRV_IO_INACTIVE);
    }

    // --- If applicable, set enable pin to inactive ---
    if (mux->type == DRV_MUX_TYPE_GPIO_EN)
    {
        drv_outputAD_channelDigital_E en_pin = mux->config.outputs.gpio_en.enable;
        drv_outputAD_setDigitalActiveState(en_pin, DRV_IO_INACTIVE);
    }

    // --- Initialize internal mux state ---
    mux->data.channel_output = 0;
    mux->data.is_enabled = false;
}

/**
 * @brief Set the output of a multiplexer
 * @param mux The multiplexer to initialize
 * @param output the output channel
 * @note If the output is higher than the configured max or the max of the select
 *       line bus width, the output will roll over
 */
void drv_mux_setMuxOutput(drv_mux_channel_S * mux, uint8_t output)
{
    if (mux == NULL) return;

    uint8_t max_output = mux->config.max_output_channel;
    uint8_t output_channel = output % max_output;
    mux->data.channel_output = output_channel;

    uint8_t bit_width = mux->data.bit_width;

    for (uint8_t bit = 0; bit < bit_width; ++bit)
    {
        drv_outputAD_channelDigital_E pin = 0U;

        switch (mux->type)
        {
            case DRV_MUX_TYPE_GPIO:
                pin = (drv_outputAD_channelDigital_E)(mux->config.outputs.gpio.pin_first + bit);
                break;
            case DRV_MUX_TYPE_GPIO_EN:
                pin = (drv_outputAD_channelDigital_E)(mux->config.outputs.gpio_en.pin_first + bit);
                break;
        }

        drv_io_activeState_E state = (output_channel & (1U << bit)) ? DRV_IO_ACTIVE : DRV_IO_INACTIVE;
        drv_outputAD_setDigitalActiveState(pin, state);
    }
}

/**
 * @brief Initialize a multiplexer
 * @param mux The multiplexer to initialize
 * @return The current channel being output
 */
uint8_t drv_mux_getMuxOutput(drv_mux_channel_S * mux)
{
    if (mux == NULL) return 0;
    return mux->data.channel_output;
}

/**
 * @brief Initialize a multiplexer
 * @param mux The multiplexer to initialize
 * @param enabled The enable state to set
 */
void drv_mux_setEnabled(drv_mux_channel_S * mux, bool enabled)
{
    if (mux == NULL) return;

    mux->data.is_enabled = enabled;

    if (mux->type == DRV_MUX_TYPE_GPIO_EN)
    {
        drv_outputAD_channelDigital_E enable_pin = mux->config.outputs.gpio_en.enable;
        drv_io_activeState_E state = enabled ? DRV_IO_ACTIVE : DRV_IO_INACTIVE;
        drv_outputAD_setDigitalActiveState(enable_pin, state);
    }
}

/**
 * @brief Initialize a multiplexer
 * @param mux The multiplexer to initialize
 * @return The current enable state of the mux
 */
bool drv_mux_getEnabled(drv_mux_channel_S * mux)
{
    if (mux == NULL) return false;
    return mux->data.is_enabled;
}
