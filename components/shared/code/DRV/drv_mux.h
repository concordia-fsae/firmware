/**
 * @file drv_mux.h
 * @brief Header file for a generic multiplexer driver
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "LIB_Types.h"
#include "drv_outputAD.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    const enum
    {
        DRV_MUX_TYPE_GPIO = 0x00U,
        DRV_MUX_TYPE_GPIO_EN,
    } type;
    const struct
    {
        union
        {
            struct
            {
                drv_outputAD_channelDigital_E pin_first;
                drv_outputAD_channelDigital_E pin_last;
                bool disable_on_ch0;
            } gpio;
            struct
            {
                drv_outputAD_channelDigital_E pin_first;
                drv_outputAD_channelDigital_E pin_last;
                drv_outputAD_channelDigital_E enable;
            } gpio_en;
        } outputs;
        uint8_t max_output_channel;
    } config;
    struct
    {
        uint8_t bit_width;
        bool is_enabled;
        uint8_t channel_output;
    } data;
} drv_mux_channel_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void    drv_mux_init(drv_mux_channel_S * mux);
void    drv_mux_setMuxOutput(drv_mux_channel_S * mux, uint8_t output);
uint8_t drv_mux_getMuxOutput(drv_mux_channel_S * mux);
void    drv_mux_setEnabled(drv_mux_channel_S * mux, bool enabled);
bool    drv_mux_getEnabled(drv_mux_channel_S * mux);
