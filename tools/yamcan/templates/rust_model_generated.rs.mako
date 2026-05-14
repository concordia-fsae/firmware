/*
 * rust_model_generated.rs
 * Generated Rust message and signal descriptors.
 */

<%!
from collections import OrderedDict

TYPE_MAP = {
    "float32_t": "f32",
    "float64_t": "f64",
    "uint8_t": "u8",
    "uint16_t": "u16",
    "uint32_t": "u32",
    "uint64_t": "u64",
    "int8_t": "i8",
    "int16_t": "i16",
    "int32_t": "i32",
    "int64_t": "i64",
    "bool": "bool",
}


def rust_type_and_convert(signal):
    if signal.discrete_values:
        return ("c_int", "val as f64")
    if signal.native_representation.bit_width == 1:
        return ("bool", "if val { 1.0 } else { 0.0 }")

    rust_type = TYPE_MAP.get(signal.datatype.name, "f64")
    if rust_type == "bool":
        return (rust_type, "if val { 1.0 } else { 0.0 }")
    return (rust_type, "val as f64")


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
extern_getters = OrderedDict()
base_wrappers = OrderedDict()
instance_wrappers = OrderedDict()
base_value_wrappers = OrderedDict()
instance_value_wrappers = OrderedDict()
discrete_enums = OrderedDict()

for node in nodes:
    for bus in node.on_buses:
        if bus not in bus_messages:
            bus_messages[bus] = []
            bus_order.append(bus)
        extern_unpacks.setdefault(bus, f"CANRX_{bus.upper()}_unpackMessage")

        seen_message_idents = {message["ident"] for message in bus_messages[bus]}
        for message_name, message in node.received_msgs.items():
            if bus not in message.source_buses:
                continue

            duplicate_msg = message.node_ref.duplicateNode
            msg_alias = message.node_ref.alias if duplicate_msg else None
            msg_suffix = message.name.split('_', 1)[1] if duplicate_msg else message.name
            msg_display = f"{msg_alias}_{msg_suffix}" if msg_alias else message.name
            msg_ident = f"{bus}_{msg_display}".upper()
            if msg_ident in seen_message_idents:
                continue

            signal_entries = []
            for signal_name in message.signals:
                if signal_name not in node.received_sigs:
                    continue

                signal = node.received_sigs[signal_name]
                sig_suffix = signal_name.split('_', 1)[1]
                getter_suffix = signal.name if not duplicate_msg else f"{message.node_ref.name.upper()}_{sig_suffix}"
                rust_type, convert = rust_type_and_convert(signal)
                enum_lookup_name = None

                if signal.discrete_values:
                    enum_lookup_name = f"yamcan_enum_{signal.discrete_values.name}".lower()
                    enum_type = rust_pascal(signal.discrete_values.name)
                    discrete_enums.setdefault(
                        signal.discrete_values.name,
                        {
                            "lookup_name": enum_lookup_name,
                            "type_name": enum_type,
                            "values": list(signal.discrete_values.values.items()),
                        },
                    )
                else:
                    enum_type = None

                getter_symbol = f"CANRX_{bus.upper()}_get_{getter_suffix}"
                getter_key = (getter_symbol, rust_type, duplicate_msg)
                extern_getters.setdefault(
                    getter_key,
                    {
                        "symbol": getter_symbol,
                        "rust_type": rust_type,
                        "duplicate": duplicate_msg,
                    },
                )

                base_wrapper_name = f"yamcan_get_{bus}_{getter_suffix}".lower()
                base_wrappers.setdefault(
                    base_wrapper_name,
                    {
                        "name": base_wrapper_name,
                        "getter_symbol": getter_symbol,
                        "rust_type": rust_type,
                        "convert": convert,
                        "duplicate": duplicate_msg,
                    },
                )

                value_wrapper_name = f"yamcan_value_{bus}_{getter_suffix}".lower()
                field_type = f"Option<{enum_type}>" if enum_type else rust_type
                base_value_wrappers.setdefault(
                    value_wrapper_name,
                    {
                        "name": value_wrapper_name,
                        "getter_symbol": getter_symbol,
                        "rust_type": rust_type,
                        "field_type": field_type,
                        "duplicate": duplicate_msg,
                        "enum_type": enum_type,
                    },
                )

                if duplicate_msg:
                    node_id = message.node_ref.offset
                    getter_name = f"{base_wrapper_name}_{node_id}"
                    value_getter_name = f"{value_wrapper_name}_{node_id}"
                    instance_wrappers.setdefault(
                        getter_name,
                        {
                            "name": getter_name,
                            "base_name": base_wrapper_name,
                            "node_id": node_id,
                        },
                    )
                    instance_value_wrappers.setdefault(
                        value_getter_name,
                        {
                            "name": value_getter_name,
                            "base_name": value_wrapper_name,
                            "node_id": node_id,
                            "field_type": field_type,
                        },
                    )
                    sig_display = f"{message.node_ref.alias}_{sig_suffix}"
                    method_source = sig_suffix
                else:
                    getter_name = base_wrapper_name
                    value_getter_name = value_wrapper_name
                    sig_display = signal.name
                    method_source = signal.name

                method_name = sanitize_ident(method_source)
                signal_struct_ident = f"{msg_ident}_{sanitize_ident(sig_display).upper()}_SIGNAL"
                signal_static_ident = f"{signal_struct_ident}_INSTANCE"
                if enum_type:
                    signal_kind = "SignalKind::Enum"
                elif field_type == "bool":
                    signal_kind = "SignalKind::Boolean"
                else:
                    signal_kind = "SignalKind::Numeric"

                signal_entries.append(
                    {
                        "name": sig_display,
                        "unit": signal.unit.name if signal.unit else "",
                        "getter_name": getter_name,
                        "method_name": method_name,
                        "struct_ident": signal_struct_ident,
                        "static_ident": signal_static_ident,
                        "enum_lookup_name": enum_lookup_name,
                        "enum_type": enum_type,
                        "value_getter_name": value_getter_name,
                        "field_type": field_type,
                        "kind": signal_kind,
                    }
                )

            message_struct_ident = f"{msg_ident}_MESSAGE"
            message_static_ident = f"{msg_ident}_INSTANCE"
            collect_fn_ident = f"yamcan_collect_{bus}_{sanitize_ident(msg_display)}".lower()
            any_variant = rust_pascal(f"{bus}_{msg_display}")
            bus_messages[bus].append(
                {
                    "ident": msg_ident,
                    "name": msg_display,
                    "id": message.id,
                    "len": message.length_bytes,
                    "signals": signal_entries,
                    "struct_ident": message_struct_ident,
                    "static_ident": message_static_ident,
                    "collect_fn_ident": collect_fn_ident,
                    "any_variant": any_variant,
                }
            )
            seen_message_idents.add(msg_ident)
