<%namespace file="message_pack.mako" import = "*"/>\
/**
 * MessagePack_generated.c
 * Defines packing functions and packtables for messages
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CAN/CanTypes.h"
#include "MessagePack_generated.h"
#include "NetworkDefines_generated.h"

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/
\
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
busTable_S ${bus.upper()}_table[${len(node.messages_by_cycle_time())}U] = {
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
