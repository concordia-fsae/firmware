<%! from classes.Types import DiscreteValue, CType %>
<%! from classes.Can import DiscreteValues %>
/*
 * CANTypes_generated.h
 * Defines all DiscreteValue CAN Types on the network
 */

#pragma once
<%
  faults_sent = []
  faults_received = {}
  has_faults = False

  for node in nodes:
    for message in node.messages.values():
      if message.fault_message and not message.from_bridge:
        has_faults = True
        for signal, data in message.signal_objs.items():
          faults_sent.append((signal, data.start_bit))
    for message, mdata in node.received_msgs.items():
      if mdata.fault_message:
        for signal, sdata in mdata.signal_objs.items():
          if mdata.node_ref.name in faults_received:
            faults_received[mdata.node_ref.name].append((signal, sdata.start_bit))
          else:
            faults_received[mdata.node_ref.name] = [(signal, sdata.start_bit)]
%>\
%if len(faults_received) > 0:

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/
  %for node, faults in faults_received.items():

    %for fault, index in faults:
#define FM_FAULT_${fault.upper()} ${int(index)}
    %endfor
  %endfor
%endif

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    CANRX_MESSAGE_SNA = 0U,
    CANRX_MESSAGE_VALID,
    CANRX_MESSAGE_MIA,
} CANRX_MESSAGE_health_E;
%if has_faults:

typedef enum
{
  %for fault, index in faults_sent:
    FM_FAULT_${fault.upper()} = ${int(index)},
  %endfor
    FM_FAULT_COUNT = ${int(index + 1)},
} FM_fault_E;
%endif
%for name, dv in DiscreteValues():

typedef enum
{
  %for d in dv.values:
    CAN_${name.upper()}_${d.upper()} = ${dv.values[d]}U,
  %endfor
} CAN_${name}_E;
%endfor




