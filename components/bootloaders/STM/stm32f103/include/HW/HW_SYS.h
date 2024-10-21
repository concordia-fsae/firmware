/*
 * HW_SYS.h
 * This file desribes low-level, mostly hardware-specific System peripheral values
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Types.h"

#include "LIB_app.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    RESET_TYPE_NONE = 0U,
    RESET_TYPE_PERSISTENT_BOOTLOADER,
    RESET_TYPE_JUST_UPDATED,
} resetType_E;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void        SYS_init(void);

void        SYS_resetSoft(void);
void        SYS_resetHard(void);
resetType_E SYS_getResetType(void);
bool        SYS_checkAppValid(appDesc_S *appDesc);
void        SYS_bootApp(uint32_t appAddr);
