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
            CANRX_${bus.upper()}_unpack_${node.received_msgs[message].name}(&CANRX_${bus.upper()}_signals, &CANRX_${bus.upper()}_messages, m);
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
            %if node.received_sigs[signal].discrete_values:

CANRX_SIGNAL_${node.received_sigs[signal].discrete_values.name} CANRX_${bus.upper()}_get_${node.received_sigs[signal].name}_verbose(void)
{
    CANRX_SIGNAL_${node.received_sigs[signal].discrete_values.name} ret = {0U};
            %elif node.received_sigs[signal].native_representation.bit_width == 1:

CANRX_SIGNAL_bool CANRX_${bus.upper()}_get_${node.received_sigs[signal].name}_verbose(void)
{
    CANRX_SIGNAL_bool ret = {0U};
            %else:

CANRX_SIGNAL_${node.received_sigs[signal].datatype.name} CANRX_${bus.upper()}_get_${node.received_sigs[signal].name}_verbose(void)
{
    CANRX_SIGNAL_${node.received_sigs[signal].datatype.name} ret = {0U};
            %endif
    ret.value = CANRX_${bus.upper()}_signals.${node.received_sigs[signal].name};
    ret.health = CANRX_${bus.upper()}_validate_${node.received_msgs[message].name}(&CANRX_${bus.upper()}_messages);
    return ret;
}
            %if node.received_sigs[signal].discrete_values:

CAN_${node.received_sigs[signal].discrete_values.name}_E CANRX_${bus.upper()}_get_${node.received_sigs[signal].name}(void)
{
            %elif node.received_sigs[signal].native_representation.bit_width == 1:

bool CANRX_${bus.upper()}_get_${node.received_sigs[signal].name}(void)
{
            %else:

${node.received_sigs[signal].datatype.name} CANRX_${bus.upper()}_get_${node.received_sigs[signal].name}(void)
{
            %endif
    return CANRX_${bus.upper()}_signals.${node.received_sigs[signal].name};
}

CANRX_MESSAGE_health_E CANRX_${bus.upper()}_get_${node.received_sigs[signal].name}_health(void)
{
    return CANRX_${bus.upper()}_validate_${node.received_msgs[message].name}(&CANRX_${bus.upper()}_messages);
}

uint32_t CANRX_${bus.upper()}_get_${node.received_sigs[signal].name}_timeSinceLastMessageMS(void)
{
    return CANRX_${bus.upper()}_messages.${node.received_msgs[message].name}.timestamp - CANRX_getTimeMs();
}
          %endif
        %endfor
      %endif
    %endfor
  %endfor
%endfor


