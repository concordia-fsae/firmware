use std::error::Error;
use std::ffi::CString;
use std::io;
use std::mem::{size_of, zeroed};
use std::os::fd::{AsRawFd, FromRawFd, OwnedFd};
use std::ptr;

use libc::{
    bind, c_long, c_void, can_frame, cmsghdr, if_nametoindex, iovec, msghdr, recvmsg, sa_family_t,
    sockaddr, sockaddr_can, socket, socklen_t, timespec, write, AF_CAN, CAN_EFF_FLAG, CAN_EFF_MASK,
    CAN_ERR_FLAG, CAN_RAW, CAN_RTR_FLAG, CAN_SFF_MASK, EINTR, SCM_TIMESTAMPING, SOCK_RAW,
    SOF_TIMESTAMPING_RAW_HARDWARE, SOF_TIMESTAMPING_RX_HARDWARE, SOF_TIMESTAMPING_RX_SOFTWARE,
    SOF_TIMESTAMPING_SOFTWARE, SOL_SOCKET, SO_TIMESTAMPING,
};
use serde_json::{json, Map, Value};

pub mod app;

pub use yamcan_generated::{
    bus_descriptor, forward_route_for_bus, forward_route_for_pair, forward_routes_from_bus, Bus,
    BusBinding, BusDescriptor, BusInterfaceType, CanFrame, DecodedMessage, ForwardRoute,
    NetworkBus, SignalMeasurement,
};
pub use yamcan_generated::{configure_iface as configure_yamcan_iface, init as yamcan_init};

#[derive(Clone, Debug)]
pub struct Event {
    pub iface: String,
    pub frame: CanFrame,
    pub ts_opt: Option<(u64, u32)>,
}

#[derive(Clone, Debug)]
pub struct ProcessedEvent {
    pub binding: BusBinding<Bus>,
    pub frame: CanFrame,
    pub ts_opt: Option<(u64, u32)>,
    pub id_masked: u32,
    pub bit_length: u32,
    pub decoded: Option<DecodedMessage>,
}

impl ProcessedEvent {
    pub fn iface(&self) -> &str {
        &self.binding.iface
    }

    pub fn bus_name(&self) -> &'static str {
        self.binding.bus.as_str()
    }
}

#[derive(Clone)]
pub struct Filters {
    pub id_ranges: Vec<IdRange>,
    pub msg_filters: Vec<String>,
    pub sig_filters: Vec<String>,
}

impl Filters {
    pub fn from_parts(
        ids: &[String],
        msgs: &[String],
        sigs: &[String],
    ) -> Result<Self, Box<dyn Error>> {
        let mut id_ranges = Vec::new();
        for tok in ids {
            id_ranges.push(parse_id_range(tok)?);
        }
        let msg_filters = msgs.iter().map(|s| s.to_lowercase()).collect();
        let sig_filters = sigs.iter().map(|s| s.to_lowercase()).collect();
        Ok(Self {
            id_ranges,
            msg_filters,
            sig_filters,
        })
    }

    #[inline]
    pub fn match_id(&self, id: u32) -> bool {
        self.id_ranges.is_empty() || self.id_ranges.iter().any(|r| r.contains(id))
    }

    #[inline]
    pub fn has_decode_filters(&self) -> bool {
        !self.msg_filters.is_empty() || !self.sig_filters.is_empty()
    }
}

#[derive(Clone, Copy)]
pub struct IdRange {
    pub start: u32,
    pub end: u32,
}

impl IdRange {
    pub fn contains(&self, x: u32) -> bool {
        self.start <= x && x <= self.end
    }
}

pub fn parse_id_range(s: &str) -> Result<IdRange, Box<dyn Error>> {
    let s = s.trim();
    if let Some((a, b)) = s.split_once('-') {
        let start = parse_u32_id(a.trim())?;
        let end = parse_u32_id(b.trim())?;
        let (start, end) = if start <= end {
            (start, end)
        } else {
            (end, start)
        };
        Ok(IdRange { start, end })
    } else {
        let v = parse_u32_id(s)?;
        Ok(IdRange { start: v, end: v })
    }
}

