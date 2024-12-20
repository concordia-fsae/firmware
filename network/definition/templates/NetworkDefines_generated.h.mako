<%namespace file="message_pack.mako" import = "*"/>\
/*
 * NetworkDefines_generated.h
 * Node specific Network deifnitions
 */

 #pragma once

/*****************************************************V*************************
 *                              D E F I N E S
 ******************************************************************************/

// Network Properties
<%
  evaluated_nodes = []
%>\
%for node in nodes:
  %for msg in node.received_msgs.values():
      %if msg.node_ref.duplicateNode and node.name not in evaluated_nodes:
<%evaluated_nodes.append(node.name)%>\
typedef enum
{
      %for i in range(0, msg.node_ref.total_duplicates):
    CAN_DUPLICATENODE_${msg.node_ref.name.upper() + str(i)},
      %endfor
    CAN_DUPLICATENODE_${msg.node_ref.name.upper()}_COUNT,
} CAN_DUPLICATENODE_${msg.node_ref.name.upper()}_E;
    %endif
  %endfor
%endfor

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
#define CAN_${bus.upper()}_${msg_name}_ID ${hex(msg.id)}U
      %endif
    %endfor

// Reception ID's
    %for msg in node.received_msgs.values():
      %if bus in msg.source_buses:
#define CAN_${bus.upper()}_${msg.name}_ID ${hex(msg.id)}U
      %endif
    %endfor
  %endfor
%endfor


