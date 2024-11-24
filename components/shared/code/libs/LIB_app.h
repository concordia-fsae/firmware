/**
 * @file LIB_app.h
 * @brief Header file for generic application functions.
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "FeatureDefines_generated.h"

#if FEATURE_IS_ENABLED(APP_LIB_ENABLED)
#include "stdint.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

// this needs to be defined for __libc_init_array() from newlib_nano to be happy
extern void _init(void);

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef uint32_t lib_app_crc_t;

typedef struct
{
    const uint32_t appStart;
    const uint32_t appEnd;
    const uint32_t appCrcLocation;
    const uint16_t appComponentId;
    const uint16_t appPcbaId;
#if FEATURE_IS_ENABLED(APP_NODE_ID)
    const uint8_t appNodeId;
#endif // APP_NODE_ID
} appDesc_S;

#endif // APP_LIB_ENABLED
