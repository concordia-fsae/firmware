/**
 * @file app_vehicleState_componentSpecific.h
 * @brief Header file for the component specific vehicle state application
 */

#pragma once

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define VEHICLESTATE_INPUTAD_TSMS DRV_INPUTAD_TSCHG_MS
#define VEHICLESTATE_INPUTAD_RUN_BUTTON DRV_INPUTAD_RUN_BUTTON
#define VEHICLESTATE_CANRX_CONTACTORSTATE CANRX_get_signal_func(VEH, BMSB_packContactorState)
#define VEHICLESTATE_CANRX_BRAKEPOSITION  CANRX_get_signal_func(VEH, VCFRONT_brakePosition)