<%namespace file="message_pack.mako" import = "*"/>\
/*
* SigTx.h
 * Signal packing function definitions
 */
 
 /******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CANTypes_generated.h"
#include "CAN/CanTypes.h"
#include "lib_atomic.h"
#include "Utility.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/
 
#define set(msg, bus, node, signal, val)               SNAKE4(set, bus, node, signal)(msg, val)
#define unsent_signal(m)                               UNUSED(m)

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/
<%
seen_sigpacks = set()
%>\

%for node in nodes:
  %for bus in node.on_buses:
    %for msg in node.messages.values():
      %if bus in msg.source_buses:
        %for signal in msg.signal_objs.values():
<%
sigpack_name = f"set_{bus.upper()}_{signal.message_ref.node_name}_{signal.get_name_nodeless()}"
%>\
          %if sigpack_name not in seen_sigpacks:
<%
seen_sigpacks.add(sigpack_name)
%>\
<%make_sigpack(bus, node.alias, signal)%>\
          %endif
        %endfor
      %endif
    %endfor
  %endfor
%endfor
