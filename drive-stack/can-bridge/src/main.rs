use clap::Parser;
use libc::{
    bind, can_frame, c_long, c_void, cmsghdr, if_nametoindex, iovec, msghdr, recvmsg, sa_family_t,
    sockaddr, sockaddr_can, socklen_t, socket, timespec, AF_CAN, CAN_EFF_FLAG, CAN_EFF_MASK,
    CAN_ERR_FLAG, CAN_RAW, CAN_RTR_FLAG, CAN_SFF_MASK, EINTR, SCM_TIMESTAMPING, SO_TIMESTAMPING,
    SOCK_RAW, SOL_SOCKET, SOF_TIMESTAMPING_RAW_HARDWARE, SOF_TIMESTAMPING_RX_HARDWARE,
    SOF_TIMESTAMPING_RX_SOFTWARE, SOF_TIMESTAMPING_SOFTWARE,
};
use std::collections::HashMap;
use std::ffi::CString;
use std::io;
use std::mem::{size_of, zeroed};
use std::os::fd::{AsRawFd, FromRawFd, OwnedFd};
use std::ptr;
use std::sync::{mpsc, Arc};
use std::thread;

use can_dbc::{ByteOrder, DBC, Message, MessageId, MultiplexIndicator, Signal, ValueType};

/// Parse inputs like:
///   can0
///   can1=chassis.dbc
///   vcan0=powertrain.dbc
/// into a list of bus names and an optional per-bus DBC bundle.
#[derive(Parser, Debug)]
#[command(
    name = "can-bridge",
    about = "Read multiple CAN buses; each input may be IFACE or IFACE=DBC. Merges output with timestamps; per-bus DBC decoding optional."
)]
struct Args {
    /// Quiet mode: do NOT print to stdout (still runs listeners)
    #[arg(long, short, default_value_t = false)]
    quiet: bool,

    /// Bus specs: IFACE or IFACE=PATH/TO.dbc (repeatable)
    ///
    /// Examples:
    ///   can0
    ///   can1=chassis.dbc
    ///   vcan0=/etc/dbcs/powertrain.dbc
    #[arg(value_name = "IFACE[=DBC]", num_args = 1..)]
    inputs: Vec<String>,
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args = Args::parse();
    let (tx, rx) = mpsc::channel::<String>();

    // Parse inputs into (buses list) and per-bus DBC bundles
    let (buses, per_bus_bundles) = parse_bus_specs(&args.inputs)?;

    // Spawn a thread per bus
    for bus in buses {
        let tx = tx.clone();
        let bus_name = bus.clone();
        let bundle_opt = per_bus_bundles.get(&bus_name).cloned(); // Option<Arc<BusDbc>>

        thread::Builder::new()
            .name(format!("can-bridge-{}", bus))
            .spawn(move || {
                if let Err(e) = run_bus(&bus_name, &tx, bundle_opt.as_ref()) {
                    let _ = tx.send(format!("[{bus_name}] fatal: {e}"));
                }
            })?;
    }
    drop(tx);

    // Print merged lines unless quiet
    if args.quiet {
        for _ in rx {
            // swallow
        }
    } else {
        for line in rx {
            println!("{line}");
        }
    }
    Ok(())
}

/// Parse CLI inputs (IFACE or IFACE=DBC) into:
///  - Vec<String> of unique buses in order of first appearance
///  - HashMap<String, Arc<BusDbc>> for those that provided a DBC
fn parse_bus_specs(
    specs: &[String],
) -> Result<(Vec<String>, HashMap<String, Arc<BusDbc>>), Box<dyn std::error::Error>> {
    let mut buses: Vec<String> = Vec::new();
    let mut seen: HashMap<String, usize> = HashMap::new();
    let mut bundles: HashMap<String, Arc<BusDbc>> = HashMap::new();

    for spec in specs {
        if let Some((bus, dbc_path)) = spec.split_once('=') {
            let bus = bus.trim();
            let dbc_path = dbc_path.trim();
            if bus.is_empty() || dbc_path.is_empty() {
                return Err("Each BUS=FILE must have non-empty BUS and FILE".into());
            }
            if !seen.contains_key(bus) {
                seen.insert(bus.to_string(), buses.len());
                buses.push(bus.to_string());
            }

            // Load/parse DBC and build index
            let dbc_bytes = std::fs::read(dbc_path)?;
            let dbc = DBC::from_slice(&dbc_bytes)
                .map_err(|e| io::Error::new(io::ErrorKind::Other, format!("{e:?}")))?;
            let id_index = build_id_index(&dbc);
            bundles.insert(bus.to_string(), Arc::new(BusDbc { dbc, id_index }));
        } else {
            // Plain IFACE
            let bus = spec.trim();
            if bus.is_empty() {
                return Err("Interface name must not be empty".into());
            }
            if !seen.contains_key(bus) {
                seen.insert(bus.to_string(), buses.len());
                buses.push(bus.to_string());
            }
            // No DBC for this bus (ok)
        }
    }

    if buses.is_empty() {
        return Err("Provide at least one IFACE or IFACE=DBC".into());
    }

    Ok((buses, bundles))
}

