use anyhow::{Context, Result, bail};
use chrono::{NaiveDate, NaiveDateTime, NaiveTime, TimeDelta};
use clap::Parser;
use libc::{
    AF_CAN, CAN_EFF_FLAG, CAN_ERR_FLAG, CAN_RAW, CAN_RTR_FLAG, CAN_SFF_MASK, EINTR, SOCK_RAW, bind,
    c_void, can_frame, if_nametoindex, read, sa_family_t, sockaddr, sockaddr_can, socket,
    socklen_t,
};
use log::{debug, info, warn};
use std::ffi::CString;
use std::io;
use std::mem::{size_of, zeroed};
use std::os::fd::{AsRawFd, FromRawFd, OwnedFd};
use std::process::Command;
use yamcan_clock::rust_model_generated::{
    FaultStatus, GpsQualityIndicator, VEH_VCFRONT_GPSDATE_MESSAGE as GpsDateMessage,
    VEH_VCFRONT_GPSDIAGNOSTICS_MESSAGE as GpsDiagnosticsMessage,
};
use yamcan_clock::yamcan::NetworkDecoder;

const CAN_RAW_FILTER: libc::c_int = 1;
const SOL_CAN_RAW: libc::c_int = 101;

const GPS_DATE_MESSAGE: &str = "VCFRONT_gpsDate";
const GPS_DIAGNOSTICS_MESSAGE: &str = "VCFRONT_gpsDiagnostics";
const CLOCK_SET_UTC_OFFSET_HOURS: i64 = -4;

#[derive(Parser, Debug)]
#[command(
    name = "clock-manager",
    about = "Set the carputer clock from valid YamCAN GPS time on the vehicle CAN bus"
)]
struct Opts {
    #[arg(long, default_value = "can0")]
    iface: String,

    #[arg(long, default_value_t = 3)]
    required_seconds: i64,

    #[arg(long, default_value = "timedatectl")]
    timedatectl_bin: String,

    #[arg(long)]
    dry_run: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct CanFilter {
    can_id: u32,
    can_mask: u32,
}

#[derive(Default)]
struct ClockGate {
    fix_ok: bool,
    date_ok: bool,
    time_ok: bool,
    stable_start: Option<NaiveDateTime>,
    last_datetime: Option<NaiveDateTime>,
}

impl ClockGate {
    fn update_diagnostics(&mut self, message: &GpsDiagnosticsMessage) {
        let fix_ok = matches!(
            message.VCFRONT_gpsQualityIndicator,
            Some(GpsQualityIndicator::FIX2D | GpsQualityIndicator::FIX3D)
        );
        let date_ok = message.VCFRONT_gpsInvalidDate == Some(FaultStatus::OK);
        let time_ok = message.VCFRONT_gpsInvalidTime == Some(FaultStatus::OK);

        if self.fix_ok != fix_ok || self.date_ok != date_ok || self.time_ok != time_ok {
            info!("gps readiness changed: fix={fix_ok} date={date_ok} time={time_ok}");
        }

        self.fix_ok = fix_ok;
        self.date_ok = date_ok;
        self.time_ok = time_ok;

        if !self.ready() {
            self.reset_stability();
        }
    }

    fn observe_datetime(&mut self, datetime: NaiveDateTime, required_seconds: i64) -> bool {
        if !self.ready() {
            return false;
        }

        if let Some(last) = self.last_datetime {
            if datetime < last {
                warn!("gps datetime moved backward from {last} to {datetime}; restarting gate");
                self.stable_start = Some(datetime);
                self.last_datetime = Some(datetime);
                return false;
            }

            if datetime == last {
                return false;
            }
        } else {
            info!("starting gps clock gate at {datetime}");
            self.stable_start = Some(datetime);
        }

        self.last_datetime = Some(datetime);

        let Some(start) = self.stable_start else {
            self.stable_start = Some(datetime);
            return false;
        };

        let elapsed = datetime.signed_duration_since(start).num_seconds();
        debug!("gps clock gate observed {elapsed}s of incrementing time");
        elapsed >= required_seconds
    }

    fn ready(&self) -> bool {
        self.fix_ok && self.date_ok && self.time_ok
    }

