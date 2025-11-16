<%def name="make_message(message)">
BO_ ${message.id if message.id <=0x7ff else message.id + 0x80000000} ${message.name}: ${message.length_bytes} ${message.node_name}\
<%
  for signal in message.signal_objs.values():
    make_signal(signal)
%>
</%def>

<%def name="make_signal(signal)">
  <%
  start_bit = signal.start_bit
  try:
    if signal.native_representation.endianness.value == 0:
      start_bit = 7 - (start_bit % 8) + int(start_bit / 7) * 8
  except:
    pass
%>\
 SG_ ${signal.name} : \
${start_bit}|${signal.native_representation.bit_width}@${signal.native_representation.endianness.value}${signal.native_representation.signedness.value} \
(${signal.scale},${signal.offset}) [${signal.native_representation.range.min}|${signal.native_representation.range.max}] \
"${signal.unit.value}" ${",".join([sig.alias.upper() for sig in signal.receivers]) if signal.receivers else "Vector__XXX"}\
</%def>

<%def name="make_comment(name, item, item_text)">
CM_ ${item_text} ${name} "${item.description}";\
</%def>

<%def name="make_val_table(discrete_value)">
VAL_TABLE_ ${discrete_value.name} ${discrete_value.repr_for_dbc()};\
</%def>

<%def name="make_signal_type(signal)">
VAL_ ${signal.message_ref.id} ${signal.name} ${signal.discrete_values.repr_for_dbc()};\
</%def>

<%def name="make_cycle_time(message)">
</%def>
