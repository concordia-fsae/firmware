/**
* @file vd.h
* @brief Module header for the live vehicle dynamics calculations
*/

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"
#include "app_vehicleSpeed.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define VEHICLE_MASS_KG 296.0f
#define TIRE_RADIUS_M 0.2032f
#define STRAIGHTLINE_RADIUS 0.0f

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t vd_getIntendedRadius(void);
float32_t vd_getMaxLonTireForce(wheel_E wheel);
