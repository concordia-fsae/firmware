use std::fs::File;
use std::fs::OpenOptions;
use std::fs::{create_dir_all, metadata, read_dir, remove_file, rename};
use std::io::BufWriter;
use std::io::Write;
use std::os::fd::{AsRawFd, OwnedFd};
use std::path::{Path, PathBuf};
use std::thread;
use std::time::{Duration, Instant, SystemTime};

use chrono::Local;
use clap::Parser;
use flate2::Compression;
use flate2::write::GzEncoder;
use libc::{POLLIN, poll, pollfd};
use tar::Builder;

use crate::{
    Bus, BusBinding, Event, Filters, ForwardRoute, NetworkBus, ProcessedEvent, bus_descriptor,
    configure_yamcan_iface, format_processed_event, forward_route_for_pair,
    forward_routes_from_bus, open_can_socket, process_event, recv_event, send_can_frame,
    yamcan_init,
};

const LOG_BUS_LABEL: &str = "can";

#[derive(Parser, Debug)]
#[command(
    name = "can-bridge",
    about = "Read one CAN interface, decode deterministically with yamcan, and emit raw plus decoded events."
)]
pub struct Args {
    #[arg(long, short, default_value_t = false)]
    quiet: bool,

    #[arg(long, short, default_value_t = false)]
    json: bool,

    #[arg(long = "id", short = 'i', value_name = "ID|START-END")]
    ids: Vec<String>,

    #[arg(long = "msg", short = 'm', value_name = "SUBSTR")]
    msgs: Vec<String>,

    #[arg(long = "sig", short = 's', value_name = "SUBSTR")]
    sigs: Vec<String>,

    #[arg(value_name = "IFACE")]
    input: String,

    #[arg(long = "forward-iface")]
    forward_iface: Option<String>,

    #[arg(long = "tmp-dir")]
    tmp_dir: Option<PathBuf>,

    #[arg(long = "log-dir")]
    log_dir: Option<PathBuf>,

    #[arg(long = "log-mins", default_value_t = 15)]
    log_rollover: u32,

    #[arg(long = "log-size", default_value_t = 250000)]
    log_size: u64,
}

#[derive(Clone)]
struct LogConfig {
    dir: PathBuf,
    tmp: PathBuf,
    max_bytes: u64,
    max_age: Duration,
}

struct BusLog {
    opened_at: Instant,
    current_size: u64,
    writer: BufWriter<File>,
    path: PathBuf,
}

trait EventSink {
    fn initialize(&mut self) {}
    fn handle(&mut self, event: &ProcessedEvent);
}

struct StdoutSink {
    json: bool,
}

impl EventSink for StdoutSink {
    fn handle(&mut self, event: &ProcessedEvent) {
        println!("{}", format_processed_event(event, self.json));
    }
}

struct LogSink {
    json: bool,
    manager: LogManager,
}

impl EventSink for LogSink {
    fn initialize(&mut self) {
        self.manager.recover_uncompressed_logs();
    }

    fn handle(&mut self, event: &ProcessedEvent) {
        let line = format_processed_event(event, self.json);
        self.manager.write_line(&line);
    }
}

struct EventProcessor {
    filters: Filters,
    bandwidth_bits: std::collections::HashMap<String, u64>,
    last_bandwidth_report: Instant,
    sinks: Vec<Box<dyn EventSink>>,
}

impl EventProcessor {
    fn new(filters: Filters, quiet: bool, json: bool, log_cfg: Option<LogConfig>) -> Self {
        let mut sinks: Vec<Box<dyn EventSink>> = Vec::new();
        if !quiet {
            sinks.push(Box::new(StdoutSink { json }));
        }
        if let Some(cfg) = log_cfg {
            sinks.push(Box::new(LogSink {
                json,
                manager: LogManager::new(Some(cfg)),
            }));
        }

        Self {
            filters,
            bandwidth_bits: std::collections::HashMap::new(),
            last_bandwidth_report: Instant::now(),
            sinks,
        }
    }

    fn initialize(&mut self) {
        for sink in &mut self.sinks {
            sink.initialize();
        }
    }

