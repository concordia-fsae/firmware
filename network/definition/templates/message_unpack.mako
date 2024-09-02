<%! import math %>
<%def name="make_structdef_message(node, message)">\
    struct {
        uint32_t timestamp;
    %if node.received_msgs[message].checksum_sig is not None:
        bool checksumValid :1;
    %endif
    %if node.received_msgs[message].counter_sig is not None:
        bool counterValid :1;
    %endif
    } ${node.received_msgs[message].name};
</%def>\

<%def name="make_structdef_signal(node, sig)">\
%if node.received_sigs[sig].discrete_values:
    CAN_${node.received_sigs[sig].discrete_values.name}_E ${node.received_sigs[sig].name}; // [${node.received_sigs[sig].unit.name}] ${node.received_sigs[sig].description}
%elif node.received_sigs[sig].native_representation.bit_width == 1:
    bool ${node.received_sigs[sig].name} :1; // [${node.received_sigs[sig].unit.name}] ${node.received_sigs[sig].description} 
%else:
    ${node.received_sigs[sig].datatype.name} ${node.received_sigs[sig].name}; // [${node.received_sigs[sig].unit.name}] ${node.received_sigs[sig].description} 
%endif
</%def>\

<%def name="make_sigunpack(bus, node, signal)">\
%if signal.native_representation.bit_width == 1:
<%
    idx = signal.start_bit // 8
%>\
    sigrx->${signal.name} = (m->u8[${idx}] & (1U << ${signal.start_bit % 8}U)) != 0U;
%elif "float" in signal.datatype.value:
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
    %if dtype == 64:
      %if signal.native_representation.endianness.value == 1:
    sigrx->${signal.name} = (float64_t)((m->u64 >> ${signal.start_bit}U) & ${(2**signal.native_representation.bit_width) - 1}) * ${float(signal.scale)}f + (${float(signal.offset)}f);
      %else:
    uint64_t tmp_${signal.name} = (m->u64 >> ${signal.start_bit}U) & ${(2**signal.native_representation.bit_width) - 1}U;
    tmp_${signal.name} = ((tmp_${signal.name} & (~${(2**(signal.native_representation.bit_width % 8) - 1)}U & ${(2**signal.native_representation.bit_width) - 1}U)) << ${(8 - signal.native_representation.bit_width % 8) % 8}U) | (tmp_${signal.name} & ${2**(signal.native_representation.bit_width % 8) - 1}U);
    reverse_bytes((uint8_t*)&tmp_${signal.name}, ${math.ceil(signal.native_representation.bit_width / 8)});
    sigrx->${signal.name} = (float64_t)(tmp_${signal.name}) * ${float(signal.scale)}f + (${float(signal.offset)}f);
      %endif
    %else:
      %if signal.native_representation.endianness.value == 1:
    sigrx->${signal.name} = (float32_t)((m->u${dtype}[${(signal.start_bit // dtype)}] >> ${signal.start_bit % dtype}U) & ${(2**signal.native_representation.bit_width) - 1}U) * ${float(signal.scale)}f + (${float(signal.offset)}f);
      %else:
    uint32_t tmp_${signal.name} = (m->u${dtype}[${(signal.start_bit // dtype)}] >> ${signal.start_bit % dtype}U) & ${(2**signal.native_representation.bit_width) - 1}U;
    tmp_${signal.name} = ((tmp_${signal.name} & (~${(2**(signal.native_representation.bit_width % 8) - 1)}U & ${(2**signal.native_representation.bit_width) - 1}U)) << ${(8 - signal.native_representation.bit_width % 8) % 8}U) | (tmp_${signal.name} & ${2**(signal.native_representation.bit_width % 8) - 1}U);
    reverse_bytes((uint8_t*)&tmp_${signal.name}, ${math.ceil(signal.native_representation.bit_width / 8)});
    sigrx->${signal.name} = (float32_t)(tmp_${signal.name}) * ${float(signal.scale)}f + (${float(signal.offset)}f);
      %endif
    %endif
%elif "uint" in signal.datatype.value:
      %if signal.native_representation.bit_width > 32 or (signal.start_bit < 32 and (signal.start_bit +signal.native_representation.bit_width - 1) >= 32):
        %if signal.native_representation.endianness.value == 1:
    sigrx->${signal.name} = ((m->u64 >> ${(signal.start_bit)}U) & ${(2**signal.native_representation.bit_width) - 1}U) * (${int(signal.scale)}) + (${int(signal.offset)});
        %else:
    uint64_t tmp_${signal.name} = (m->u64 >> ${signal.start_bit}U) & ${(2**signal.native_representation.bit_width) - 1}U;
    tmp_${signal.name} = ((tmp_${signal.name} & (~${(2**(signal.native_representation.bit_width % 8) - 1)}U & ${(2**signal.native_representation.bit_width) - 1}U)) << ${(8 - signal.native_representation.bit_width % 8) % 8}U) | (tmp_${signal.name} & ${2**(signal.native_representation.bit_width % 8) - 1}U);
    reverse_bytes((uint8_t*)&tmp_${signal.name}, ${math.ceil(signal.native_representation.bit_width // 8)});
    sigrx->${signal.name} = (tmp_${signal.name}) * (${int(signal.scale)}) + (${int(signal.offset)});
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
    sigrx->${signal.name} = ((m->u32[${startIndex}] >> ${startBit}U) & ${(2**signal.native_representation.bit_width) - 1}U) * (${int(signal.scale)}) + (${int(signal.offset)});
        %else:
    uint32_t tmp_${signal.name} = (m->u32[${(signal.start_bit // 32)}] >> ${signal.start_bit % 32}U) & ${(2**signal.native_representation.bit_width) - 1}U;
    tmp_${signal.name} = ((tmp_${signal.name} & (~${(2**(signal.native_representation.bit_width % 8) - 1)}U & ${(2**signal.native_representation.bit_width) - 1}U)) << ${(8 - signal.native_representation.bit_width % 8) % 8}U) | (tmp_${signal.name} & ${2**(signal.native_representation.bit_width % 8) - 1}U);
    reverse_bytes((uint8_t*)&tmp_${signal.name}, ${math.ceil(signal.native_representation.bit_width / 8)});
    sigrx->${signal.name} = (tmp_${signal.name}) * (${int(signal.scale)}) + (${int(signal.offset)});
        %endif
      %endif
%else:
    (void)m;
    (void)val;
%endif
</%def>\