fn parse_u32_id(tok: &str) -> Result<u32, Box<dyn Error>> {
    let t = tok.trim().to_lowercase();
    if let Some(hex) = t.strip_prefix("0x") {
        Ok(u32::from_str_radix(hex, 16)?)
    } else if t.chars().any(|c| matches!(c, 'a'..='f')) {
        Ok(u32::from_str_radix(&t, 16)?)
    } else {
        Ok(t.parse::<u32>()?)
    }
}

pub fn process_event(
    binding: &BusBinding<Bus>,
    filters: &Filters,
    event: Event,
) -> Option<ProcessedEvent> {
    let (id_masked, bit_length) = frame_id_and_bit_length(&event.frame);
    if !filters.match_id(id_masked) {
        return None;
    }

    let decoded = if filters.has_decode_filters() {
        Some(yamcan_generated::maybe_decode(
            Some(binding),
            &event.frame,
            id_masked,
            false,
            false,
            &filters.msg_filters,
            &filters.sig_filters,
        )?)
    } else {
        yamcan_generated::maybe_decode(Some(binding), &event.frame, id_masked, true, true, &[], &[])
    };

    Some(ProcessedEvent {
        binding: binding.clone(),
        frame: event.frame,
        ts_opt: event.ts_opt,
        id_masked,
        bit_length,
        decoded,
    })
}

pub fn format_processed_event(event: &ProcessedEvent, json: bool) -> String {
    let f = &event.frame;
    let is_eff = (f.can_id & CAN_EFF_FLAG) != 0;
    let is_rtr = (f.can_id & CAN_RTR_FLAG) != 0;
    let is_err = (f.can_id & CAN_ERR_FLAG) != 0;

    let id_str = if is_eff {
        format!("{:08X}", event.id_masked)
    } else {
        format!("{:03X}", event.id_masked)
    };

    let mut flags = String::new();
    if is_eff {
        flags.push_str(" EXT");
    }
    if is_rtr {
        flags.push_str(" RTR");
    }
    if is_err {
        flags.push_str(" ERR");
    }

    let ts_str = event
        .ts_opt
        .map(|(s, ns)| format!("{}.{}", s, ns / 1_000))
        .unwrap_or_else(|| "-".to_string());

    if !json {
        if is_rtr {
            return format!(
                "[{}] {}:{} ID={}{flags} DLC={}",
                ts_str,
                event.iface(),
                event.bus_name(),
                id_str,
                f.can_dlc
            );
        }

        let bytes = &f.data[..(f.can_dlc as usize).min(8)];
        let payload = bytes
            .iter()
            .map(|b| format!("{:02X}", b))
            .collect::<Vec<_>>()
            .join(" ");

        if let Some(decoded) = &event.decoded {
            if let Some(dec_str) = decoded_to_pretty(decoded, true) {
                return format!(
                    "[{}] {}:{} ID={}{flags} DLC={} DATA={}\n{}",
                    ts_str,
                    event.iface(),
                    event.bus_name(),
                    id_str,
                    f.can_dlc,
                    payload,
                    dec_str
                );
            }
        }

        return format!(
            "[{}] {}:{} ID={}{flags} DLC={} DATA={}",
            ts_str,
            event.iface(),
            event.bus_name(),
            id_str,
            f.can_dlc,
            payload
        );
    }

    let (sec, nsec) = event.ts_opt.unwrap_or_default();
    let data_vec = if is_rtr {
        Vec::<String>::new()
    } else {
        let bytes = &f.data[..(f.can_dlc as usize).min(8)];
        bytes
            .iter()
            .map(|b| format!("{:02X}", b))
            .collect::<Vec<_>>()
    };

    let (msg_name, measurements) = if let Some(decoded) = &event.decoded {
        (
            Some(decoded.message_name.clone()),
            Value::Object(decoded_to_measurements_map(decoded)),
        )
    } else {
        (None, Value::Object(Map::new()))
    };

    json!({
        "msg": msg_name,
        "bus": {
            "iface": event.iface(),
            "name": event.bus_name(),
        },
        "time": Value::from(sec as f64 + (nsec as f64 / 1e9)),
        "id": {
            "val": event.id_masked,
            "ext": is_eff,
            "rtr": is_rtr,
            "err": is_err,
        },
        "dlc": f.can_dlc,
        "data": data_vec,
        "meas": measurements,
    })
    .to_string()
}

