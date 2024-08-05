<%def name="make_packfn(bus, node, msg)">
static bool pack_${bus.upper()}_${msg.name}(CAN_data_T *message, const uint8_t counter)
{
  %for signal in msg.get_non_val_sigs():
    set_${signal.get_name_nodeless()}(message, ${bus.upper()}, ${node}, ${signal.get_name_nodeless()});
  %endfor

  %if msg.counter_sig:
    set_${bus.upper()}_${msg.node_name}_${msg.counter_sig.get_name_nodeless()}(message, counter);
  %else:
    (void)counter;
  %endif

    return true;
}
</%def>

<%def name="make_packtable(bus, msgs, cycle_time)">
static const packTable_S ${bus.upper()}_packTable_${cycle_time}ms [] = {
  %for msg in msgs:
    { &pack_${bus.upper()}_${msg.name}, ${hex(msg.id)}, ${msg.length_bytes}U },
  %endfor
};
</%def>

<%def name="make_sigpack(bus, node, signal)">
__attribute__((always_inline)) static inline void set_${bus.upper()}_${node.upper()}_${signal.get_name_nodeless()}(CAN_data_T* m, ${signal.datatype.value} val)
{
%if signal.native_representation.bit_width == 1:
<%
    idx = signal.start_bit // 8
  %>\
    atomicXorU8(&m->u8[${idx}], m->u8[${idx}] ^ ((val ? 1U : 0U) << ${signal.start_bit % 8}U));
  %else:
    %if "float" in signal.datatype.value:
      %if "32" in signal.datatype.value:
          uint32_t 
      %else:
      %endif
    %endif
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
        s_idx = 1
    %>\
\
    %if dtype == 64:
    atomicXorU64(&m->u64, (val & ${(2**signal.native_representation.bit_width) - 1}) << ${signal.start_bit % dtype}U);
    %else:
    atomicXorU${dtype}(&m->u${dtype}[${idx_s}], (val & ${(2**signal.native_representation.bit_width) - 1}) << ${signal.start_bit % dtype}U);
    %endif
  %else:
    (void)m;
    (void)val;
%endif
}
</%def>