    fn reset_stability(&mut self) {
        self.stable_start = None;
        self.last_datetime = None;
    }
}

fn main() -> Result<()> {
    env_logger::init();

    let opts = Opts::parse();
    if opts.required_seconds < 1 {
        bail!("--required-seconds must be at least 1");
    }

    yamcan_clock::init_static();

    let iface_map = [(opts.iface.as_str(), yamcan_clock::Bus::Veh)];
    let binding = yamcan_clock::configure_iface(&opts.iface, &iface_map).map_err(|error| {
        anyhow::anyhow!("failed to configure yamcan for {}: {error}", opts.iface)
    })?;
    let gps_ids = gps_message_ids()?;
    let socket = open_can_socket(&opts.iface, &gps_ids)
        .with_context(|| format!("failed to open filtered CAN socket on {}", opts.iface))?;

    info!(
        "listening on {} for YamCAN GPS messages: {:?}",
        opts.iface, gps_ids
    );

    let mut gate = ClockGate::default();
    loop {
        let (frame, id) = recv_frame(&socket)
            .with_context(|| format!("failed reading filtered GPS CAN frame on {}", opts.iface))?;
        let Some(decoded) = decode_message(&binding, &frame, id) else {
            continue;
        };

        match decoded {
            yamcan_clock::AnyMessage::VehVCFRONTGpsDiagnostics(message) => {
                gate.update_diagnostics(&message);
            }
            yamcan_clock::AnyMessage::VehVCFRONTGpsDate(message) => {
                let Some(datetime) = gps_datetime(&message) else {
                    warn!("discarding malformed GPS date message");
                    gate.reset_stability();
                    continue;
                };
                if gate.observe_datetime(datetime, opts.required_seconds) {
                    let clock_datetime = gps_offset_datetime(datetime)?;
                    set_system_clock(&opts, clock_datetime)?;
                    info!(
                        "system clock set to {clock_datetime} from GPS UTC time {datetime} with UTC{CLOCK_SET_UTC_OFFSET_HOURS:+} offset"
                    );
                    return Ok(());
                }
            }
        }
    }
}

fn gps_message_ids() -> Result<Vec<u32>> {
    let mut ids = Vec::new();
    for descriptor in yamcan_clock::message_descriptors() {
        if descriptor.bus != yamcan_clock::Bus::Veh {
            continue;
        }
        if descriptor.name == GPS_DATE_MESSAGE || descriptor.name == GPS_DIAGNOSTICS_MESSAGE {
            ids.push(descriptor.id);
        }
    }

    ids.sort_unstable();
    ids.dedup();

    if ids.len() != 2 {
        bail!(
            "expected exactly two generated GPS message descriptors on veh, found {:?}",
            ids
        );
    }

    Ok(ids)
}

fn decode_message(
    binding: &yamcan_clock::BusBinding<yamcan_clock::Bus>,
    frame: &yamcan_clock::CanFrame,
    id: u32,
) -> Option<yamcan_clock::AnyMessage> {
    let message = yamcan_clock::ReceivedCanMessage {
        bus: binding.bus,
        id,
        len: frame.can_dlc,
        data: frame.data,
    };

    match <yamcan_clock::GeneratedNetwork as NetworkDecoder>::decode_received_message(message) {
        yamcan_clock::MessageDecodeResult::Decoded(message) => Some(message),
        yamcan_clock::MessageDecodeResult::Unhandled(_) => None,
    }
}

fn gps_datetime(message: &GpsDateMessage) -> Option<NaiveDateTime> {
    let date = NaiveDate::from_ymd_opt(
        message.VCFRONT_year as i32,
        message.VCFRONT_month as u32,
        message.VCFRONT_day as u32,
    )?;
    let time = NaiveTime::from_hms_opt(
        message.VCFRONT_hour as u32,
        message.VCFRONT_minute as u32,
        message.VCFRONT_second as u32,
    )?;
    Some(NaiveDateTime::new(date, time))
}

fn set_system_clock(opts: &Opts, datetime: NaiveDateTime) -> Result<()> {
    run_privileged(&opts.timedatectl_bin, &["set-ntp", "false"], opts.dry_run)
        .context("failed to disable NTP before setting system clock")?;

    let timestamp = clock_set_timestamp(datetime);
    run_privileged(
        &opts.timedatectl_bin,
        &["set-time", timestamp.as_str()],
        opts.dry_run,
    )
    .with_context(|| format!("failed to set system clock to {timestamp}"))?;

    Ok(())
}

fn gps_offset_datetime(datetime: NaiveDateTime) -> Result<NaiveDateTime> {
    datetime
        .checked_add_signed(TimeDelta::hours(CLOCK_SET_UTC_OFFSET_HOURS))
        .with_context(|| {
            format!(
                "failed to apply UTC{CLOCK_SET_UTC_OFFSET_HOURS:+} offset to GPS datetime {datetime}"
            )
        })
}

fn clock_set_timestamp(datetime: NaiveDateTime) -> String {
    datetime.format("%Y-%m-%d %H:%M:%S").to_string()
}

fn run_privileged(program: &str, args: &[&str], dry_run: bool) -> Result<()> {
    let use_sudo = unsafe { libc::geteuid() } != 0;
    let display = if use_sudo {
        format!("sudo {program} {}", args.join(" "))
    } else {
        format!("{program} {}", args.join(" "))
    };

    if dry_run {
        info!("dry-run: {display}");
        return Ok(());
    }

    let status = if use_sudo {
        Command::new("sudo").arg(program).args(args).status()
    } else {
        Command::new(program).args(args).status()
    }
    .with_context(|| format!("failed to run `{display}`"))?;

    if !status.success() {
        bail!("`{display}` exited with {status}");
    }

    Ok(())
}

fn open_can_socket(iface: &str, filter_ids: &[u32]) -> io::Result<OwnedFd> {
    let fd = unsafe { socket(AF_CAN, SOCK_RAW, CAN_RAW) };
    if fd < 0 {
        return Err(io::Error::last_os_error());
    }
    let fd = unsafe { OwnedFd::from_raw_fd(fd) };

    install_can_filters(&fd, filter_ids)?;

    let ifname = CString::new(iface)
        .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "invalid iface"))?;
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

