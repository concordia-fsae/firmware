<%! import math %>
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
%if signal.native_representation.bit_width == 1:
<%
    idx = signal.start_bit // 8
%>\
    atomicXorU8(&m->u8[${idx}], m->u8[${idx}] ^ ((val ? 1U : 0U) << ${signal.start_bit % 8}U));
%elif "float" in signal.datatype.value:
\
<%
      sb = signal.start_bit
      eb = signal.start_bit + (signal.native_representation.bit_width - 1)

      # determine how to access the relevant parts of the message for stuffing this signal
      idx_s = sb // 8
      idx_e = eb // 8
      span = idx_e - idx_s
      dtype = 8 + (8 * span)
      dtype = 32 if dtype == 24 else 64 if dtype > 32 else dtype

      # handle critical boundaries, overriding the above
      if sb < 32:
        if eb > 32:
          dtype = 64
        elif sb < 16 and eb > 16:
          dtype = 32
          idx_s = 0
      elif sb > 32 and eb > 48:
        dtype = 32
        idx_s = 1
%>\
\
    %if dtype == 64:
      %if signal.native_representation.endianness.value == 1:
    atomicXorU64(&m->u64, ((uint64_t)((val - ${float(signal.offset)}f) / ${float(signal.scale)}f) & ${(2**signal.native_representation.bit_width) - 1}) << ${signal.start_bit}U);
      %else:
    uint64_t tmp_${signal.name} = (uint64_t)((val - ${float(signal.offset)}f) / ${float(signal.scale)}f);
    reverse_bytes((uint8_t*)&tmp_${signal.name}, ${math.ceil(signal.native_representation.bit_width / 8)}U);
    tmp_${signal.name} = ((tmp_${signal.name} >> ${(8 - (signal.native_representation.bit_width % 8)) % 8}U) | (tmp_${signal.name} & ${2**(signal.native_representation.bit_width % 8) - 1}U));
    atomicXorU64(&m->u64, (tmp_${signal.name} & ${(2**signal.native_representation.bit_width) - 1}) << ${signal.start_bit % dtype}U);
      %endif
    %else:
      %if signal.native_representation.endianness.value == 1:
    atomicXorU${dtype}(&m->u${dtype}[${idx_s}], ((uint${dtype}_t)((val - ${float(signal.offset)}f) / ${float(signal.scale)}f) & ${(2**signal.native_representation.bit_width) - 1}) << ${signal.start_bit % dtype}U);
    %else:
    uint${dtype}_t tmp_${signal.name} = (uint${dtype}_t)((val - ${float(signal.offset)}f) / ${float(signal.scale)}f);
    reverse_bytes((uint8_t*)&tmp_${signal.name}, ${math.ceil(signal.native_representation.bit_width / 8)}U);
    tmp_${signal.name} = ((tmp_${signal.name} >> ${(8 - (signal.native_representation.bit_width % 8)) % 8}U) | (tmp_${signal.name} & ${2**(signal.native_representation.bit_width % 8) - 1}U));
    atomicXorU${dtype}(&m->u${dtype}[${idx_s}], (tmp_${signal.name} & ${(2**signal.native_representation.bit_width) - 1}) << ${signal.start_bit % dtype}U);
      %endif
    %endif
  %elif "uint" in signal.datatype.value:
      %if signal.native_representation.bit_width > 32 or (signal.start_bit < 32 and (signal.start_bit +signal.native_representation.bit_width - 1) >= 32):
        %if signal.native_representation.endianness.value == 1:
    atomicXorU64(&m->u64, ((uint64_t)((val - ${float(signal.offset)}f) / ${float(signal.scale)}f) & ${(2**signal.native_representation.bit_width - 1)}) << ${signal.start_bit}U);
        %else:
    uint64_t tmp_${signal.name} = (uint64_t)((val - ${int(signal.offset)}U) / ${int(signal.scale)}U);
    reverse_bytes((uint8_t*)&tmp_${signal.name}, ${math.ceil(signal.native_representation.bit_width / 8)}U);
    tmp_${signal.name} = ((tmp_${signal.name} >> ${(8 - (signal.native_representation.bit_width % 8)) % 8}U) | (tmp_${signal.name} & ${2**(signal.native_representation.bit_width % 8) - 1}U));
    atomicXorU64(&m->u64, (tmp_${signal.name} & ${(2**signal.native_representation.bit_width) - 1}) << ${signal.start_bit}U);
        %endif
      %else:
<%
      startBit = signal.start_bit
      startIndex = 0

      if startBit >= 32:
        startBit = startBit - 32
        startIndex = 1
%>\
        %if signal.native_representation.endianness.value == 1:
    atomicXorU32(&m->u32[${startIndex}], m->u32[${startIndex}] ^ (((uint32_t)val & ${(2**signal.native_representation.bit_width - 1)}) << ${startBit}U));
        %else:
    uint32_t tmp_${signal.name} = (uint32_t)((val - ${int(signal.offset)}U) / ${int(signal.scale)}U);
    reverse_bytes((uint8_t*)&tmp_${signal.name}, ${math.ceil(signal.native_representation.bit_width / 8)}U);
    tmp_${signal.name} = ((tmp_${signal.name} >> ${(8 - (signal.native_representation.bit_width % 8)) % 8}U) | (tmp_${signal.name} & ${2**(signal.native_representation.bit_width % 8) - 1}U));
    atomicXorU32(&m->u32[${startIndex}], (tmp_${signal.name} & ${(2**signal.native_representation.bit_width) - 1}) << ${start_bit}U);
        %endif
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
