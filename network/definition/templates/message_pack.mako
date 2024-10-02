<%! import math %>
<%! from classes.Types import Signedness %>
<%def name="make_packfn(bus, msg)">
static bool pack_${bus.upper()}_${msg.name}(CAN_data_T *message, const uint8_t counter)
{
    if (transmit_${msg.name} == false)
    {
        return false;
    }

  %for signal in msg.get_non_val_sigs():
    set_${signal.get_name_nodeless()}(message, ${bus.upper()}, ${msg.node_name}, ${signal.get_name_nodeless()});
  %endfor

  %if msg.counter_sig:
    message->u64 |= counter;
  %else:
    (void)counter;
  %endif

    return true;
}
</%def>

<%def name="make_packtable(bus, msgs, cycle_time)">
const packTable_S ${bus.upper()}_packTable_${cycle_time}ms [] = {
  %for msg in msgs:
    { &pack_${bus.upper()}_${msg.name}, ${hex(msg.id)}, ${msg.length_bytes}U },
  %endfor
};
</%def>

<%def name="make_sigpack(bus, node, signal)">
%if signal.discrete_values:
__attribute__((always_inline)) static inline void set_${bus.upper()}_${node.upper()}_${signal.get_name_nodeless()}(CAN_data_T* m, CAN_${signal.discrete_values.name}_E val)
{
%else:
__attribute__((always_inline)) static inline void set_${bus.upper()}_${node.upper()}_${signal.get_name_nodeless()}(CAN_data_T* m, ${signal.datatype.value} val)
{
%endif
<%
      sb = signal.start_bit
      eb = signal.start_bit + (signal.native_representation.bit_width - 1)

      # determine how to access the relevant parts of the message for stuffing this signal
      idx_s = sb // 8
      idx_e = eb // 8
      span = idx_e - idx_s
      dtype = 8 + (8 * span)
      dtype = 32 if dtype == 24 else 64 if dtype > 32 else dtype
      float_type = 32 if dtype <= 32 else 64

      # handle critical boundaries, overriding the above
      if sb < 32 and eb >= 32:
        dtype = 64
      elif sb < 16 and eb >= 16:
        dtype = 32
        idx_s = 0
      elif sb < 48 and eb >= 48:
        dtype = 32
        idx_s = 1
      elif sb < 8 and eb >= 8:
        dtype = 16
        idx_s = 0
      elif sb < 16 and eb >= 16:
        dtype = 16
        idx_s = 1
      elif sb < 40 and eb >= 40:
        dtype = 16
        idx_s = 2
      elif sb < 56 and eb >= 56:
        dtype = 16
        idx_s = 3
      elif dtype == 16:
        idx_s = int(idx_s/2)
%>\
%if signal.native_representation.bit_width == 1:
    atomicXorU8(&m->u8[${int(signal.start_bit / 8)}U], (uint${dtype}_t)(m->u8[${int(signal.start_bit / 8)}U] ^ ((val ? 1U : 0U) << ${signal.start_bit % 8}U)));
%elif "float" in signal.datatype.value or "int" in signal.datatype.value: # Handles both int and uint
    %if signal.native_representation.signedness == Signedness.unsigned:
    uint${dtype}_t tmp_${signal.name};
    %else:
    int${dtype}_t tmp_${signal.name};
    %endif
    %if "float" in signal.datatype.value:
    tmp_${signal.name} = (${'u' if signal.native_representation.signedness == Signedness.unsigned else ''}int${dtype}_t)((val - ${float(signal.offset)}f) / ${float(signal.scale)}f);
    %elif "int" in signal.datatype.value:
    tmp_${signal.name} = (${'u' if signal.native_representation.signedness == Signedness.unsigned else ''}int${dtype}_t)((val - ${int(signal.offset)}) / ${int(signal.scale)});
    %endif
    %if signal.native_representation.signedness == Signedness.signed:
    tmp_${signal.name} = (tmp_${signal.name} & ${2**(signal.native_representation.bit_width - 1) - 1}U) | ((tmp_${signal.name} < 0) ? 1 << ${(signal.native_representation.bit_width - 1)}U : 0U);
    %endif
    %if signal.native_representation.endianness.value == 0 and signal.native_representation.bit_width > 8:
    reverse_bytes((uint8_t*)&tmp_${signal.name}, ${int(signal.native_representation.bit_width / 8)}U);
    tmp_${signal.name} = ((tmp_${signal.name} & ~(255U)) >> ${(8 - signal.native_representation.bit_width % 8) % 8}U) | (tmp_${signal.name} & ${(2**(8 - (signal.native_representation.bit_width % 8))) - 1}U);
    %endif
    %if dtype == 64:
    atomicXorU64(&m->u64, (uint64_t)(tmp_${signal.name} & ${(2**signal.native_representation.bit_width) - 1}U) << ${signal.start_bit}U);
    %else: 
    atomicXorU${dtype}(&m->u${dtype}[${idx_s}], (uint${dtype}_t)(tmp_${signal.name} & ${(2**signal.native_representation.bit_width) - 1}U) << ${signal.start_bit % dtype}U);
    %endif 
%else:
    (void)m;
    (void)val;
%endif
}
</%def>
\
<%def name="make_transmitstub(message)">
#ifndef transmit_${message.name}
#define transmit_${message.name} true
#endif
</%def>
\
<%def name="make_sigstub(signal)">
#ifndef set_${signal.get_name_nodeless()}
#define set_${signal.get_name_nodeless()}(m,b,n,s) set(m,b,n,s, 0U); unsent_signal(m)
#endif
</%def>
\
