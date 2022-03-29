/**
 * Display/Common.h
 * Contains common display function prototypes to be included elsewhere
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "DisplayTypes.h"


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void toggleInfoDotState(dispCommonInfoDots_E infoDot);
void setInfoDot(dispCommonInfoDots_E infoDot);
void clearInfoDot(dispCommonInfoDots_E infoDot);
void assignInfoDot(dispCommonInfoDots_E infoDot, bool cond);