    fn tick(&mut self) {
        let elapsed = self.last_bandwidth_report.elapsed();
        if elapsed < Duration::from_secs(60) {
            return;
        }

        for (label, bits) in self.bandwidth_bits.drain() {
            println!(
                "[{}] {:.3} bits/s over {:?}",
                label,
                bits as f64 / elapsed.as_secs_f64(),
                elapsed
            );
        }
        self.last_bandwidth_report = Instant::now();
    }

    fn handle_event(&mut self, binding: &BusBinding<Bus>, event: Event) {
        let (_, bit_length) = crate::frame_id_and_bit_length(&event.frame);
        let bandwidth_key = format!("{}:{}", binding.iface, binding.bus.as_str());
        if let Some(processed) = process_event(binding, &self.filters, event) {
            self.bandwidth_bits
                .entry(bandwidth_key)
                .and_modify(|bits| *bits = bits.saturating_add(processed.bit_length as u64))
                .or_insert(processed.bit_length as u64);
            for sink in &mut self.sinks {
                sink.handle(&processed);
            }
            return;
        }

        self.bandwidth_bits
            .entry(bandwidth_key)
            .and_modify(|bits| *bits = bits.saturating_add(bit_length as u64))
            .or_insert(bit_length as u64);
    }
}

struct ForwardingContext {
    outgoing: Option<RouteEndpoint>,
    incoming: Option<RouteEndpoint>,
}

struct RouteEndpoint {
    route: ForwardRoute<Bus>,
    binding: BusBinding<Bus>,
    fd: OwnedFd,
}

struct LogManager {
    cfg: Option<LogConfig>,
    global_log: Option<BusLog>,
}

impl LogManager {
    fn new(cfg: Option<LogConfig>) -> Self {
        Self {
            cfg,
            global_log: None,
        }
    }

    fn recover_uncompressed_logs(&self) {
        let Some(cfg) = &self.cfg else {
            return;
        };

        match find_uncompressed_logs(&cfg.tmp) {
            Ok(paths) => {
                if !paths.is_empty() {
                    println!(
                        "log: startup: found {} uncompressed logs; compressing...",
                        paths.len()
                    );
                }
                for path in paths {
                    spawn_compression(path, cfg.clone());
                }
            }
            Err(e) => eprintln!(
                "log: startup: failed to enumerate '{}': {e}",
                cfg.dir.display()
            ),
        }
    }

    fn write_line(&mut self, line: &str) {
        let Some(cfg) = self.cfg.clone() else {
            return;
        };

        if self.ensure_log_ready(&cfg).is_err() {
            return;
        }

        if let Some(log) = self.global_log.as_mut() {
            if let Err(e) = writeln!(log.writer, "{line}") {
                eprintln!("log: write failed for single log: {e}");
                self.global_log = None;
            } else {
                log.current_size = log.current_size.saturating_add((line.len() + 1) as u64);
            }
        }
    }

    fn ensure_log_ready(&mut self, cfg: &LogConfig) -> Result<(), ()> {
        let old_path = self.global_log.as_ref().map(|log| log.path.clone());
        let need_open = match self.global_log.as_ref() {
            None => {
                println!("log: creating new log...");
                true
            }
            Some(log) => should_roll(log, cfg),
        };

        if !need_open {
            return Ok(());
        }

        if let Some(log) = &self.global_log {
            print_roll_reason(log, cfg);
        }

        self.global_log = match open_bus_log(&cfg.tmp, LOG_BUS_LABEL) {
            Ok(log) => Some(log),
            Err(e) => {
                eprintln!("log: failed to open global log: {e}");
                return Err(());
            }
        };

        if let Some(path) = old_path {
            spawn_compression(path, cfg.clone());
        }

        Ok(())
    }
}

