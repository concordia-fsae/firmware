/*
 * rust_decode_generated.rs
 * Generated Rust decode dispatch and network descriptors.
 */

<%!
from collections import OrderedDict

def sanitize_ident(name):
    chars = []
    for ch in name:
        if ch.isalnum():
            chars.append(ch)
        else:
            chars.append("_")

    ident = "".join(chars).strip("_")
    if not ident:
        ident = "value"
    if ident[0].isdigit():
        ident = "_" + ident
    return ident


def rust_pascal(name):
    sanitized = sanitize_ident(name)
    parts = []
    current = ""
    for idx, ch in enumerate(sanitized):
        if ch == "_":
            if current:
                parts.append(current)
                current = ""
            continue
        if (
            current
            and ch.isupper()
            and (current[-1].islower() or (idx + 1 < len(sanitized) and sanitized[idx + 1].islower()))
        ):
            parts.append(current)
            current = ch
            continue
        current += ch

    if current:
        parts.append(current)

    if not parts:
        return "Value"
    return "".join(part[:1].upper() + part[1:] for part in parts)
%>
<%
bus_order = []
bus_messages = OrderedDict()
extern_unpacks = OrderedDict()
forward_routes = []

for node in nodes:
    for bus in node.on_buses:
        if bus not in bus_messages:
            bus_messages[bus] = []
            bus_order.append(bus)
        extern_unpacks.setdefault(bus, f"CANRX_{bus.upper()}_unpackMessage")

        seen_message_idents = {message["ident"] for message in bus_messages[bus]}
        for _, message in node.received_msgs.items():
            if bus not in message.source_buses:
                continue

            duplicate_msg = message.node_ref.duplicateNode
            msg_alias = message.node_ref.alias if duplicate_msg else None
            msg_suffix = message.name.split('_', 1)[1] if duplicate_msg else message.name
            msg_display = f"{msg_alias}_{msg_suffix}" if msg_alias else message.name
            msg_ident = f"{bus}_{msg_display}".upper()
            if msg_ident in seen_message_idents:
                continue

            bus_messages[bus].append(
                {
                    "ident": msg_ident,
                    "id": message.id,
                    "struct_ident": f"{msg_ident}_MESSAGE",
                    "any_variant": rust_pascal(f"{bus}_{msg_display}"),
                }
            )
            seen_message_idents.add(msg_ident)

    for route in getattr(node, "forwarding_routes", []):
        forwarded_messages = []
        seen_forwarded_ids = set()
        if route["policy"] == "all":
            for message in can_bus_defs[route["source_bus"]].messages.values():
                if route["source_bus"] not in message.source_buses or message.id in seen_forwarded_ids:
                    continue
                forwarded_messages.append({"name": message.name, "id": message.id})
                seen_forwarded_ids.add(message.id)
        else:
            for msg_name in route.get("bridged_messages", []):
                message = can_bus_defs[route["source_bus"]].messages[msg_name]
                if message.id in seen_forwarded_ids:
                    continue
                forwarded_messages.append({"name": message.name, "id": message.id})
                seen_forwarded_ids.add(message.id)
        forward_routes.append(
            {
                "source_bus": route["source_bus"],
                "source_variant": rust_pascal(route["source_bus"]),
                "dest_bus": route["dest_bus"],
                "dest_variant": rust_pascal(route["dest_bus"]),
                "policy": route["policy"],
                "forwarded_messages": forwarded_messages,
            }
        )
%>\
use crate::rust_model_generated::{AnyMessage, Bus};
use crate::yamcan::{
    BusDescriptor,
    BusInterfaceType,
    CanData,
    ForwardMessage,
    ForwardPolicy,
    ForwardRoute,
    MessageDecodeResult,
    MessageMetadata,
    NetworkDecoder,
    ReceivedCanMessage,
    UnhandledMessage,
};

unsafe extern "C" {
%for bus in bus_order:
    fn ${extern_unpacks[bus]}(id: u32, msg: *const CanData);
%endfor
}

