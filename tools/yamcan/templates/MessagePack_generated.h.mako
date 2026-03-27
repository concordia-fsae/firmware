/*
 * MessagePack_generated.h
 * Header for can message stuff
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "YamcanTypes.h"
#include "LIB_FloatTypes.h"
#include "Utility.h"
#include "NetworkDefines_generated.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CANTX_inject_pending(bus, message) (JOIN(JOIN3(CANTX_inject_pending_,bus,_),message)())
#define CANTX_inject(bus, message, m) (JOIN(JOIN3(CANTX_inject_,bus,_),message)(m))

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/
 

%for node in nodes:
extern canTable_S CAN_table[CAN_BUS_COUNT];
%endfor

typedef struct
{
    CAN_data_T raw;
    bool       pending;
} CANTX_injectedMessage_S;

%for node in nodes:
  %for bus in node.on_buses:
    %for msg in node.messages.values():
      %if bus in msg.source_buses and msg.injected_tx:
bool CANTX_inject_${bus.upper()}_${msg.name}(const CAN_data_T *message);
bool CANTX_inject_pending_${bus.upper()}_${msg.name}(void);
      %endif
    %endfor
  %endfor
%endfor