fn decoded_to_pretty(decoded: &DecodedMessage, allow_empty: bool) -> Option<String> {
    if decoded.members.is_empty() {
        return if allow_empty {
            Some(decoded.message_name.clone())
        } else {
            None
        };
    }

    let mut parts = String::new();
    for measurement in &decoded.members {
        let suffix = match (measurement.label.as_deref(), measurement.unit.as_deref()) {
            (Some(label), Some(unit)) if !unit.is_empty() => format!(" ({label}) {unit}"),
            (Some(label), _) => format!(" ({label})"),
            (None, Some(unit)) if !unit.is_empty() => format!(" {unit}"),
            _ => String::new(),
        };
        parts.push_str(&format!(
            "  {}={}{}\n",
            measurement.name, measurement.value, suffix
        ));
    }

    Some(format!("{}:\n{}", decoded.message_name, parts))
}

fn decoded_to_measurements_map(decoded: &DecodedMessage) -> Map<String, Value> {
    let mut map = Map::new();
    for measurement in &decoded.members {
        if measurement.label.is_some()
            || measurement
                .unit
                .as_deref()
                .is_some_and(|unit| !unit.is_empty())
        {
            let mut obj = Map::new();
            obj.insert("value".into(), Value::from(measurement.value));
            if let Some(unit) = measurement.unit.as_deref() {
                if !unit.is_empty() {
                    obj.insert("unit".into(), Value::from(unit));
                }
            }
            if let Some(label) = measurement.label.as_deref() {
                obj.insert("label".into(), Value::from(label));
            }
            map.insert(measurement.name.clone(), Value::Object(obj));
            continue;
        }
        map.insert(measurement.name.clone(), Value::from(measurement.value));
    }
    map
}

pub fn frame_id_and_bit_length(frame: &CanFrame) -> (u32, u32) {
    let mut bit_length = 43 + frame.can_dlc as u32 * 8;
    let id_masked = if (frame.can_id & CAN_EFF_FLAG) != 0 {
        bit_length += 29;
        frame.can_id & CAN_EFF_MASK
    } else {
        bit_length += 11;
        frame.can_id & CAN_SFF_MASK
    };

    (id_masked, bit_length)
}

pub fn recv_event(fd: &OwnedFd, iface: &str) -> io::Result<Event> {
    let mut frame: can_frame = unsafe { zeroed() };
    let mut name: sockaddr_can = unsafe { zeroed() };
    let mut iov = iovec {
        iov_base: (&mut frame as *mut can_frame) as *mut c_void,
        iov_len: size_of::<can_frame>(),
    };
    let mut cbuf = [0u8; 256];

    loop {
        let mut msg: msghdr = unsafe { zeroed() };
        msg.msg_name = (&mut name as *mut sockaddr_can) as *mut c_void;
        msg.msg_namelen = size_of::<sockaddr_can>() as socklen_t;
        msg.msg_iov = &mut iov as *mut iovec;
        msg.msg_iovlen = 1;
        msg.msg_control = cbuf.as_mut_ptr() as *mut c_void;
        msg.msg_controllen = cbuf.len();

        let n = unsafe { recvmsg(fd.as_raw_fd(), &mut msg, 0) };
        if n <= 0 {
            let err = io::Error::last_os_error();
            if err.raw_os_error() == Some(EINTR) {
                continue;
            }
            return Err(err);
        }

        let ts_opt = parse_timestamp_from_cmsgs(&msg);
        let frame = CanFrame {
            can_id: frame.can_id,
            can_dlc: frame.can_dlc,
            data: frame.data,
        };
        return Ok(Event {
            iface: iface.to_string(),
            frame,
            ts_opt,
        });
    }
}

