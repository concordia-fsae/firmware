<%namespace file="message_pack.mako" import = "*"/>\
/*
 * TemporaryStubbing.h
 * Signal packing function definitions - stubbed
 */

/*****************************************************V*************************
 *                              D E F I N E S
 ******************************************************************************/

#ifndef set 
#define set(msg, bus, node, signal, val) SNAKE4(set, bus, node, signal)(msg, val)
#endif

#ifndef unsent_signal
#define unsent_signal(m) UNUSED(m)
#endif
\
%for node in nodes:
  %for bus in node.on_buses:
    %for msg in node.messages.values():
      %if bus in msg.source_buses:
<%make_transmitstub(msg)%>\
        %for signal in msg.signal_objs.values():
<%make_sigstub(signal)%>\
        %endfor
      %endif
    %endfor
  %endfor
%endfor