%for route in forward_routes:
static ${sanitize_ident(route["source_bus"] + "_" + route["dest_bus"] + "_forwarded_messages").upper()}: &[ForwardMessage] = &[
%for message in route["forwarded_messages"]:
    ForwardMessage { id: ${message["id"]}u32, name: "${message["name"]}" },
%endfor
];
%endfor

static YAMCAN_FORWARD_ROUTES: &[ForwardRoute<Bus>] = &[
%for route in forward_routes:
    ForwardRoute {
        source_bus: Bus::${route["source_variant"]},
        dest_bus: Bus::${route["dest_variant"]},
        policy: ForwardPolicy::${"All" if route["policy"] == "all" else "Bridged"},
        forwarded_messages: ${sanitize_ident(route["source_bus"] + "_" + route["dest_bus"] + "_forwarded_messages").upper()},
    },
%endfor
];

%for bus in bus_order:
pub fn decode_${bus}_message(message: ReceivedCanMessage<Bus>) -> MessageDecodeResult<AnyMessage> {
    let data = CanData { u8: message.data };
    unsafe { ${extern_unpacks[bus]}(message.id, &data as *const CanData) };

    match message.id {
    %for message in bus_messages[bus]:
        ${message["id"]}u32 => MessageDecodeResult::Decoded(
            AnyMessage::${message["any_variant"]}(crate::rust_model_generated::${message["struct_ident"]}::from(message))
        ),
    %endfor
        _ => MessageDecodeResult::Unhandled(UnhandledMessage {
            metadata: MessageMetadata {
                bus: Bus::${rust_pascal(bus)},
                name: None,
                id: message.id,
                len: message.len,
            },
            data: message.data,
        }),
    }
}
%endfor

pub fn decode_received_message(message: ReceivedCanMessage<Bus>) -> MessageDecodeResult<AnyMessage> {
    match message.bus {
%for bus in bus_order:
        Bus::${rust_pascal(bus)} => decode_${bus}_message(message),
%endfor
    }
}

pub struct GeneratedNetwork;

impl NetworkDecoder for GeneratedNetwork {
    type Bus = Bus;
    type Message = AnyMessage;

    fn decode_received_message(message: ReceivedCanMessage<Bus>) -> MessageDecodeResult<Self::Message> {
        decode_received_message(message)
    }

    fn buses() -> &'static [BusDescriptor<Bus>] {
        YAMCAN_BUSES
    }
}

pub(crate) static YAMCAN_BUSES: &[BusDescriptor<Bus>] = &[
%for bus in bus_order:
    BusDescriptor {
        name: Bus::${rust_pascal(bus)},
        interface_type: BusInterfaceType::${"Virtual" if can_bus_defs[bus].interface_type == "virtual" else "Physical"},
        unpack: ${extern_unpacks[bus]},
    },
%endfor
];

%for bus in bus_order:
const FORWARD_ROUTES_${sanitize_ident(bus).upper()}: &[ForwardRoute<Bus>] = &[
%for route in [route for route in forward_routes if route["source_bus"] == bus]:
    ForwardRoute {
        source_bus: Bus::${route["source_variant"]},
        dest_bus: Bus::${route["dest_variant"]},
        policy: ForwardPolicy::${"All" if route["policy"] == "all" else "Bridged"},
        forwarded_messages: ${sanitize_ident(route["source_bus"] + "_" + route["dest_bus"] + "_forwarded_messages").upper()},
    },
%endfor
];
%endfor

pub fn forward_routes_from_bus(bus: Bus) -> &'static [ForwardRoute<Bus>] {
    match bus {
%for bus in bus_order:
        Bus::${rust_pascal(bus)} => FORWARD_ROUTES_${sanitize_ident(bus).upper()},
%endfor
    }
}

pub fn forward_route_for_bus(bus: Bus) -> Option< &'static ForwardRoute<Bus>> {
    forward_routes_from_bus(bus).first()
}

pub fn forward_route_for_pair(source_bus: Bus, dest_bus: Bus) -> Option< &'static ForwardRoute<Bus>> {
    YAMCAN_FORWARD_ROUTES
        .iter()
        .find(|route| route.source_bus == source_bus && route.dest_bus == dest_bus)
}