/// Bundle decoder and ID index for one bus
struct BusDbc {
    dbc: DBC,
    id_index: HashMap<u32, usize>,
}

/// Build a fast lookup: masked 11/29-bit CAN ID -> message index
fn build_id_index(dbc: &DBC) -> HashMap<u32, usize> {
    let mut map = HashMap::new();
    for (i, m) in dbc.messages().iter().enumerate() {
        let id = match m.message_id() {
            MessageId::Standard(s) => *s as u32,
            MessageId::Extended(e) => *e,
        };
        map.insert(id, i);
    }
    map
}

/// Open a RAW CAN socket bound to `iface`, enable SO_TIMESTAMPING (HW/SW).
fn open_can_with_timestamping(iface: &str) -> io::Result<OwnedFd> {
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

    // Bind to interface
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

/// Per-bus receive loop using recvmsg() to capture SCM_TIMESTAMPING and decode via per-bus DBC (if provided).
fn run_bus(bus: &str, tx: &mpsc::Sender<String>, bundle: Option<&Arc<BusDbc>>) -> io::Result<()> {
    let fd = open_can_with_timestamping(bus)?;

    let mut frame: can_frame = unsafe { zeroed() };
    let mut name: sockaddr_can = unsafe { zeroed() };

    let mut iov = iovec {
        iov_base: (&mut frame as *mut can_frame) as *mut c_void,
        iov_len: size_of::<can_frame>(),
    };

    // Enough for cmsghdr + 3x timespec + padding
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
        if n < 0 {
            let err = io::Error::last_os_error();
            if err.kind() == io::ErrorKind::WouldBlock || err.raw_os_error() == Some(EINTR) {
                continue;
            }
            return Err(err);
        }

        // Decode (unless RTR/ERR frames) and only if this bus has a DBC
        let decoded = if (frame.can_id & (CAN_RTR_FLAG | CAN_ERR_FLAG)) == 0 {
            if let Some(b) = bundle {
                let id_masked = if (frame.can_id & CAN_EFF_FLAG) != 0 {
                    frame.can_id & CAN_EFF_MASK
                } else {
                    frame.can_id & CAN_SFF_MASK
                } as u32;

                if let Some(&msg_idx) = b.id_index.get(&id_masked) {
                    let msg_dbc = &b.dbc.messages()[msg_idx];
                    Some(render_decoded(msg_dbc, &frame))
                } else {
                    None
                }
            } else {
                None
            }
        } else {
            None
        };

        let ts_opt = parse_timestamp_from_cmsgs(&msg);
        let line = format_can_line(bus, &frame, ts_opt, decoded);
        let _ = tx.send(line);
    }
}

/// Parse SCM_TIMESTAMPING ([timespec; 3]) from control messages.
/// Prefer RAW_HARDWARE (idx 2), else SOFTWARE (idx 0).
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

        if cmsg.cmsg_level == SOL_SOCKET && cmsg.cmsg_type == SCM_TIMESTAMPING {
            if cdata_len >= size_of::<timespec>() * 3 {
                let mut ts_arr: [timespec; 3] = unsafe { zeroed() };
                unsafe {
                    ptr::copy_nonoverlapping(
                        data_ptr as *const timespec,
                        ts_arr.as_mut_ptr(),
                        3,
                    );
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
        }

        let cmsg_len_aligned = (cmsg.cmsg_len as usize + align - 1) & !(align - 1);
        offset = offset.saturating_add(cmsg_len_aligned);
    }
    None
}

/// Pretty-print one CAN frame, similar to candump, plus decoded signals if available.
/// ts_opt is (sec, nsec) from kernel (HW if available, else SW). If None, prints "-".
fn format_can_line(
    bus: &str,
    f: &can_frame,
    ts_opt: Option<(u64, u32)>,
    decoded: Option<String>,
) -> String {
    let is_eff = (f.can_id & CAN_EFF_FLAG) != 0;
    let is_rtr = (f.can_id & CAN_RTR_FLAG) != 0;
    let is_err = (f.can_id & CAN_ERR_FLAG) != 0;

    let id_val = if is_eff { f.can_id & CAN_EFF_MASK } else { f.can_id & CAN_SFF_MASK };
    let id_str = if is_eff { format!("{:08X}", id_val) } else { format!("{:03X}", id_val) };

    let mut flags = String::new();
    if is_eff { flags.push_str(" EXT"); }
    if is_rtr { flags.push_str(" RTR"); }
    if is_err { flags.push_str(" ERR"); }

    let ts_str = ts_opt
        .map(|(s, ns)| format!("{}.{}", s, ns / 1_000))
        .unwrap_or_else(|| "-".to_string());

    if is_rtr {
        format!("[{}] {} ID={}{flags} DLC={}", ts_str, bus, id_str, f.can_dlc)
    } else {
        let bytes = &f.data[..(f.can_dlc as usize).min(8)];
        let payload = bytes.iter().map(|b| format!("{:02X}", b)).collect::<Vec<_>>().join(" ");
        match decoded {
            Some(dec) if !dec.is_empty() => format!(
                "[{}] {} ID={}{flags} DLC={} DATA={} | {}",
                ts_str, bus, id_str, f.can_dlc, payload, dec
            ),
            _ => format!(
                "[{}] {} ID={}{flags} DLC={} DATA={}",
                ts_str, bus, id_str, f.can_dlc, payload
            ),
        }
    }
}

