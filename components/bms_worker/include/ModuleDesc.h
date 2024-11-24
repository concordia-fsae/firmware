/**
 * @file ModuleDesc.h
 * @brief  Header file for Modules Descriptor table
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version
 * @date 2024-01-30
 */

#pragma once

#include "FeatureDefines_generated.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

/**
 * @brief  Module Descriptor
 */
typedef struct
{
    void (*moduleInit)(void);           // Pointer to module init function
#if FEATURE_IS_ENABLED(APP_10KHZ_TASK)
    void (*periodic10kHz_CLK)(void);    // Pointer to module 10kHz periodic function
#endif // APP_10KHZ_TASK
    void (*periodic1kHz_CLK)(void);     // Pointer to module 1kHz periodic function
    void (*periodic100Hz_CLK)(void);    // Pointer to module 100Hz periodic function
    void (*periodic10Hz_CLK)(void);     // Pointer to module 10Hz periodic function
    void (*periodic1Hz_CLK)(void);      // Pointer to module 1Hz periodic function
} ModuleDesc_S;
