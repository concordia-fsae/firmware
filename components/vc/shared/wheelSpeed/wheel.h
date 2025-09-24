/**
 * @file wheel.h
 * @brief  Header file for generic wheel definitions
 */

#pragma once

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    WHEEL_FL = 0x00,
    WHEEL_FR,
    WHEEL_RL,
    WHEEL_RR,
    WHEEL_CNT,
} wheel_E;

typedef enum
{
    AXLE_FRONT = 0x00,
    AXLE_REAR,
    AXLE_CNT,
} axle_E;