pub fn run(bus: Bus) -> Result<(), Box<dyn std::error::Error>> {
    let args = Args::parse();
    let iface = args.input.clone();
    let iface_bus_map = [(iface.as_str(), bus)];
    let binding = configure_yamcan_iface(&iface, &iface_bus_map)?;
    let routes = forward_routes_from_bus(binding.bus);
    yamcan_init();

    let filters = Filters::from_parts(&args.ids, &args.msgs, &args.sigs)?;
    print_filter_summary(&filters);

    let mut processor =
        EventProcessor::new(filters, args.quiet, args.json, build_log_config(&args));
    processor.initialize();

    println!(
        "[bridge] listening on {}:{}",
        binding.iface,
        binding.bus.as_str()
    );
    if let Some(route) = routes.first() {
        println!(
            "[forward] route {} -> {}",
            route.source_bus.as_str(),
            route.dest_bus.as_str()
        );
    }
    if routes.len() > 1 && args.forward_iface.is_some() {
        return Err(format!(
            "Multiple forwarding routes are defined for bus '{}'; explicit route selection is not implemented yet",
            binding.bus.as_str()
        )
        .into());
    }

    loop {
        let physical_fd = match open_can_socket(&binding.iface) {
            Ok(fd) => fd,
            Err(e) => {
                eprintln!("[bridge] failed to open {}: {e}", binding.iface);
                thread::sleep(Duration::from_secs(1));
                continue;
            }
        };

        let forwarding =
            match open_forwarding_context(&binding, routes.first().copied(), &args.forward_iface) {
                Ok(ctx) => ctx,
                Err(e) => {
                    eprintln!("{e}");
                    thread::sleep(Duration::from_secs(1));
                    continue;
                }
            };

        loop {
            match wait_for_ready_fds(
                &physical_fd,
                forwarding
                    .as_ref()
                    .and_then(|ctx| ctx.incoming.as_ref())
                    .map(|ep| &ep.fd),
            ) {
                Ok((physical_ready, incoming_ready)) => {
                    if physical_ready {
                        match recv_event(&physical_fd, &binding.iface) {
                            Ok(event) => {
                                if let Some(ctx) = forwarding.as_ref() {
                                    if let Some(outgoing) = ctx.outgoing.as_ref() {
                                        forward_on_route(outgoing, &event);
                                    }
                                }
                                processor.handle_event(&binding, event);
                            }
                            Err(e) => {
                                eprintln!("[bridge] receive error on {}: {e}", binding.iface);
                                break;
                            }
                        }
                    }

                    if incoming_ready {
                        let Some(ctx) = forwarding.as_ref() else {
                            continue;
                        };
                        let Some(incoming) = ctx.incoming.as_ref() else {
                            continue;
                        };
                        match recv_event(&incoming.fd, &incoming.binding.iface) {
                            Ok(event) => {
                                forward_frame_on_route(
                                    incoming.route,
                                    &binding.iface,
                                    &physical_fd,
                                    &event,
                                );
                                processor.handle_event(&incoming.binding, event);
                            }
                            Err(e) => {
                                eprintln!(
                                    "[bridge] receive error on {}: {e}",
                                    incoming.binding.iface
                                );
                                break;
                            }
                        }
                    }

                    processor.tick();
                }
                Err(e) => {
                    eprintln!("[bridge] poll error: {e}");
                    break;
                }
            }
        }

        thread::sleep(Duration::from_secs(1));
    }
}

fn build_log_config(args: &Args) -> Option<LogConfig> {
    args.log_dir.as_ref().map(|dir| LogConfig {
        dir: dir.clone(),
        tmp: args
            .tmp_dir
            .clone()
            .expect("tmp folder must be specified when logging"),
        max_age: Duration::from_secs((args.log_rollover * 60).into()),
        max_bytes: args.log_size,
    })
}

