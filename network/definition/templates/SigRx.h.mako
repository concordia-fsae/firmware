<%namespace file="message_unpack.mako" import = "*"/>\
/*
 * SigRx.h
 * Signal unpacking function definitions
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "MessageUnpack_generated.h"
#include "stdbool.h"
#include "stdint.h"
#include "FloatTypes.h"
#include "Utility.h"

#include "CAN/CANRX_config.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/
 %for node in nodes:
   %for bus in node.on_buses:

typedef struct
{
    %for msg in node.received_msgs:
      %if bus in node.received_msgs[msg].source_buses:
<%make_structdef_message(node, msg)%>\
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
<%make_structdef_signal(node, sig)%>\
      %endif
    %endfor
} CANRX_${bus.upper()}_signals_S;
  %endfor
%endfor

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/
%for node in nodes:
  %for bus in node.on_buses:
    %for message in node.received_msgs:
      %if bus in node.received_msgs[message].source_buses:

__attribute__((always_inline)) static inline void CANRX_${bus.upper()}_unpack_${node.received_msgs[message].name}(CANRX_${bus.upper()}_signals_S* sigrx, CANRX_${bus.upper()}_messages_S* msgrx, const CAN_data_T *const m)
{
          %for signal in node.received_msgs[message].signals:
            %if signal in node.received_sigs:
          %if signal in node.received_sigs and node.received_msgs[message].checksum_sig is None and node.received_msgs[message].counter_sig is None:
<%make_sigunpack(bus, node, node.received_msgs[message].signal_objs[signal])%>\
          %elif node.received_msgs[message].counter_sig != None:
            %if node.received_msgs[message].counter_sig.name != signal:
<%make_sigunpack(bus, node, node.received_msgs[message].signal_objs[signal])%>\
            %endif
          %elif node.received_msgs[message].checksum_sig != None:
            %if node.received_msgs[message].checksum_sig.name != signal:
<%make_sigunpack(bus, node, node.received_msgs[message].signal_objs[signal])%>\
            %endif
          %endif
        %endif
        %endfor
    msgrx->${message}.timestamp = CANRX_getTimeMs();
        %if node.received_msgs[message].checksum_sig is not None:

<%make_sigunpack(bus, node, node.received_msgs[message].checksum_sig)%>\
    msgrx->${message}.checksumValid = true;
        %elif node.received_msgs[message].counter_sig is not None:

    ${node.received_msgs[message].counter_sig.datatype.name} oldCount = sigrx->${node.received_msgs[message].counter_sig.name};
<%make_sigunpack(bus, node, node.received_msgs[message].counter_sig)%>\
    if (sigrx->${node.received_msgs[message].counter_sig.name} == oldCount + 1)
    {
        msgrx->${message}.counterValid = true;
    }
    else
    {
        msgrx->${message}.counterValid = false;
    }
        %endif
}

__attribute__((always_inline)) static inline CANRX_MESSAGE_health_E CANRX_${bus.upper()}_validate_${node.received_msgs[message].name}(CANRX_${bus.upper()}_messages_S* canrx)
{
    CANRX_MESSAGE_health_E ret = CANRX_MESSAGE_SNA;
    uint8_t failures = 0;

    if (canrx->${node.received_msgs[message].name}.timestamp != 0U)
    {
    %if node.received_msgs[message].checksum_sig is not None:
        if (canrx->${node.received_msgs[message].name}.checksumValid != true)
        {
            failures++;
            ret = CANRX_MESSAGE_CHECKSUM_INVALID;
        }
        %endif
        %if node.received_msgs[message].counter_sig is not None:
        if (canrx->${node.received_msgs[message].name}.counterValid != true)
        {
            failures++;
            ret = CANRX_MESSAGE_COUNTER_INVALID;
        }
        %endif
        if (canrx->${node.received_msgs[message].name}.timestamp < (CANRX_getTimeMs() - ${int(node.received_msgs[message].cycle_time_ms * 1.25)}U))
        {
            failures++;
            ret = CANRX_MESSAGE_MIA;
        }

        if (failures == 0U)
        {
            ret = CANRX_MESSAGE_VALID;
        }
        else if (failures > 1)
        {
            ret = CANRX_MESSAGE_MULTIPLE_FAILURES;
        }
    }

    return ret;
}
      %endif
    %endfor
  %endfor
%endfor


