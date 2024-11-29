<%namespace file="message_unpack.mako" import = "*"/>\
<%! import classes.Types %>
/*
 * SigRx.h
 * Signal unpacking function definitions
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "MessageUnpack_generated.h"
#include "CANTypes_generated.h"
#include "stdbool.h"
#include "stdint.h"
#include "FloatTypes.h"
#include "Utility.h"

#include "CAN/CANIO_componentSpecific.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/
 %for node in nodes:
   %for bus in node.on_buses:

typedef struct
{
    %for msg in node.received_msgs:
      %if bus in node.received_msgs[msg].source_buses:
        %if node.received_msgs[msg].node_ref.duplicateNode:
          %if node.received_msgs[msg].node_ref.offset != 0:
<%continue%>\
          %else:
<%make_structdef_messageDuplicates(node, msg, node.received_msgs[msg].node_ref.total_duplicates)%>\
          %endif
        %else:
<%make_structdef_message(node, msg)%>\
        %endif
      %endif
    %endfor
} CANRX_${bus.upper()}_messages_S;
  %endfor
%endfor
 %for node in nodes:
   %for bus in node.on_buses:

typedef struct
{
    %for sig in node.received_sigs:
      %if bus in node.received_sigs[sig].message_ref.source_buses:
        %if node.received_sigs[sig].message_ref.node_ref.duplicateNode:
          %if node.received_sigs[sig].message_ref.node_ref.offset != 0:
<%continue%>\
          %else:
<%make_structdef_signalDuplicates(node, sig, node.received_sigs[sig].message_ref.node_ref.total_duplicates)%>\
          %endif
        %else:
<%make_structdef_signal(node, sig)%>\
        %endif
      %endif
    %endfor
} CANRX_${bus.upper()}_signals_S;
  %endfor
%endfor
