<%namespace file="dbc_functions.mako" import = "*"/>
<%include file="dbc_header.dbc"/>
BS_:

BU_: ${" ".join([node.upper() for node in bus.nodes])}\

<%
  for _, discrete_value in bus.DISC:
    make_val_table(discrete_value)
%>
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
  for _, signal in bus.signals.items():
    if not signal.discrete_values is None:
      make_signal_type(signal)
%>

