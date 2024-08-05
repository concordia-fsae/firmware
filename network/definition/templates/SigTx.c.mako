<%namespace file="message_pack.mako" import = "*"/>\
/*
 * SigTx.c
 * Signal packing function definitions
 */

#include "CAN/CanTypes.h"
#include "lib_atomic.h"

%for node in nodes:
  %for bus in node.on_buses:
    %for msg in node.messages.values():
      %if bus in msg.source_buses:
        %for signal in msg.signal_objs.values():
<%make_sigpack(bus, node.name, signal)%>\
        %endfor
      %endif
    %endfor
  %endfor
%endfor
