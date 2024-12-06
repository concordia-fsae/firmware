/**
 * @file LIB_app.h
 * @brief Header file for generic application functions.
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_app.h"

#if FEATURE_IS_ENABLED(APP_LIB_ENABLED)
__attribute__((weak)) void _init(void){}

#if FEATURE_IS_ENABLED(APP_VALIDATION_ENABLED)
#include "LIB_app_config.h"

bool lib_app_validateAppDesc(const lib_app_appDesc_S * const appDesc, const lib_app_appDescValid_E validation)
{
    bool ret = false;

    switch(validation)
    {
        case APPDESC_VALID_START:
            ret = appDesc->appStart >= LIB_APP_FLASH_START;
            break;

        case APPDESC_VALID_END:
            ret = appDesc->appEnd <= (LIB_APP_FLASH_END - sizeof(lib_app_crc_t));
            break;

        case APPDESC_VALID_CRCLOCATION:
            ret = (appDesc->appCrcLocation <= (LIB_APP_FLASH_END - sizeof(lib_app_crc_t))) && (appDesc->appCrcLocation >= LIB_APP_FLASH_START);
            break;

        case APPDESC_VALID_COUNT:
        default:
            break;
    }

    return ret;
}

bool lib_app_validateApp(const lib_app_appDesc_S * const hwDesc, const lib_app_appDesc_S * const appDesc, const lib_app_appValid_E validation)
{
    bool ret = false;

    switch(validation)
    {
#if FEATURE_IS_ENABLED(APP_VALIDATE_CRC_ENABLED)
#error "CRC support has not been built out for LIB_app.c/h"
        case APP_VALID_CRC:
            break;
#endif
        case APP_VALID_PCBA_ID:
            ret = hwDesc->appPcbaId == appDesc->appPcbaId;
            break;

        case APP_VALID_COMPONENT_ID:
            ret = hwDesc->appComponentId == appDesc->appComponentId;
            break;

#if FEATURE_IS_ENABLED(APP_NODE_ID)
        case APP_VALID_NODE_ID:
            ret = hwDesc->appNodeId == appDesc->appNodeId;
            break;
#endif // APP_NODE_ID

        case APP_VALID_COUNT:
        default:
            break;
    }

    return ret;
}
#endif // APP_VALIDATION_ENABLED
#endif // APP_LIB_ENABLED
