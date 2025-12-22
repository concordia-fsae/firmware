/**
 * @file drv_pedalMonitor.h
 * @brief Header file for pedal monitor
 * @note Pedal positon is a float percentage between 0.0f and 1.0f where 
 *       0.0f is 0% and 1.0f is 100%
 *
 * Setup
 * 1. Define the pedal monitor channels in drv_pedalMonitor_componentSpecific.h
 *    and name it drv_pedalMonitor_channel_E
 * 2. Declare and configure the pedal channels in drv_pedalMonitor_componentSpecific.c
 *    and name it drv_pedalMonitor_channels
 * 3. Initialize the pedal monitor driver with drv_pedalMonitor_init
 *
 * Usage
 * - Periodically (at the desired frequency) call the drv_pedalMonitor_run function
 * - Get the current pedal position with the drv_pedalMonitor_getPedalPosition function
 *   Note: Read the note attached at the top of this file for pedal percentage mapping
 * - The pedal_map of an analog channel must saturate left and saturate right, with a
 *   min value of 0.0f and a max value of 1.0f
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_pedalMonitor_componentSpecific.h"
#include "LIB_Types.h"
#include "drv_inputAD.h"
#include "lib_interpolation.h"
#include "CANTypes_generated.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_PEDALMONITOR_INIT = 0x00,
    DRV_PEDALMONITOR_OK,
    DRV_PEDALMONITOR_FAULT_SHORTED,
    DRV_PEDALMONITOR_FAULT_DISCONNECTED,
    DRV_PEDALMONITOR_FAULT,
} drv_pedalMonitor_state_E;

typedef struct
{
    enum
    {
        DRV_PEDALMONITOR_TYPE_ANALOG = 0x00U,
        DRV_PEDALMONITOR_TYPE_CAN,
    } type;
    union
    {
        struct
        {
            drv_inputAD_channelAnalog_E channel;
            float32_t                   fault_high; // When the measured voltage is above this, fault
            float32_t                   fault_low; // When the voltage is below this, fault
            lib_interpolation_mapping_S pedal_map;
        } analog;
        CANRX_MESSAGE_health_E (*canrx_getPedalPosition)(float32_t* percentage);
    } input;
} drv_pedalMonitor_channelConfig_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void                     drv_pedalMonitor_init(void);
void                     drv_pedalMonitor_run(void);
float32_t                drv_pedalMonitor_getPedalPosition(drv_pedalMonitor_channel_E channel);
drv_pedalMonitor_state_E drv_pedalMonitor_getPedalState(drv_pedalMonitor_channel_E channel);
CAN_pedalState_E         drv_pedalMonitor_getPedalStateCAN(drv_pedalMonitor_channel_E channel);
float32_t                drv_pedalMonitor_getPedalVoltage(drv_pedalMonitor_channel_E channel);
