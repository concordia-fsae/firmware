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
%endfor
