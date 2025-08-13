<%! import math %>
<%! from classes.Types import Signedness %>
<%def name="make_structdef_message(node, message)">\
    struct {
        uint32_t timestamp;
%if node.received_msgs[message].bridged:
        CAN_data_T raw;
        bool new_message;
%endif
    } ${node.received_msgs[message].name};
</%def>\

<%def name="make_structdef_messageDuplicates(node, message, total)">\
    struct {
        uint32_t timestamp;
    } ${node.received_msgs[message].node_ref.name.upper()}_${node.received_msgs[message].name.split('_')[1]}[${total}U];
</%def>\

<%def name="make_structdef_signal(node, sig)">\
  %if node.received_sigs[sig].discrete_values:
    CAN_${node.received_sigs[sig].discrete_values.name}_E ${node.received_sigs[sig].name}; // [${node.received_sigs[sig].unit.name}] ${node.received_sigs[sig].description}
%elif node.received_sigs[sig].native_representation.bit_width == 1:
    bool ${node.received_sigs[sig].name} :1; // [${node.received_sigs[sig].unit.name}] ${node.received_sigs[sig].description};
%else:
    ${node.received_sigs[sig].datatype.name} ${node.received_sigs[sig].name}; // [${node.received_sigs[sig].unit.name}] ${node.received_sigs[sig].description}
%endif
</%def>\
<%def name="make_structdef_signalDuplicates(node, sig, total)">\
  %if node.received_sigs[sig].discrete_values:
    CAN_${node.received_sigs[sig].discrete_values.name}_E ${node.received_sigs[sig].message_ref.node_ref.name.upper()}_${sig.split('_')[1]}[${total}U]; // [${node.received_sigs[sig].unit.name}] ${node.received_sigs[sig].description}
%elif node.received_sigs[sig].native_representation.bit_width == 1:
    bool ${node.received_sigs[sig].message_ref.node_ref.name.upper()}_${sig.split('_')[1]}[${total}U]; // [${node.received_sigs[sig].unit.name}] ${node.received_sigs[sig].description}
%else:
    ${node.received_sigs[sig].datatype.name} ${node.received_sigs[sig].message_ref.node_ref.name.upper()}_${sig.split('_')[1]}[${total}U]; // [${node.received_sigs[sig].unit.name}] ${node.received_sigs[sig].description}
%endif
</%def>\

<%def name="make_sigunpack(bus, node, signal, is_duplicate :bool)">\
<%
      if is_duplicate:
        sig_name = signal.message_ref.node_ref.name.upper() + '_' + signal.name.split('_')[1] + '[nodeId]'
      else:
        sig_name = signal.name

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
    sigrx->${sig_name} = (m->u8[${int(signal.start_bit / 8)}U] & (1U << ${signal.start_bit % 8}U)) != 0U;
%elif "float" in signal.datatype.value or "int" in signal.datatype.value: # Handles both int and uint
    %if signal.native_representation.signedness == Signedness.unsigned:
        %if dtype == 64:
    uint64_t tmp_${signal.name} = (m->u64 >> ${signal.start_bit}U) & ${(2**signal.native_representation.bit_width) - 1}U;
    %else:
    uint${math.ceil(signal.native_representation.bit_width / 8) * 8}_t tmp_${signal.name} = (m->u${dtype}[${idx_s}U] >> ${signal.start_bit % dtype}U) & ${(2**signal.native_representation.bit_width) - 1}U;
        %endif
    %else:
        %if dtype == 64:
    int64_t tmp_${signal.name} = (m->u64 >> ${signal.start_bit}U) & ${(2**signal.native_representation.bit_width) - 1}U;
        %else:
    int${math.ceil(signal.native_representation.bit_width / 8) * 8}_t tmp_${signal.name} = (m->u${dtype}[${idx_s}U] >> ${signal.start_bit % dtype}U) & ${(2**signal.native_representation.bit_width) - 1}U;
        %endif
    %endif
    %if signal.native_representation.endianness.value == 0:
    tmp_${signal.name} = ((tmp_${signal.name} & (~${(2**(signal.native_representation.bit_width % 8) - 1)}U & ${(2**signal.native_representation.bit_width) - 1}U)) << ${(8 - signal.native_representation.bit_width % 8) % 8}U) | (tmp_${signal.name} & ${2**(signal.native_representation.bit_width % 8) - 1}U);
    reverse_bytes((uint8_t*)&tmp_${signal.name}, ${math.ceil(signal.native_representation.bit_width / 8)}U);
    %endif
    %if signal.native_representation.signedness == Signedness.signed:
    tmp_${signal.name} = (tmp_${signal.name} & ${(2**(signal.native_representation.bit_width - 1) - 1)}U) * (((tmp_${signal.name} & (1U << ${(signal.native_representation.bit_width - 1)})) != 0U) ? -1.0f : 1.0f);
    %endif
    %if "float" in signal.datatype.value:
    sigrx->${sig_name} = (${signal.datatype.value})(tmp_${signal.name}) * ${float(signal.scale)}f + (${float(signal.offset)}f);
    %elif "int" in signal.datatype.value:
    sigrx->${sig_name} = (tmp_${signal.name} * ${int(signal.scale)}) + (${int(signal.offset)});
    %endif
%else:
    (void)m;
    (void)val;
%endif
</%def>\
