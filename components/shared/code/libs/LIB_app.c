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
#endif // APP_LIB_ENABLED
