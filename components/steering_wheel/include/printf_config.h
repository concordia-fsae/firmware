/**
 * printf_config.h
 * Config file for the lightweight printf library in /platforms/libs/printf
 */

#pragma once

#include <stdbool.h>

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define PRINTF_SUPPORT_DECIMAL_SPECIFIERS               true
#define PRINTF_SUPPORT_EXPONENTIAL_SPECIFIERS           false
#define PRINTF_SUPPORT_WRITEBACK_SPECIFIER              false
#define PRINTF_SUPPORT_MSVC_STYLE_INTEGER_SPECIFIERS    false
#define PRINTF_SUPPORT_LONG_LONG                        false

#define PRINTF_ALIAS_STANDARD_FUNCTION_NAMES            true
#define PRINTF_ALIAS_STANDARD_FUNCTION_NAMES_SOFT       false
#define PRINTF_ALIAS_STANDARD_FUNCTION_NAMES_HARD       true

#define PRINTF_INTEGER_BUFFER_SIZE                      32
#define PRINTF_DECIMAL_BUFFER_SIZE                      32
#define PRINTF_DEFAULT_FLOAT_PRECISION                  6
#define PRINTF_MAX_INTEGRAL_DIGITS_FOR_DECIMAL          9
#define PRINTF_LOG10_TAYLOR_TERMS                       4
#define PRINTF_CHECK_FOR_NUL_IN_FORMAT_SPECIFIER        true