%>\
use core::ffi::c_int;

use crate::SignalMeasurement;
use crate::yamcan::{
    CanData,
    DecodedCanMessage,
    MessageDescriptor,
    MessageMetadata,
    NetworkBus,
    ReceivedCanMessage,
    SignalAccessor,
    SignalDescriptor,
    SignalKind,
};

unsafe extern "C" {
%for bus in bus_order:
    fn ${extern_unpacks[bus]}(id: u32, msg: *const CanData);
%endfor
%for getter in extern_getters.values():
    fn ${getter["symbol"]}(val: *mut ${getter["rust_type"]}${', node_id: u8' if getter["duplicate"] else ''}) -> c_int;
%endfor
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum Bus {
%for bus in bus_order:
    ${rust_pascal(bus)},
%endfor
}

impl NetworkBus for Bus {
    fn as_str(self) -> &'static str {
        match self {
%for bus in bus_order:
            Self::${rust_pascal(bus)} => "${bus}",
%endfor
        }
    }
}

%for enum_name, enum_data in discrete_enums.items():
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum ${enum_data["type_name"]} {
    %for label, raw in enum_data["values"]:
    ${rust_pascal(label)} = ${raw},
    %endfor
}

impl ${enum_data["type_name"]} {
    pub fn from_raw(value: i32) -> Option<Self> {
        match value {
        %for label, raw in enum_data["values"]:
            ${raw} => Some(Self::${rust_pascal(label)}),
        %endfor
            _ => None,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
        %for label, raw in enum_data["values"]:
            Self::${rust_pascal(label)} => "${label}",
        %endfor
        }
    }

