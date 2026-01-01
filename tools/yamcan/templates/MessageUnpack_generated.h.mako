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

#define CANRX_validate_func(bus, signal) (JOIN3(JOIN3(CANRX_,bus,_validate),_,signal))
#define CANRX_validate(bus, signal) (CANRX_validate_func(bus, signal)())
#define CANRX_validateDuplicate(bus, signal, nodeId) (JOIN3(JOIN3(CANRX_,bus,_validate),_,signal)(nodeId))
#define CANRX_get_signal_func(bus, signal) (JOIN3(JOIN3(CANRX_,bus,_get),_,signal))
#define CANRX_get_signal(bus, signal, val) CANRX_get_signal_func(bus, signal)(val)
#define CANRX_get_signalDuplicate(bus, signal, val, nodeId) (JOIN3(JOIN3(CANRX_,bus,_get),_,signal))(val, nodeId)
#define CANRX_get_signal_timeSinceLastMessageMS(bus, signal) (JOIN3(JOIN3(CANRX_,bus,_get),_,JOIN(signal,_timeSinceLastMessageMS)))()
#define CANRX_get_rawMessage(bus, message) (JOIN3(JOIN3(CANRX_,bus,_rawMessage),_,message)())
#define CANRX_get_bridgeWaiting(bus, message) (JOIN3(JOIN3(CANRX_,bus,_rawBridgeWaiting),_,message)())
#define CANRX_set_bridgeWaiting(bus, message, val) (JOIN3(JOIN3(CANRX_,bus,_setRawBridgeWaiting),_,message)(val))

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/
%for node in nodes:
  %for bus in node.on_buses:

extern CANRX_${bus.upper()}_signals_S CANRX_${bus.upper()}_signals;
extern CANRX_${bus.upper()}_messages_S CANRX_${bus.upper()}_messages;
  %endfor
%endfor
%for node in nodes:
  %for bus in node.on_buses:

static const uint16_t CANRX_${bus.upper()}_unpackList[] = {
    %for message in node.received_msgs:
      %if bus in node.received_msgs[message].source_buses and node.received_msgs[message].id <= 0x7ff:
    CAN_${bus.upper()}_${message}_ID,
      %endif
    %endfor
};
static const uint32_t CANRX_${bus.upper()}_unpackListExtID[] = {
    %for message in node.received_msgs:
      %if bus in node.received_msgs[message].source_buses and node.received_msgs[message].id > 0x7ff:
    CAN_${bus.upper()}_${message}_ID,
      %endif
    %endfor
};
<%
  length = 0
  for message in node.received_msgs:
    if bus in node.received_msgs[message].source_buses:
      length += 1
%>\
  %endfor
%endfor

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void CANRX_init(void);
%for node in nodes:
  %for bus in node.on_buses:
void CANRX_${bus.upper()}_unpackMessage(const uint32_t id, const CAN_data_T *const m);
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
void CANRX_${bus.upper()}_unpack_${msg_name}(CANRX_${bus.upper()}_signals_S* sigrx, CANRX_${bus.upper()}_messages_S* msgrx, const CAN_data_T *const m${arg});
      %endif
    %endfor
  %endfor
%endfor

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/
%for node in nodes:
  %for bus in node.on_buses:
    %for message in node.received_msgs:
        %if bus in node.received_msgs[message].source_buses and (node.received_msgs[message].fault_message or node.received_msgs[message].bridged):

inline CAN_data_T* CANRX_${bus.upper()}_rawMessage_${message}(void)
{
    return &CANRX_${bus.upper()}_messages.${message}.raw;
}
      %endif
      %if bus in node.received_msgs[message].source_buses and node.received_msgs[message].bridged:

inline bool CANRX_${bus.upper()}_rawBridgeWaiting_${message}(void)
{
    return CANRX_${bus.upper()}_messages.${message}.new_message;
}

inline void CANRX_${bus.upper()}_setRawBridgeWaiting_${message}(bool val)
{
    CANRX_${bus.upper()}_messages.${message}.new_message = val;
}
      %endif
    %endfor
  %endfor
%endfor
