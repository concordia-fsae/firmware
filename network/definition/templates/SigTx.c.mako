<%namespace file="message_pack.mako" import = "*"/>\
/*
 * SigTx.c
 * Signal packing function definitions
 */
 
 /******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CAN/CanTypes.h"
#include "lib_atomic.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/
 
#define set(msg, bus, node, signal, val)               SNAKE4(set, bus, node, signal)(msg, val)
#define unsent_signal(m)                               UNUSED(m)

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

%for node in nodes:
  %for bus in node.on_buses:
    %for msg in node.messages.values():
      %if bus in msg.source_buses:
        %for signal in msg.signal_objs.values():
<%make_sigpack(bus, node.alias, signal)%>\
        %endfor
      %endif
    %endfor
  %endfor
%endfor