    pub fn as_raw(self) -> i32 {
        self as i32
    }
}

impl core::convert::TryFrom<i32> for ${enum_data["type_name"]} {
    type Error = i32;

    fn try_from(value: i32) -> Result<Self, Self::Error> {
        Self::from_raw(value).ok_or(value)
    }
}

impl From<${enum_data["type_name"]}> for i32 {
    fn from(value: ${enum_data["type_name"]}) -> Self {
        value.as_raw()
    }
}

fn ${enum_data["lookup_name"]}(value: i32) -> Option${"<"}&'static str> {
    ${enum_data["type_name"]}::from_raw(value).map(${enum_data["type_name"]}::as_str)
}
%endfor

%for wrapper in base_wrappers.values():
fn ${wrapper["name"]}(out: &mut f64${', node_id: u8' if wrapper["duplicate"] else ''}) -> u8 {
    let mut val: ${wrapper["rust_type"]} = Default::default();
    let health = unsafe {
        ${wrapper["getter_symbol"]}(&mut val as *mut ${wrapper["rust_type"]}${', node_id' if wrapper["duplicate"] else ''})
    };
    *out = ${wrapper["convert"]};
    health as u8
}
%endfor

%for wrapper in instance_wrappers.values():
fn ${wrapper["name"]}(out: &mut f64) -> u8 {
    ${wrapper["base_name"]}(out, ${wrapper["node_id"]}u8)
}
%endfor

%for wrapper in base_value_wrappers.values():
fn ${wrapper["name"]}(${ 'node_id: u8' if wrapper["duplicate"] else ''}) -> ${wrapper["field_type"]} {
    let mut val: ${wrapper["rust_type"]} = Default::default();
    let _health = unsafe {
        ${wrapper["getter_symbol"]}(&mut val as *mut ${wrapper["rust_type"]}${', node_id' if wrapper["duplicate"] else ''})
    };
    %if wrapper["enum_type"]:
    ${wrapper["enum_type"]}::from_raw(val)
    %else:
    val
    %endif
}
%endfor

%for wrapper in instance_value_wrappers.values():
fn ${wrapper["name"]}() -> ${wrapper["field_type"]} {
    ${wrapper["base_name"]}(${wrapper["node_id"]}u8)
}
%endfor

%for bus in bus_order:
  %for message in bus_messages[bus]:
    %for signal in message["signals"]:
pub struct ${signal["struct_ident"]};

impl ${signal["struct_ident"]} {
    const ACCESSOR: SignalAccessor = SignalAccessor {
        name: "${signal["name"]}",
        unit: ${'None' if signal["unit"] == '' else 'Some("' + signal["unit"] + '")'},
        get: ${signal["getter_name"]},
        enum_name: ${'Some(' + signal["enum_lookup_name"] + ')' if signal["enum_lookup_name"] else 'None'},
    };

    fn read(&self) -> SignalMeasurement {
        Self::ACCESSOR.read()
    }

    pub fn measurement(&self) -> SignalMeasurement {
        self.read()
    }

    pub fn measurement_from(&self, value: ${signal["field_type"]}) -> SignalMeasurement {
        SignalMeasurement {
            name: Self::ACCESSOR.name.to_string(),
            value: ${f'value.map(|v| i32::from(v) as f64).unwrap_or(f64::NAN)' if signal["enum_type"] else 'value as f64'},
            unit: Self::ACCESSOR.unit.map(str::to_string),
            label: ${f'value.map(|v| v.as_str().to_string())' if signal["enum_type"] else 'None'},
        }
    }
    %if signal["enum_type"]:

    pub fn value(&self) -> Option<${signal["enum_type"]}> {
        ${signal["enum_type"]}::from_raw(self.read().value as i32)
    }
    %else:

    pub fn value(&self) -> ${signal["field_type"]} {
        %if signal["field_type"] == "bool":
        self.read().value != 0.0
        %else:
        self.read().value as ${signal["field_type"]}
        %endif
    }
    %endif
}

pub static ${signal["static_ident"]}: ${signal["struct_ident"]} = ${signal["struct_ident"]};
    %endfor

