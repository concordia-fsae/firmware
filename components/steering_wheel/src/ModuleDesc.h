/*
 * ModuleDesc.h
 * This file contains the definition of the Module Description
 */

#pragma once

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

/*
 * Modules
 */
extern const ModuleDesc_S Screen_desc;
