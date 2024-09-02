<%! from classes.Types import DiscreteValue, CType %>
<%! from classes.Can import DiscreteValues %>
/*
 * CANTypes_generated.h
 * Defines all DiscreteValue CAN Types on the network
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/
 
#include "stdbool.h"
#include "stdint.h"
#include "FloatTypes.h"
 
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
%for name, dv in DiscreteValues():

typedef enum
{
  %for d in dv.values:
    CAN_${name.upper()}_${d.upper()} = ${dv.values[d]}U,
  %endfor
} CAN_${name}_E;
%endfor
<%
  e = [e.value for e in CType]
%>\
%for t in e:

typedef struct
{
    CANRX_MESSAGE_health_E health;
    ${t} value;
} CANRX_SIGNAL_${t};
%endfor
<%
  discrete_values = DiscreteValues()
%>\
%for d in discrete_values:

typedef struct
{
    CANRX_MESSAGE_health_E health;
    CAN_${d[0]}_E value;
} CANRX_SIGNAL_${d[0]};
%endfor