fn install_can_filters(fd: &OwnedFd, filter_ids: &[u32]) -> io::Result<()> {
    let filters = filter_ids
        .iter()
        .map(|id| CanFilter {
            can_id: *id,
            can_mask: CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_SFF_MASK,
        })
        .collect::<Vec<_>>();

    let rc = unsafe {
        libc::setsockopt(
            fd.as_raw_fd(),
            SOL_CAN_RAW,
            CAN_RAW_FILTER,
            filters.as_ptr() as *const c_void,
            (filters.len() * size_of::<CanFilter>()) as socklen_t,
        )
    };
    if rc != 0 {
        return Err(io::Error::last_os_error());
    }

    Ok(())
}

fn recv_frame(fd: &OwnedFd) -> io::Result<(yamcan_clock::CanFrame, u32)> {
    loop {
        let mut raw: can_frame = unsafe { zeroed() };
        let n = unsafe {
            read(
                fd.as_raw_fd(),
                &mut raw as *mut can_frame as *mut c_void,
                size_of::<can_frame>(),
            )
        };
        if n <= 0 {
            let err = io::Error::last_os_error();
            if err.raw_os_error() == Some(EINTR) {
                continue;
            }
            return Err(err);
        }
        if n as usize != size_of::<can_frame>() {
            return Err(io::Error::new(
                io::ErrorKind::UnexpectedEof,
                "short CAN frame read",
            ));
        }
        if (raw.can_id & (CAN_RTR_FLAG | CAN_ERR_FLAG)) != 0 {
            continue;
        }

        let id = if (raw.can_id & CAN_EFF_FLAG) != 0 {
            raw.can_id & libc::CAN_EFF_MASK
        } else {
            raw.can_id & CAN_SFF_MASK
        };
        return Ok((
            yamcan_clock::CanFrame {
                can_id: raw.can_id,
                can_dlc: raw.can_dlc,
                data: raw.data,
            },
            id,
        ));
    }
}
