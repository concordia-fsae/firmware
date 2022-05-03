VERSION ""

NS_ :
    BA_
	BA_DEF_
	BA_DEF_DEF_
	BA_DEF_DEF_REL_
	BA_DEF_REL_
    BA_DEF_SGTYPE_
    BA_REL_
    BA_SGTYPE_
    BO_TX_BU_
    BU_BO_REL_
    BU_EV_REL_
    BU_SG_REL_
    CAT_
    CAT_DEF_
    CM_
    ENVVAR_DATA_
    EV_DATA_
    FILTER
	NS_DESC_
    SGTYPE_
    SGTYPE_VAL_
    SG_MUL_VAL_
    SIGTYPE_VALTYPE_
    SIG_GROUP_
    SIG_TYPE_REF_
    SIG_VALTYPE_
    VAL_
    VAL_TABLE_

BS_: ${bus.baudrate}

BU_: ${" ".join([node.upper() for node in bus.nodes])}\

<%
  for message in messages:
    make_message(message)
%>

CM_ "${bus.description}";
<%
  for name, node in bus.nodes.items():
    make_comment(name.upper(), node, "BU_")
%>
<%
  for name, message in bus.messages.items():
    make_comment(message.id, message, "BO_")
%>
<%
  for name, signal in bus.signals.items():
    make_comment(f"{signal.message_ref.id} {name}", signal, "SG_")
%>
<%
  for _, message in bus.messages.items():
    make_cycle_time(message)
%>
<%
  for _, discrete_value in bus.discrete_values:
    make_val_table(discrete_value)
%>
<%
  for _, signal in bus.signals.items():
    if not signal.discrete_values is None:
      make_signal_type(signal)
%>

<%def name="make_message(message)">
BO_ ${message.id} ${message.name}: ${message.length_bytes} ${message.node_name}\
<%
  for signal in message.signal_objs.values():
    make_signal(signal)
%>
</%def>


<%def name="make_signal(signal)">
 SG_ ${signal.name} : \
${signal.start_bit}|${signal.native_representation.bit_width}@${signal.native_representation.endianness.value}${signal.native_representation.signedness.value} \
(${signal.scale},${signal.offset}) [${signal.native_representation.range.min}|${signal.native_representation.range.max}] \
"${signal.unit.value}" ${",".join([sig.name.upper() for sig in signal.receivers]) if signal.receivers else "Vector__XXX"}\
</%def>

<%def name="make_comment(name, item, item_text)">
CM_ ${item_text} ${name} "${item.description}";\
</%def>

<%def name="make_val_table(discrete_value)">
VAL_TABLE_ ${discrete_value.name} ${discrete_value.repr_for_dbc()};\
</%def>

<%def name="make_signal_type(signal)">
VAL_ ${signal.message_ref.id} ${signal.name} ${signal.discrete_values.name};\
</%def>

<%def name="make_cycle_time(message)">
BA_ "GenMsgCycleTime" BO_ ${message.id} ${message.cycle_time_ms};\
</%def>
