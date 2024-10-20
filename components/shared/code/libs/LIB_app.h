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
#include <stdint.h>
#include <stdbool.h>

#define FDEF_TO_DID_RESPONSE(fdef) (fdef - 2U)

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
} lib_app_appDesc_S;

typedef enum
{
    APPDESC_VALID_START = 0U,
    APPDESC_VALID_END,
    APPDESC_VALID_CRCLOCATION,
    APPDESC_VALID_COUNT,
} lib_app_appDescValid_E;

typedef enum
{
    APP_VALID_CRC = 0U,
    APP_VALID_PCBA_ID,
    APP_VALID_COMPONENT_ID,
#if FEATURE_IS_ENABLED(APP_NODE_ID)
    APP_VALID_NODE_ID,
#endif // APP_NODE_ID
    APP_VALID_COUNT,
} lib_app_appValid_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool lib_app_validateAppDesc(const lib_app_appDesc_S * const appDesc, const lib_app_appDescValid_E validation);
bool lib_app_validateApp(const lib_app_appDesc_S * const hwDesc, const lib_app_appDesc_S * const appDesc, const lib_app_appValid_E validation);

#endif // APP_LIB_ENABLED
