
/**
 * @file LIB_app_config.h
 * @brief Header file for generic application functions.
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_app.h"

#if FEATURE_IS_ENABLED(APP_LIB_ENABLED)
#include "HW_FLASH.h"

#define LIB_APP_FLASH_START APP_FLASH_START
#define LIB_APP_FLASH_END APP_FLASH_END

#endif // APP_LIB_ENABLED
