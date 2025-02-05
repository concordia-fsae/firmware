/*
 * MessagePack_generated.h
 * Header for can message stuff
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CAN/CanTypes.h"
#include "FloatTypes.h"
#include "Utility.h"
#include "NetworkDefines_generated.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/
 
 %for node in nodes:
extern canTable_S CAN_table[CAN_BUS_COUNT];
%endfor
