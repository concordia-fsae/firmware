/*
 * rust_faults_generated.rs
 * Generated Rust fault identifiers.
 */

<%!
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
faults_sent = []
faults_received = {}

for node in nodes:
    for message in node.messages.values():
        if message.fault_message and not message.from_bridge:
            for signal, data in message.signal_objs.items():
                faults_sent.append((data.message_ref.node_ref.name + "_" + data.get_name_nodeless(), int(data.start_bit)))
    for message, mdata in node.received_msgs.items():
        if mdata.fault_message:
            for signal, sdata in mdata.signal_objs.items():
                faults_received.setdefault(mdata.node_ref.name, []).append((signal, int(sdata.start_bit)))

fault_count = max([index for _, index in faults_sent], default=-1) + 1
%>\
%if faults_sent:
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum FmFault {
  %for fault, index in faults_sent:
    ${rust_pascal(fault)} = ${index},
  %endfor
}

impl FmFault {
    pub fn from_raw(value: i32) -> Option<Self> {
        match value {
        %for fault, index in faults_sent:
            ${index} => Some(Self::${rust_pascal(fault)}),
        %endfor
            _ => None,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
        %for fault, index in faults_sent:
            Self::${rust_pascal(fault)} => "${fault}",
        %endfor
        }
    }

    pub fn as_raw(self) -> i32 {
        self as i32
    }
}

impl core::convert::TryFrom<i32> for FmFault {
    type Error = i32;

    fn try_from(value: i32) -> Result<Self, Self::Error> {
        Self::from_raw(value).ok_or(value)
    }
}

impl From<FmFault> for i32 {
    fn from(value: FmFault) -> Self {
        value.as_raw()
    }
}

pub const FM_FAULT_COUNT: i32 = ${fault_count};
%else:
pub const FM_FAULT_COUNT: i32 = 0;
%endif
%if faults_received:

pub mod received_fault {
  %for node, faults in faults_received.items():
    %for fault, index in faults:
    pub const ${sanitize_ident(fault).upper()}: i32 = ${index};
    %endfor
  %endfor
}
%endif
