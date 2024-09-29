<%namespace file="message_pack.mako" import = "*"/>\
/*
 * NetworkDefines_generated.h
 * Node specific Network deifnitions
 */

/*****************************************************V*************************
 *                              D E F I N E S
 ******************************************************************************/

// Transmission ID's
%for node in nodes:
  %for bus in node.on_buses:
    %for msg in node.messages.values():
      %if bus in msg.source_buses:
#define CAN_${bus.upper()}_${msg.name}_ID ${msg.id}U
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
