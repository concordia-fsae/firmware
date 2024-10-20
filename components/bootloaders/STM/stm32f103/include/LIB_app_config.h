
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

#define LIB_APP_START_FLASH APP_FLASH_START
#define LIB_APP_END_FLASH APP_FLASH_END

#endif // APP_LIB_ENABLED
