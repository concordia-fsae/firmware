<%def name="make_packfn(bus, msg)">
static bool pack_${bus.upper()}_${msg.name}(CAN_data_T *message, const uint8_t counter)
{
  %for signal in msg.get_non_val_sigs():
    set_${signal.get_name_nodeless()}(message, ${bus.upper()}, ${msg.node_name}, ${signal.get_name_nodeless()});
  %endfor

  %if msg.counter_sig:
    set_${bus.upper()}_${msg.node_name}_${msg.counter_sig.get_name_nodeless()}(message, counter);
  %else:
    (void)counter;
  %endif

    return true;
}
</%def>
\
<%def name="make_packtable(bus, msgs, cycle_time)">
static const packTable_S ${bus.upper()}_packTable_${cycle_time}ms [] = {
  %for msg in msgs:
    { &pack_${bus.upper()}_${msg.name}, ${hex(msg.id)}, ${msg.length_bytes}U },
  %endfor
};
</%def>