// ----------------- Safe bit + decode helpers -----------------

#[inline]
fn safe_len(len: usize) -> usize {
    if len == 0 { 0 } else if len >= 64 { 64 } else { len }
}

#[inline]
fn bitmask(len: usize) -> u64 {
    let len = safe_len(len);
    if len == 0 {
        0
    } else if len == 64 {
        u64::MAX
    } else {
        (1u64 << len) - 1
    }
}

fn extract_le_bits(data: &[u8; 8], start_bit: usize, len: usize) -> u64 {
    let len = safe_len(len);
    if len == 0 { return 0; }
    let le = u64::from_le_bytes(*data);
    (le >> start_bit) & bitmask(len)
}

// DBC BigEndian (Motorola, MSB0 numbering).
fn extract_be_bits(data: &[u8; 8], start_bit: usize, len: usize) -> u64 {
    let len = safe_len(len);
    if len == 0 { return 0; }
    let mut out = 0u64;
    for j in 0..len {
        let pos = match start_bit.checked_sub(j) { Some(p) => p, None => break };
        let byte = pos / 8;
        let bit_in_byte = 7 - (pos % 8);
        let bit = (data[byte] >> bit_in_byte) & 1;
        out = (out << 1) | bit as u64;
    }
    out
}

fn sign_extend_u64(raw: u64, len: usize) -> i64 {
    let len = safe_len(len);
    if len == 0 { return 0; }
    if len == 64 { return raw as i64; }
    let sign_bit = 1u64 << (len - 1);
    let mask = (!0u64) << len;
    if (raw & sign_bit) != 0 {
        (raw | mask) as i64
    } else {
        (raw & !mask) as i64
    }
}

fn extract_raw(sig: &Signal, data: &[u8; 8]) -> u64 {
    let start = *sig.start_bit() as usize;
    let len = *sig.signal_size() as usize;
    match *sig.byte_order() {
        ByteOrder::LittleEndian => extract_le_bits(data, start, len),
        ByteOrder::BigEndian    => extract_be_bits(data, start, len),
    }
}

fn decode_signal(sig: &Signal, data: &[u8; 8]) -> f64 {
    let raw = extract_raw(sig, data);
    let len = *sig.signal_size() as usize;

    let raw_f = match sig.value_type() {
        ValueType::Signed => sign_extend_u64(raw, len) as f64,
        _ => raw as f64,
    };

    raw_f * *sig.factor() + *sig.offset()
}

fn find_mux_value(msg: &Message, data: &[u8; 8]) -> Option<u64> {
    msg.signals().iter().find_map(|s| {
        if matches!(s.multiplexer_indicator(), MultiplexIndicator::Multiplexor) {
            Some(extract_raw(s, data))
        } else {
            None
        }
    })
}

fn is_signal_active(sig: &Signal, mux: Option<u64>) -> bool {
    match (sig.multiplexer_indicator(), mux) {
        (MultiplexIndicator::Multiplexor, _) => true,
        (MultiplexIndicator::Plain, _) => true,
        (MultiplexIndicator::MultiplexedSignal(v), Some(m)) => *v as u64 == m,
        (MultiplexIndicator::MultiplexedSignal(_), None) => false,
        _ => true,
    }
}

fn render_decoded(msg: &Message, f: &can_frame) -> String {
    let data: [u8; 8] = f.data;
    let mux = find_mux_value(msg, &data);

    let mut parts: Vec<String> = Vec::new();
    for sig in msg.signals() {
        if !is_signal_active(sig, mux) {
            continue;
        }
        let v = decode_signal(sig, &data);
        let unit = sig.unit();
        if unit.is_empty() {
            parts.push(format!("{}={}", sig.name(), v));
        } else {
            parts.push(format!("{}={} {}", sig.name(), v, unit));
        }
    }

    if parts.is_empty() {
        format!("{}", msg.message_name())
    } else {
        format!("{}: {}", msg.message_name(), parts.join("; "))
    }
}

