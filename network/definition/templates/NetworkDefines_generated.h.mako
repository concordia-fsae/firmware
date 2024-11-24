<%namespace file="message_pack.mako" import = "*"/>\
/*
 * NetworkDefines_generated.h
 * Node specific Network deifnitions
 */

 #pragma once

/*****************************************************V*************************
 *                              D E F I N E S
 ******************************************************************************/

%for node in nodes:
// Node Properties
  %if node.duplicateNode:
#define CAN_NODE_OFFSET ${node.offset}
  %endif
typedef enum
{
  %for bus in node.on_buses:
    CAN_BUS_${bus.upper()},
  %endfor
    CAN_BUS_COUNT,
} CAN_bus_E;
 
// Transmission ID's
  %for bus in node.on_buses:
    %for msg in node.messages.values():
      %if bus in msg.source_buses:
<%
  msg_name = msg.name if not msg.node_ref.duplicateNode else msg.node_ref.name.upper() + '_' + msg.name.split('_')[1]
%>\
#define CAN_${bus.upper()}_${msg_name}_ID ${msg.id}U
      %endif
    %endfor

// Reception ID's
    %for msg in node.received_msgs.values():
      %if bus in msg.source_buses:
#define CAN_${bus.upper()}_${msg.name}_ID ${msg.id}U
      %endif
    %endfor
  %endfor
%endfor