fn open_forwarding_context(
    binding: &BusBinding<Bus>,
    route: Option<ForwardRoute<Bus>>,
    forward_iface: &Option<String>,
) -> Result<Option<ForwardingContext>, Box<dyn std::error::Error>> {
    match (route, forward_iface.as_deref()) {
        (Some(route), Some(forward_iface)) => {
            let Some(dest_desc) = bus_descriptor(route.dest_bus) else {
                return Err(format!(
                    "Missing yamcan bus descriptor for forward destination '{}'",
                    route.dest_bus.as_str()
                )
                .into());
            };
            if !dest_desc.is_virtual() {
                return Err(format!(
                    "Forward destination bus '{}' is not virtual; raw bridge forwarding requires a virtual bus destination",
                    route.dest_bus.as_str()
                )
                .into());
            }
            let outgoing = RouteEndpoint {
                route,
                binding: BusBinding {
                    iface: forward_iface.to_string(),
                    bus: route.dest_bus,
                },
                fd: open_can_socket(forward_iface)?,
            };

            let incoming = match forward_route_for_pair(route.dest_bus, binding.bus) {
                Some(incoming_route) => Some(RouteEndpoint {
                    route: *incoming_route,
                    binding: BusBinding {
                        iface: forward_iface.to_string(),
                        bus: incoming_route.source_bus,
                    },
                    fd: open_can_socket(forward_iface)?,
                }),
                None => None,
            };

            Ok(Some(ForwardingContext {
                outgoing: Some(outgoing),
                incoming,
            }))
        }
        (Some(route), None) => {
            eprintln!(
                "[forward] route to {} is configured in yamcan, but no `--forward-iface` was provided; forwarding disabled",
                route.dest_bus.as_str()
            );
            Ok(None)
        }
        (None, Some(forward_iface)) => Err(format!(
            "No yamcan forwarding route is defined for bus '{}', but `--forward-iface {forward_iface}` was provided",
            binding.bus.as_str(),
        )
        .into()),
        (None, None) => Ok(None),
    }
}

fn wait_for_ready_fds(
    primary_fd: &OwnedFd,
    incoming_fd: Option<&OwnedFd>,
) -> std::io::Result<(bool, bool)> {
    let mut fds = vec![pollfd {
        fd: primary_fd.as_raw_fd(),
        events: POLLIN,
        revents: 0,
    }];
    if let Some(fd) = incoming_fd {
        fds.push(pollfd {
            fd: fd.as_raw_fd(),
            events: POLLIN,
            revents: 0,
        });
    }

    let rc = unsafe { poll(fds.as_mut_ptr(), fds.len() as _, -1) };
    if rc < 0 {
        return Err(std::io::Error::last_os_error());
    }

    let primary_ready = (fds[0].revents & POLLIN) != 0;
    let incoming_ready = fds.get(1).is_some_and(|fd| (fd.revents & POLLIN) != 0);
    Ok((primary_ready, incoming_ready))
}

fn forward_on_route(endpoint: &RouteEndpoint, event: &Event) {
    forward_frame_on_route(endpoint.route, &endpoint.binding.iface, &endpoint.fd, event);
}

fn forward_frame_on_route(
    route: ForwardRoute<Bus>,
    dest_iface: &str,
    dest_fd: &OwnedFd,
    event: &Event,
) {
    let (id_masked, _) = crate::frame_id_and_bit_length(&event.frame);
    if !route.forwards_id(id_masked) {
        return;
    }
    if let Err(e) = send_can_frame(dest_fd, &event.frame) {
        let message_name = route
            .forwarded_message_for_id(id_masked)
            .map(|message| message.name)
            .unwrap_or("unknown");
        eprintln!(
            "[forward] failed {}:{} -> {}:{} for {} (0x{:X}): {e}",
            event.iface,
            route.source_bus.as_str(),
            dest_iface,
            route.dest_bus.as_str(),
            message_name,
            id_masked
        );
    }
}

fn print_filter_summary(filters: &Filters) {
    if filters.id_ranges.is_empty()
        && filters.msg_filters.is_empty()
        && filters.sig_filters.is_empty()
    {
        println!("[filters] none");
        return;
    }

    if !filters.id_ranges.is_empty() {
        let ids: Vec<String> = filters
            .id_ranges
            .iter()
            .map(|range| {
                if range.start == range.end {
                    format!("0x{:X} ({})", range.start, range.start)
                } else {
                    format!(
                        "0x{:X}-0x{:X} ({}-{})",
                        range.start, range.end, range.start, range.end
                    )
                }
            })
            .collect();
        println!("[filters] id: {}", ids.join(", "));
    }

    if !filters.msg_filters.is_empty() {
        println!("[filters] msg: {}", filters.msg_filters.join(", "));
    }

    if !filters.sig_filters.is_empty() {
        println!("[filters] sig: {}", filters.sig_filters.join(", "));
    }
}

