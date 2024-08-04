/*
 * MessageUnack_generated.h
 * Header for can message stuff
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

 #include "CAN/CanTypes.h"
 #include "FloatTypes.h"
 #include "Utility.h"
 
 /******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

 #define CANRX_get_signal(bus, signal) (JOIN3(JOIN3(CANRX_,bus,_get),_,signal))()
 #define CANRX_get_signal_duplicateNode(bus, node, id, signal) (JOIN3(JOIN3(CANRX_,bus,_get),_,JOIN4(node,id,_,signal)))()

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    CANRX_MESSAGE_SNA = 0U,
    CANRX_MESSAGE_VALID,
    CANRX_MESSAGE_CHECKSUM_INVALID,
    CANRX_MESSAGE_COUNTER_INVALID,
    CANRX_MESSAGE_MIA,
    CANRX_MESSAGE_MULTIPLE_FAILURES,
} CANRX_MESSAGE_health_E;
%for e in ctypes:

typedef struct
{
    CANRX_MESSAGE_health_E health;
    ${e} value;
} CANRX_SIGNAL_${e};
%endfor
%for d in discrete_values:

typedef struct
{
    CANRX_MESSAGE_health_E health;
    ${d} value;
} CANRX_SIGNAL_${d};
%endfor

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/
%for node in nodes:
  %for bus in node.on_buses:

static const uint16_t CANRX_${bus.upper()}_unpackList[] = { \
    %for message in node.received_msgs:
      %if bus in node.received_msgs[message].source_buses:
${node.received_msgs[message].id}U, \
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
            %if node.received_sigs[signal].native_representation.bit_width == 1:
CANRX_SIGNAL_bool CANRX_${bus.upper()}_get_${node.received_sigs[signal].name}(void);
            %else:
CANRX_SIGNAL_${node.received_sigs[signal].datatype.name} CANRX_${bus.upper()}_get_${node.received_sigs[signal].name}(void);
            %endif
          %endif
        %endfor
      %endif
    %endfor
  %endfor
%endfor
