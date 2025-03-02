/**
 * @file lib_voltageDivider.h
 * @brief  Header file for the voltage divider library Library
 */

#pragma once

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define lib_voltageDivider_getRFromVKnownPullUp(voltage, pull_up, vref) (pull_up * ((voltage / vref) / (1.0F - (voltage / vref))))
