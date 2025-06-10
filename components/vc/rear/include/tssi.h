/**
 * @file tssi.h
 * @brief Module for Tractive System Status Indicator (TSSI) Lights Based on Status from the Insulation Monitoring Device (IMD).
 */
#pragma once
// #include "HW_gpio.h"
// #include "HW_adc.h"
#include "CANIO_componentSpecific.h"
// #include "CANTypes_generated.h"
// #include "drv_outputAD.h"
#include "MessageUnpack_generated.h"
// #include "NetworkDefines_generated.h"
#include "drv_timer.h"
// #include "HW_can.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define TSSI_BLINK_INTERVAL_MS 500U


/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    imdFault_INIT = 0x00U,
    tssi_RED = 0x01U,
    tssi_GREEN = 0x02U,
} IMD_State_E;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void tssi_init(void);
// void tssi_update(void);
void tssi_reset(void);
// void LEDTimer(void);

IMD_State_E imdFault_getState(void);
CAN_digitalStatus_E BMS_getStatusMemCAN(void);