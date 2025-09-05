use clap::Parser;
use libc::{
    EINTR, AF_CAN, CAN_RAW, SOCK_RAW, SOL_SOCKET, SCM_TIMESTAMPING, SO_TIMESTAMPING,
    CAN_EFF_FLAG, CAN_RTR_FLAG, CAN_ERR_FLAG, CAN_SFF_MASK, CAN_EFF_MASK,
    SOF_TIMESTAMPING_RX_HARDWARE, SOF_TIMESTAMPING_RAW_HARDWARE,
    SOF_TIMESTAMPING_RX_SOFTWARE, SOF_TIMESTAMPING_SOFTWARE,
    sockaddr, sockaddr_can, socklen_t, msghdr, cmsghdr, iovec, timespec,
    bind, can_frame, c_long, c_void, sa_family_t,
    if_nametoindex, recvmsg, setsockopt, socket, 
};
use std::ffi::CString;
use std::io;
use std::mem::{size_of, zeroed};
use std::os::fd::{AsRawFd, FromRawFd, OwnedFd};
use std::ptr;
use std::sync::mpsc;
use std::thread;

/// CAN interfaces to read (e.g. can0 can1 vcan0)
#[derive(Parser, Debug)]
#[command(about = "Read multiple CAN buses and print a merged stream (with HW/SW timestamps)")]
struct Args {
    /// List of interfaces to process messages over
    #[arg(value_name = "IFACE", num_args = 1..)]
    buses: Vec<String>,
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args = Args::parse();
    let (tx, rx) = mpsc::channel::<String>();

    for bus in args.buses {
        let tx = tx.clone();
        let bus_name = bus.clone();

        thread::Builder::new()
            .name(format!("can-bridge-{}", bus))
            .spawn(move || {
                if let Err(e) = run_bus(&bus_name, &tx) {
                    let _ = tx.send(format!("[{bus_name}] fatal: {e}"));
                }
            })?;
    }
    drop(tx);

    for line in rx {
        println!("{line}");
    }
    Ok(())
}

/// Open a RAW CAN socket bound to `iface`, enable SO_TIMESTAMPING (HW/SW).
fn open_can_with_timestamping(iface: &str) -> io::Result<OwnedFd> {
    // socket(AF_CAN, SOCK_RAW, CAN_RAW)
    let fd = unsafe { socket(AF_CAN, SOCK_RAW, CAN_RAW) };
    if fd < 0 {
        return Err(io::Error::last_os_error());
    }
    let fd = unsafe { OwnedFd::from_raw_fd(fd) };

    // Enable timestamping: request HW (RAW_HARDWARE) and SW as fallback.
    let ts_flags = SOF_TIMESTAMPING_RX_HARDWARE
        | SOF_TIMESTAMPING_RAW_HARDWARE
        | SOF_TIMESTAMPING_RX_SOFTWARE
        | SOF_TIMESTAMPING_SOFTWARE;

    let rc = unsafe {
        setsockopt(
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

/// Per-bus receive loop using recvmsg() to capture SCM_TIMESTAMPING.
fn run_bus(bus: &str, tx: &mpsc::Sender<String>) -> io::Result<()> {
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

        let ts_opt = parse_timestamp_from_cmsgs(&msg);
        let line = format_can_line(bus, &frame, ts_opt);
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
        // SAFETY: control buffer is kernel-provided; we read unaligned safely.
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

        // CMSG_NXTHDR alignment (round up to sizeof(long))
        let cmsg_len_aligned = (cmsg.cmsg_len as usize + align - 1) & !(align - 1);
        offset = offset.saturating_add(cmsg_len_aligned);
    }
    None
}

/// Pretty-print one CAN frame, similar to candump.
/// ts_opt is (sec, nsec) from kernel (HW if available, else SW). If None, prints "-".
fn format_can_line(bus: &str, f: &can_frame, ts_opt: Option<(u64, u32)>) -> String {
    let is_eff = (f.can_id & CAN_EFF_FLAG) != 0;
    let is_rtr = (f.can_id & CAN_RTR_FLAG) != 0;
    let is_err = (f.can_id & CAN_ERR_FLAG) != 0;

    let id_val = if is_eff { f.can_id & CAN_EFF_MASK } else { f.can_id & CAN_SFF_MASK };
    let id_str = if is_eff { format!("{:08X}", id_val) } else { format!("{:03X}", id_val) };

    let mut flags = String::new();
    if is_eff { flags.push_str(" EXT"); }
    if is_rtr { flags.push_str(" RTR"); }
    if is_err { flags.push_str(" ERR"); }

    // Default to "-" if no timestamp; otherwise show secs.usecs (candump-style)
    let ts_str = ts_opt
        .map(|(s, ns)| format!("{}.{}", s, ns / 1_000))
        .unwrap_or_else(|| "-".to_string());

    if is_rtr {
        format!("[{}] {} ID={}{flags} DLC={}", ts_str, bus, id_str, f.can_dlc)
    } else {
        let bytes = &f.data[..(f.can_dlc as usize).min(8)];
        let payload = bytes.iter().map(|b| format!("{:02X}", b)).collect::<Vec<_>>().join(" ");
        format!(
            "[{}] {} ID={}{flags} DLC={} DATA={}",
            ts_str, bus, id_str, f.can_dlc, payload
        )
    }
}

