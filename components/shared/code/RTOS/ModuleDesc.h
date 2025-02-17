/**
 * @file ModuleDesc.h
 * @brief  Header file for Modules Descriptor table
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version
 * @date 2024-01-30
 */

#pragma once

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

/**
 * @brief  Module Descriptor
 */
typedef struct
{
    void (*moduleInit)(void);           // Pointer to module init function
    void (*periodic1kHz_CLK)(void);     // Pointer to module 1kHz periodic function
    void (*periodic100Hz_CLK)(void);    // Pointer to module 100Hz periodic function
    void (*periodic10Hz_CLK)(void);     // Pointer to module 10Hz periodic function
    void (*periodic1Hz_CLK)(void);      // Pointer to module 1Hz periodic function
} ModuleDesc_S;
