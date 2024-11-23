<%namespace file="message_unpack.mako" import = "*"/>\
/*
 * MessageUnack_generated.h
 * Header for can message stuff
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "NetworkDefines_generated.h"
#include "CANTypes_generated.h"
#include "CAN/CanTypes.h"
#include "Utility.h"
#include "SigRx.h"

 /******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CANRX_validate(bus, signal) (JOIN3(JOIN3(CANRX_,bus,_validate),_,signal)())
#define CANRX_validateDuplicate(bus, signal, nodeId) (JOIN3(JOIN3(CANRX_,bus,_validate),_,signal)(nodeId))
#define CANRX_get_signal(bus, signal, val) (JOIN3(JOIN3(CANRX_,bus,_get),_,signal))(val)
#define CANRX_get_signalDuplicate(bus, signal, val, nodeId) (JOIN3(JOIN3(CANRX_,bus,_get),_,signal))(val, nodeId)
#define CANRX_get_signal_timeSinceLastMessageMS(bus, signal) (JOIN3(JOIN3(CANRX_,bus,_get),_,JOIN(signal,_timeSinceLastMessageMS)))()

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/
%for node in nodes:
  %for bus in node.on_buses:

static const uint16_t CANRX_${bus.upper()}_unpackList[] = {
    %for message in node.received_msgs:
      %if bus in node.received_msgs[message].source_buses:
    CAN_${bus.upper()}_${message}_ID,
      %endif
    %endfor
};\
<%
  length = 0
  for message in node.received_msgs:
    if bus in node.received_msgs[message].source_buses:
      length += 1
%>
#define CANRX_${bus.upper()}_unpackList_length ${length}U
  %endfor
%endfor

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void CANRX_init(void);
%for node in nodes:
  %for bus in node.on_buses:
void CANRX_${bus.upper()}_unpackMessage(const uint16_t id, const CAN_data_T *const m);
  %endfor
%endfor
%for node in nodes:
  %for bus in node.on_buses:
    %for message in node.received_msgs:
      %if bus in node.received_msgs[message].source_buses:
        %for signal in node.received_msgs[message].signals:
          %if signal in node.received_sigs:
<%
  duplicate = node.received_sigs[signal].message_ref.node_ref.duplicateNode
  offset = node.received_sigs[signal].message_ref.node_ref.offset
  if duplicate and offset != 0:
    continue
  sig_name = node.received_sigs[signal].message_ref.node_ref.name.upper() + '_' + signal.split('_')[1] if duplicate else node.received_sigs[signal].name
  arg = ', uint8_t nodeId' if duplicate else ''
  argNodeOnly = 'uint8_t nodeId' if duplicate else 'void'
  if node.received_sigs[signal].discrete_values:
    type = 'CAN_' + node.received_sigs[signal].discrete_values.name + '_E'
  elif node.received_sigs[signal].native_representation.bit_width == 1:
    type = 'bool'
  else:
    type = node.received_sigs[signal].datatype.name
%>\
CANRX_MESSAGE_health_E CANRX_${bus.upper()}_get_${sig_name}(${type} * const val${arg});
uint32_t CANRX_${bus.upper()}_get_${sig_name}_timeSinceLastMessageMS(${argNodeOnly});
          %endif
        %endfor
      %endif
    %endfor
  %endfor
%endfor

%for node in nodes:
  %for bus in node.on_buses:
    %for message in node.received_msgs:
      %if bus in node.received_msgs[message].source_buses:
<%
  duplicate = node.received_msgs[message].node_ref.duplicateNode
  offset = node.received_msgs[message].node_ref.offset
  if duplicate and offset != 0:
    continue
  msg_name = node.received_msgs[message].node_ref.name.upper() + '_' + message.split('_')[1] if duplicate else message
  arg = ', uint8_t nodeId' if duplicate else ''
  argNodeOnly = 'uint8_t nodeId' if duplicate else 'void'
  index = '[nodeId]' if duplicate else ''
%>\

CANRX_MESSAGE_health_E CANRX_${bus.upper()}_validate_${msg_name}(${argNodeOnly});
__attribute__((always_inline)) static inline void CANRX_${bus.upper()}_unpack_${msg_name}(CANRX_${bus.upper()}_signals_S* sigrx, CANRX_${bus.upper()}_messages_S* msgrx, const CAN_data_T *const m${arg})
{
    UNUSED(sigrx); UNUSED(msgrx); UNUSED(m); // For messages with unrecorded signals (Immediately processed at the CAN Layer)
    bool valid = true;
            %if node.received_msgs[message].checksum_sig is not None:

    valid = false;
    <%make_sigunpack(bus, node, node.received_msgs[message].checksum_sig, duplicate)%>\
            %endif
            %if node.received_msgs[message].counter_sig is not None:

                uint8_t oldCount = sigrx->${node.received_msgs[message].counter_sig.message_ref.node_ref.name.upper()}_${node.received_msgs[message].counter_sig.name.split('_')[1]}${index};
<%make_sigunpack(bus, node, node.received_msgs[message].counter_sig, True)%>\
    if ((sigrx->${node.received_msgs[message].counter_sig.message_ref.node_ref.name.upper()}_${node.received_msgs[message].counter_sig.name.split('_')[1]}${index} == oldCount + 1) ||
        ((oldCount == ${2**(node.received_msgs[message].counter_sig.native_representation.bit_width) - 1}U) &&
         (sigrx->${node.received_msgs[message].counter_sig.message_ref.node_ref.name.upper()}_${node.received_msgs[message].counter_sig.name.split('_')[1]}${index} == 0U)))
    {
        // Stays valid
    }
    else
    {
        valid = false;
    }
            %endif

    if (valid == false)
    {
        return;
    }

            %for signal in node.received_msgs[message].signals:
              %if signal in node.received_sigs:
                %if signal in node.received_sigs and node.received_msgs[message].checksum_sig is None and node.received_msgs[message].counter_sig is None:
<%make_sigunpack(bus, node, node.received_msgs[message].signal_objs[signal], duplicate)%>\
                %elif node.received_msgs[message].counter_sig != None:
                  %if node.received_msgs[message].counter_sig.name != signal:
<%make_sigunpack(bus, node, node.received_msgs[message].signal_objs[signal], duplicate)%>\
                  %endif
                %elif node.received_msgs[message].checksum_sig != None:
                  %if node.received_msgs[message].checksum_sig.name != signal:
<%make_sigunpack(bus, node, node.received_msgs[message].signal_objs[signal], duplicate)%>\
                  %endif
                %endif
              %endif
            %endfor
    msgrx->${node.received_msgs[message].node_ref.name.upper()}_${node.received_msgs[message].name.split('_')[1]}${index}.timestamp = CANRX_getTimeMs();
}
      %endif
    %endfor
  %endfor
%endfor

