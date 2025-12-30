/**
 * @file app_vehicleState_componentSpecific.h
 * @brief Header file for the component specific vehicle state application
 */

#pragma once

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define VEHICLESTATE_CANRX_SIGNAL       CANRX_get_signal_func(VEH, VCPDU_vehicleState)
#define VEHICLESTATE_CANRX_RESET_SWITCH CANRX_get_signal_func(VEH, VCPDU_safetyReset)
