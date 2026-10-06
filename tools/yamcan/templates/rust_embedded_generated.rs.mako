// Generated Yamcan no_std Rust wrapper. Do not edit.
<%!
import re

def snake(name):
    return re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", name).replace("-", "_")
%>
pub mod codec {
${wire_codec}
}
pub use codec::{get_bits, set_bits};
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Frame {
    pub id: u32,
    pub len: u8,
    pub data: [u8; 8],
}
#[derive(Clone, Copy)]
pub struct Message {
    pub id: u32,
    pub len: u8,
    pub period: u32,
    pub timeout: u32,
    pub checksum: Option<(u8, u8, u32)>,
    pub counter: Option<(u8, u8)>,
}
#[derive(Clone, Copy)]
pub struct Signal {
    pub message: usize,
    pub start: u8,
    pub width: u8,
    pub signed: bool,
    pub scale: f64,
    pub offset: f64,
    pub sna: Option<u64>,
    pub min: f64,
    pub max: f64,
}
impl Signal {
    /// Encode a physical value, clamped to the network-defined range.
    pub fn encode(self, data: &mut [u8; 8], value: f64) {
        let raw = codec::encode_raw(value, self.scale, self.offset, self.min, self.max);
        set_bits(data, self.start, self.width, raw);
    }
    pub fn set(self, data: &mut [u8; 8], raw: u32) {
        set_bits(data, self.start, self.width, raw.into());
    }
    pub fn value(self, raw: u64) -> f64 {
        codec::decode_raw(raw, self.width, self.signed, self.scale, self.offset)
    }
    pub fn raw(self, data: &[u8; 8]) -> u64 {
        get_bits(data, self.start, self.width)
    }
}
impl Message {
    pub fn frame(self, mut data: [u8; 8], counter: u8) -> Frame {
        if let Some((start, width)) = self.counter {
            set_bits(&mut data, start, width, counter.into());
        }
        if let Some((start, width, seed)) = self.checksum {
            codec::apply_checksum(&mut data, self.len, start, width, seed);
        }
        Frame {
            id: self.id,
            len: self.len,
            data,
        }
    }
    pub fn valid(self, frame: &Frame) -> bool {
        if frame.id != self.id || frame.len != self.len {
            return false;
        }
        if let Some((start, width, seed)) = self.checksum {
            return codec::valid_checksum(&frame.data, self.len, start, width, seed);
        }
        true
    }
}

<%
node = nodes[0]
messages = {m.name: m for m in [*node.received_msgs.values(), *node.messages.values()]}
enums = {}
%>
%for idx, msg in enumerate(messages.values()):
pub const MSG_${snake(msg.name).upper()}: usize = ${idx};
  %for sig in msg.signal_objs.values():
<%
n = sig.native_representation
assert n.endianness.name == "little", f"unsupported endianness: {sig.name}"
if sig.discrete_values:
    enums[sig.discrete_values.name] = sig.discrete_values
sna = sig.sna_params.value
%>
// Preserve the resolved network range, including rounded 64-bit limits.
#[allow(clippy::excessive_precision)]
pub const ${snake(sig.name).upper()}: Signal = Signal { message: ${idx}, start: ${sig.start_bit}, width: ${n.bit_width}, signed: ${str(n.signedness.name == 'signed').lower()}, scale: ${float(sig.scale)}f64, offset: ${float(sig.offset)}f64, sna: ${f'Some({sna})' if sna is not None else 'None'}, min: ${float(n.range.min)}f64, max: ${float(n.range.max)}f64 };
  %endfor
%endfor
pub const MESSAGES: [Message; ${len(messages)}] = [
%for msg in messages.values():
<%
checksum = msg.checksum_sig
check = f"Some(({checksum.start_bit}, {checksum.native_representation.bit_width}, {msg.crc}))" if checksum else "None"
counter = msg.counter_sig
count = f"Some(({counter.start_bit}, {counter.native_representation.bit_width}))" if counter else "None"
%>
Message { id: ${msg.id}, len: ${msg.length_bytes}, period: ${msg.cycle_time_ms or 0}, timeout: ${msg.timeout_period_ms}, checksum: ${check}, counter: ${count} },
%endfor
];
pub const TX_MESSAGES: &[usize] = &[
%for msg in node.messages.values():
  %if not msg.unscheduled:
MSG_${snake(msg.name).upper()},
  %endif
%endfor
];
%for name, enum in enums.items():
pub mod ${snake(name).lower()} {
  %for key, val in enum.values.items():
pub const ${snake(str(key)).upper()}: u32 = ${val};
  %endfor
}
%endfor
