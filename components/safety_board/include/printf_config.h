/**
 * @file printf_config.h
 * @brief  Config file for lightweight printf library
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2023-01-15
 */
#pragma once

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define PRINTF_DEFAULT_FLOAT_PRECISION 2

// can enable if needed but not worth the memory if not
#define PRINTF_DISABLE_SUPPORT_EXPONENTIAL
#define PRINTF_DISABLE_SUPPORT_LONG_LONG
#define PRINTF_DISABLE_SUPPORT_PTRDIFF_T
