/**
 * @file ModuleDesc.h
 * @brief  Header file for Module Description
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-23
 */

#pragma once

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

/*
 * Module Descriptor
 */
typedef struct
{
    void (*moduleInit)(void);        // Pointer to module init function
    void (*periodic1kHz_CLK)(void);  // Pointer to module 1kHz periodic function
    void (*periodic100Hz_CLK)(void); // Pointer to module 100Hz periodic function
    void (*periodic10Hz_CLK)(void);  // Pointer to module 10Hz periodic function
    void (*periodic1Hz_CLK)(void);   // Pointer to module 1Hz periodic function
} ModuleDesc_S;


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

/*
 * Modules
 */
extern const ModuleDesc_S Sensors_desc;