#[derive(Clone, Debug)]
pub struct ${message["struct_ident"]} {
    pub metadata: MessageMetadata<Bus>,
    %for signal in message["signals"]:
    pub ${signal["method_name"]}: ${signal["field_type"]},
    %endfor
}

impl ${message["struct_ident"]} {
    const NAME: &'static str = "${message["name"]}";
    const ID: u32 = ${message["id"]}u32;
    const LEN: u8 = ${message["len"]}u8;

    pub fn measurements(
        &self,
        sig_filters: &[String],
        allow_empty: bool,
    ) -> Option<Vec<SignalMeasurement>> {
        let mut out: Vec<SignalMeasurement> = Vec::new();
        %for signal in message["signals"]:
        if sig_filters.is_empty()
            || sig_filters
                .iter()
                .any(|pattern| "${signal["name"]}".to_lowercase().contains(pattern))
        {
            out.push(${signal["static_ident"]}.measurement_from(self.${signal["method_name"]}));
        }
        %endfor

        if out.is_empty() {
            if allow_empty { Some(Vec::new()) } else { None }
        } else {
            Some(out)
        }
    }

    pub fn metadata(&self) -> &MessageMetadata<Bus> {
        &self.metadata
    }
    %for signal in message["signals"]:
    pub fn ${signal["method_name"]}_signal(&self) -> &'static ${signal["struct_ident"]} {
        &${signal["static_ident"]}
    }
    %endfor
}

impl From<ReceivedCanMessage<Bus>> for ${message["struct_ident"]} {
    fn from(message: ReceivedCanMessage<Bus>) -> Self {
        Self {
            metadata: MessageMetadata {
                bus: message.bus,
                name: Some(Self::NAME),
                id: message.id,
                len: message.len,
            },
            %for signal in message["signals"]:
            ${signal["method_name"]}: ${signal["static_ident"]}.value(),
            %endfor
        }
    }
}
  %endfor
%endfor

pub fn message_descriptors() -> &'static [MessageDescriptor<Bus>] {
    YAMCAN_MESSAGES
}

pub fn signal_descriptors() -> &'static [SignalDescriptor<Bus>] {
    YAMCAN_SIGNALS
}

static YAMCAN_MESSAGES: &[MessageDescriptor<Bus>] = &[
%for bus in bus_order:
  %for message in bus_messages[bus]:
    MessageDescriptor {
        bus: Bus::${rust_pascal(bus)},
        name: "${message["name"]}",
        id: ${message["id"]}u32,
        len: ${message["len"]}u8,
    },
  %endfor
%endfor
];

static YAMCAN_SIGNALS: &[SignalDescriptor<Bus>] = &[
%for bus in bus_order:
  %for message in bus_messages[bus]:
    %for signal in message["signals"]:
    SignalDescriptor {
        bus: Bus::${rust_pascal(bus)},
        message_name: "${message["name"]}",
        message_id: ${message["id"]}u32,
        signal_name: "${signal["name"]}",
        fqid: "${bus}/${message["name"]}/${signal["name"]}",
        unit: ${'None' if signal["unit"] == '' else 'Some("' + signal["unit"] + '")'},
        kind: ${signal["kind"]},
    },
    %endfor
  %endfor
%endfor
];

#[derive(Clone, Debug)]
pub enum AnyMessage {
%for bus in bus_order:
  %for message in bus_messages[bus]:
    ${message["any_variant"]}(${message["struct_ident"]}),
  %endfor
%endfor
}

impl DecodedCanMessage for AnyMessage {
    type Bus = Bus;

    fn metadata(&self) -> &MessageMetadata<Bus> {
        match self {
%for bus in bus_order:
  %for message in bus_messages[bus]:
            Self::${message["any_variant"]}(message) => message.metadata(),
  %endfor
%endfor
        }
    }

    fn measurements(
        &self,
        sig_filters: &[String],
        allow_empty: bool,
    ) -> Option<Vec<SignalMeasurement>> {
        match self {
%for bus in bus_order:
  %for message in bus_messages[bus]:
            Self::${message["any_variant"]}(message) => message.measurements(sig_filters, allow_empty),
  %endfor
%endfor
        }
    }
}