pub fn open_can_socket(iface: &str) -> io::Result<OwnedFd> {
    let fd = unsafe { socket(AF_CAN, SOCK_RAW, CAN_RAW) };
    if fd < 0 {
        return Err(io::Error::last_os_error());
    }
    let fd = unsafe { OwnedFd::from_raw_fd(fd) };

    let ts_flags = SOF_TIMESTAMPING_RX_HARDWARE
        | SOF_TIMESTAMPING_RAW_HARDWARE
        | SOF_TIMESTAMPING_RX_SOFTWARE
        | SOF_TIMESTAMPING_SOFTWARE;

    let rc = unsafe {
        libc::setsockopt(
            fd.as_raw_fd(),
            SOL_SOCKET,
            SO_TIMESTAMPING,
            &ts_flags as *const _ as *const c_void,
            size_of::<i32>() as socklen_t,
        )
    };
    if rc != 0 {
        return Err(io::Error::last_os_error());
    }

    let ifname = CString::new(iface).unwrap();
    let ifindex = unsafe { if_nametoindex(ifname.as_ptr()) };
    if ifindex == 0 {
        return Err(io::Error::last_os_error());
    }

    let mut addr: sockaddr_can = unsafe { zeroed() };
    addr.can_family = AF_CAN as sa_family_t;
    addr.can_ifindex = ifindex as i32;

    let rc = unsafe {
        bind(
            fd.as_raw_fd(),
            &addr as *const _ as *const sockaddr,
            size_of::<sockaddr_can>() as socklen_t,
        )
    };
    if rc != 0 {
        return Err(io::Error::last_os_error());
    }

    Ok(fd)
}

pub fn send_can_frame(fd: &OwnedFd, frame: &CanFrame) -> io::Result<()> {
    let mut raw_frame: can_frame = unsafe { zeroed() };
    raw_frame.can_id = frame.can_id;
    raw_frame.can_dlc = frame.can_dlc;
    raw_frame.data = frame.data;

    let written = unsafe {
        write(
            fd.as_raw_fd(),
            &raw_frame as *const can_frame as *const c_void,
            size_of::<can_frame>(),
        )
    };
    if written < 0 {
        return Err(io::Error::last_os_error());
    }
    if written as usize != size_of::<can_frame>() {
        return Err(io::Error::new(
            io::ErrorKind::WriteZero,
            "short CAN frame write",
        ));
    }
    Ok(())
}

fn parse_timestamp_from_cmsgs(msg: &msghdr) -> Option<(u64, u32)> {
    let ctrl = msg.msg_control as *const u8;
    let mut offset = 0usize;
    let clen = msg.msg_controllen as usize;
    let align = size_of::<c_long>();

    while offset + size_of::<cmsghdr>() <= clen {
        let cmsg_ptr = unsafe { ctrl.add(offset) as *const cmsghdr };
        let cmsg = unsafe { ptr::read_unaligned(cmsg_ptr) };

        if cmsg.cmsg_len < size_of::<cmsghdr>() as usize {
            break;
        }

        let cdata_len = cmsg.cmsg_len as usize - size_of::<cmsghdr>();
        let data_ptr = unsafe { (cmsg_ptr as *const u8).add(size_of::<cmsghdr>()) };

        if cmsg.cmsg_level == SOL_SOCKET
            && cmsg.cmsg_type == SCM_TIMESTAMPING
            && cdata_len >= size_of::<timespec>() * 3
        {
            let mut ts_arr: [timespec; 3] = unsafe { zeroed() };
            unsafe {
                ptr::copy_nonoverlapping(data_ptr as *const timespec, ts_arr.as_mut_ptr(), 3);
            }
            let pick = |t: timespec| -> Option<(u64, u32)> {
                if t.tv_sec != 0 || t.tv_nsec != 0 {
                    Some((t.tv_sec as u64, t.tv_nsec as u32))
                } else {
                    None
                }
            };
            return pick(ts_arr[2]).or_else(|| pick(ts_arr[0]));
        }

        let cmsg_len_aligned = (cmsg.cmsg_len as usize + align - 1) & !(align - 1);
        offset = offset.saturating_add(cmsg_len_aligned);
    }

    None
}
