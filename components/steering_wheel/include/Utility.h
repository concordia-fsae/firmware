/**
 * Utility.h
 * This file contains various useful tools
 */

#pragma once

/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

#define COUNTOF(x)           ((uint16_t)(sizeof(x) / sizeof(x[0]))) // return size of an array as uint16

#define VAR_IN_SECTION(s)    __attribute__((section(s)))            // place a given variable in the specified linker section
