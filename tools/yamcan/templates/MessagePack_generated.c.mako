<%namespace file="message_pack.mako" import = "*"/>\
/**
 * MessagePack_generated.c
 * Defines packing functions and packtables for messages
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "YamcanTypes.h"
#include "MessagePack_generated.h"
#include "NetworkDefines_generated.h"
#include "SigTx.h"
#include "MessageUnpack_generated.h"
#include "YamcanConfig.h"
#include "TemporaryStubbing.h"

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/
\
%for node in nodes:
  %for bus in node.on_buses:
    %for msg in node.messages.values():
      %if bus in msg.source_buses and msg.injected_tx:
static CANTX_injectedMessage_S CANTX_inject_${bus.upper()}_${msg.name}_state;

bool CANTX_inject_${bus.upper()}_${msg.name}(const CAN_data_T *message)
{
    if (CANTX_inject_${bus.upper()}_${msg.name}_state.pending)
    {
        return false;
    }

    CANTX_inject_${bus.upper()}_${msg.name}_state.raw = *message;
    CANTX_inject_${bus.upper()}_${msg.name}_state.pending = true;
    return true;
}

bool CANTX_inject_pending_${bus.upper()}_${msg.name}(void)
{
    return CANTX_inject_${bus.upper()}_${msg.name}_state.pending;
}
      %endif
    %endfor
  %endfor
%endfor
%for node in nodes:
  %for bus in node.on_buses:
    %for cycle_time, msgs in node.messages_by_cycle_time().items():
      %for msg in msgs:
        %if bus in msg.source_buses:
<%make_packfn(bus, msg)%>\
        %endif
      %endfor
<%make_packtable(bus, msgs, cycle_time)%>\
    %endfor
  %endfor
  %for bus in node.on_buses:

busTable_S ${bus.upper()}_table[] = {
    %for cycle_time, msgs in node.messages_by_cycle_time().items():
    {
        .packTable = ${bus.upper()}_packTable_${cycle_time}ms,
        .packTableLength = COUNTOF(${bus.upper()}_packTable_${cycle_time}ms),
        .period = ${cycle_time}U,
        .counter = 0U,
        .index = 0U,
        .lastTimestamp = 0U,
    },
    %endfor
};
  %endfor

canTable_S CAN_table[CAN_BUS_COUNT] = {
  %for bus in node.on_buses:
    [CAN_BUS_${bus.upper()}] = {
        .busTable = ${bus.upper()}_table,
        .busTableLength = COUNTOF(${bus.upper()}_table),
    },
  %endfor
};
%endfor