fn log_file_path(base: &Path, bus: &str) -> PathBuf {
    let stamp = Local::now().format("%Y-%m-%d_%H%M%S");
    base.join(format!("{bus}-{stamp}.log"))
}

fn open_bus_log(base: &Path, bus: &str) -> std::io::Result<BusLog> {
    create_dir_all(base)?;
    let path = log_file_path(base, bus);
    let file = OpenOptions::new().create(true).append(true).open(&path)?;
    eprintln!("log: opening file {:?}", path);
    let size = file.metadata().ok().map(|m| m.len()).unwrap_or(0);
    Ok(BusLog {
        opened_at: Instant::now(),
        current_size: size,
        writer: BufWriter::new(file),
        path,
    })
}

fn should_roll(log: &BusLog, cfg: &LogConfig) -> bool {
    log.current_size >= cfg.max_bytes || log.opened_at.elapsed() >= cfg.max_age
}

fn print_roll_reason(log: &BusLog, cfg: &LogConfig) {
    let elapsed = log.opened_at.elapsed();
    let secs = elapsed.as_secs_f64();
    let size_mb = log.current_size / (1024 * 1024);
    let time_triggered = elapsed >= cfg.max_age;
    let size_triggered = log.current_size >= cfg.max_bytes;
    let reason = match (time_triggered, size_triggered) {
        (true, true) => "time and size",
        (true, false) => "time",
        (false, true) => "size",
        (false, false) => "policy",
    };
    println!(
        "log: opening new log (reason: {reason}, duration: {:.1}s, size: {} MB)",
        secs, size_mb
    );
}

fn spawn_compression(path: PathBuf, cfg: LogConfig) {
    if let Err(e) = thread::Builder::new()
        .name("log-compress".into())
        .spawn(move || {
            let start_time = Instant::now();
            match compress_and_remove(&path, &cfg.dir) {
                Ok(gz) => {
                    let size_mb = metadata(&gz).map(|m| m.len() / (1024 * 1024)).unwrap_or(0);
                    println!(
                        "log: compressed '{}' -> '{}', size: {}MB duration: {:?}",
                        path.display(),
                        gz.display(),
                        size_mb,
                        start_time.elapsed(),
                    );
                }
                Err(e) => eprintln!("log: compression failed for '{}': {e}", path.display()),
            }
        })
    {
        eprintln!("log: failed to spawn compression thread: {e}");
    }
}

fn compress_and_remove(orig_path: &Path, dest_folder: &Path) -> std::io::Result<PathBuf> {
    let mut gz_path = orig_path.to_path_buf();
    gz_path.set_extension("log.tar.gz");

    let tar_gz = File::create(&gz_path)?;
    let enc = GzEncoder::new(tar_gz, Compression::default());
    let mut tar = Builder::new(enc);
    tar.append_path_with_name(orig_path, orig_path.file_name().unwrap())?;
    let enc = tar.into_inner()?;
    enc.finish()?;

    let final_path = dest_folder.join(gz_path.file_name().unwrap());
    create_dir_all(dest_folder)?;
    rename(&gz_path, &final_path)?;
    remove_file(orig_path)?;
    Ok(final_path)
}

fn find_uncompressed_logs(dir: &Path) -> std::io::Result<Vec<PathBuf>> {
    let mut ret = Vec::new();
    if !dir.exists() {
        return Ok(ret);
    }
    for entry in read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();
        if path.extension().is_some_and(|ext| ext == "log") {
            ret.push(path);
        }
    }
    Ok(ret)
}

#[allow(dead_code)]
fn now_unix_secs() -> u64 {
    SystemTime::now()
        .duration_since(SystemTime::UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs()
}
