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


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

%for node in nodes:
  %for bus in node.on_buses:
    %for cycle_time, msgs in node.messages_by_cycle_time().items():
#define ${bus.upper()}_packTable_${cycle_time}ms_length ${len(msgs)}U
    %endfor
  %endfor
%endfor
