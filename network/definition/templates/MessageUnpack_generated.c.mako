/*
 * MessageUnpack_generated.c
 * Header for can message stuff
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "MessageUnpack_generated.h"
#include "SigRx.h"
#include "string.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/
%for node in nodes:
  %for bus in node.on_buses:

static CANRX_${bus.upper()}_signals_S CANRX_${bus.upper()}_signals;
static CANRX_${bus.upper()}_messages_S CANRX_${bus.upper()}_messages;
  %endfor
%endfor

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void CANRX_init(void)
{
%for node in nodes:
  %for bus in node.on_buses:
    memset(&CANRX_${bus.upper()}_signals, 0U, sizeof(CANRX_${bus.upper()}_signals));
    memset(&CANRX_${bus.upper()}_messages, 0U, sizeof(CANRX_${bus.upper()}_messages));
  %endfor
%endfor
}
%for node in nodes:
  %for bus in node.on_buses:

void CANRX_${bus.upper()}_unpackMessage(const uint16_t id, const CAN_data_T *const m)
{
    switch(id)
    {
    %for message in node.received_msgs:
      %if bus in node.received_msgs[message].source_buses:
        case ${node.received_msgs[message].id}U:
        %if node.received_msgs[message].node_ref.duplicateNode:
          CANRX_${bus.upper()}_unpack_${node.received_msgs[message].node_ref.name.upper()}_${node.received_msgs[message].name.split('_')[1]}(&CANRX_${bus.upper()}_signals, &CANRX_${bus.upper()}_messages, m, ${node.received_msgs[message].node_ref.offset});
        %else:
            CANRX_${bus.upper()}_unpack_${node.received_msgs[message].name}(&CANRX_${bus.upper()}_signals, &CANRX_${bus.upper()}_messages, m);
        %endif
            break;

      %endif
    %endfor
        default:
            // Do nothing
            break;
    }
}
    %for message in node.received_msgs:
      %if bus in node.received_msgs[message].source_buses:
        %for signal in node.received_msgs[message].signals:
          %if signal in node.received_sigs:
<%
  duplicate = node.received_msgs[message].node_ref.duplicateNode
  if duplicate and node.received_msgs[message].node_ref.offset != 0:
    continue
  arg = ', uint8_t nodeId' if duplicate else ''
  index = '[nodeId]' if duplicate else ''
  nodeStr = 'nodeId' if duplicate else ''
  sig_name = node.received_sigs[signal].name if not duplicate else node.received_msgs[message].node_ref.name.upper() + '_' + signal.split('_')[1]
  msg_name = node.received_msgs[message].name if not duplicate else node.received_msgs[message].node_ref.name.upper() + '_' + node.received_msgs[message].name.split('_')[1]
%>\
            %if node.received_sigs[signal].discrete_values:

CANRX_MESSAGE_health_E CANRX_${bus.upper()}_get_${sig_name}(CAN_${node.received_sigs[signal].discrete_values.name}_E * const val${arg})
{
            %elif node.received_sigs[signal].native_representation.bit_width == 1:

CANRX_MESSAGE_health_E CANRX_${bus.upper()}_get_${sig_name}(bool * const val${arg}) 
{
            %else:

CANRX_MESSAGE_health_E CANRX_${bus.upper()}_get_${sig_name}(${node.received_sigs[signal].datatype.name} * const val${arg})
{
            %endif
    *val = CANRX_${bus.upper()}_signals.${sig_name}${index};

    CANRX_MESSAGE_health_E health = CANRX_${bus.upper()}_validate_${msg_name}(${nodeStr});

    return health;
}
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
  if duplicate and node.received_msgs[message].node_ref.offset != 0:
    continue
  arg = 'uint8_t nodeId' if duplicate else 'void'
  index = '[nodeId]' if duplicate else ''
  msg_name = node.received_msgs[message].name if not duplicate else node.received_msgs[message].node_ref.name.upper() + '_' + node.received_msgs[message].name.split('_')[1]
%>\

CANRX_MESSAGE_health_E CANRX_${bus.upper()}_validate_${msg_name}(${arg})
{
    CANRX_MESSAGE_health_E ret = CANRX_MESSAGE_SNA;

    if (CANRX_${bus.upper()}_messages.${msg_name}${index}.timestamp == 0U)
    {
        // Stays SNA
    }
    if (CANRX_${bus.upper()}_messages.${msg_name}${index}.timestamp < (CANRX_getTimeMs() - ${int(node.received_msgs[message].cycle_time_ms * 10)}U))
    {
        ret = CANRX_MESSAGE_MIA;
    }
    else
    {
        ret = CANRX_MESSAGE_VALID;
    }

    return ret;
}
      %endif
    %endfor
  %endfor
%endfor
