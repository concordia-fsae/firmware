/**
 RX_config* CAN.h
 * Header file for CANRX configuration
 */

#pragma once


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// imports for timebase
#include "HW_tim.h"

// imports for CAN generated types
#include "CANTypes_generated.h"

// imports for data access
#include "IO.h"
#include "BMS.h"
#include "Sys.h"
#include "IMD.h"
#include "ENV.h"

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

uint8_t CANIO_tx_getNLG513ControlByte(void);
CAN_prechargeContactorState_E CANIO_tx_getContactorState(void);

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CANIO_UDS_BUFFER_LENGTH 8U
#define CANIO_getTimeMs() (HW_TIM_getTimeMS())

#define set_criticalDataCounter(m,b,n,s) set(m,b,n,s, cantx_counter.counter_100Hz)
#define set_packChargeLimit(m,b,n,s) set(m,b,n,s, BMS.pack_charge_limit)
#define set_packDischargeLimit(m,b,n,s) set(m,b,n,s, BMS.pack_discharge_limit)
#define set_packVoltage(m,b,n,s) set(m,b,n,s, BMS.pack_voltage)
#define set_packCurrent(m,b,n,s) set(m,b,n,s, BMS.pack_current)
#define set_packContactorState(m,b,n,s) set(m,b,n,s, CANIO_tx_getContactorState())
#define set_nlg513ControlByte(m,b,n,s) set(m,b,n,s, CANIO_tx_getNLG513ControlByte())
#define set_nlg513MaxMainsCurrent(m,b,n,s) set(m,b,n,s, 16.0f)
#define set_nlg513MaxChargeVoltage(m,b,n,s) set(m,b,n,s, BMS_CONFIGURED_PACK_MAX_VOLTAGE)
#define set_nlg513MaxChargeCurrent(m,b,n,s) set(m,b,n,s, BMS.pack_charge_limit)
#define transmit_BMSB_brusaChargeCommand (SYS_SFT_checkChargerTimeout() == false)

#include "TemporaryStubbing.h"
