use std::fs::File;
use std::fs::OpenOptions;
use std::fs::{create_dir_all, metadata, read_dir, remove_file, rename};
use std::io::BufWriter;
use std::io::Write;
use std::os::fd::{AsRawFd, OwnedFd};
use std::path::{Path, PathBuf};
use std::sync::mpsc::{Receiver, RecvTimeoutError, Sender, channel};
use std::thread;
use std::time::{Duration, Instant, SystemTime};

use bytes::Bytes;
use chrono::Local;
use clap::Parser;
use flate2::Compression;
use flate2::write::GzEncoder;
use influxdb2::Client as InfluxClient;
use influxdb2::ClientBuilder as InfluxClientBuilder;
use libc::{POLLIN, poll, pollfd};
use tar::Builder;
use tokio::runtime::{Builder as TokioRuntimeBuilder, Runtime};

use crate::{
    Bus, BusBinding, Event, Filters, ForwardRoute, NetworkBus, ProcessedEvent, bus_descriptor,
    configure_yamcan_iface, format_processed_event, format_processed_event_line_protocol,
    forward_route_for_pair, forward_routes_from_bus, open_can_socket, process_event, recv_event,
    send_can_frame, yamcan_init,
};

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

    #[arg(long = "log-to-influx", default_value_t = false)]
    log_to_influx: bool,

    #[arg(long = "influx-url", default_value = "http://localhost:8086")]
    influx_url: String,

    #[arg(long = "influx-token")]
    influx_token: Option<String>,

    #[arg(long = "influx-org")]
    influx_org: Option<String>,

    #[arg(long = "influx-bucket")]
    influx_bucket: Option<String>,

    #[arg(long = "influx-batch-size", default_value_t = 5_000)]
    influx_batch_size: usize,

    #[arg(long = "influx-batch-bytes", default_value_t = 4 * 1024 * 1024)]
    influx_batch_bytes: usize,

    #[arg(long = "influx-flush-ms", default_value_t = 1_000)]
    influx_flush_ms: u64,
}

#[derive(Clone)]
struct LogConfig {
    dir: PathBuf,
    tmp: PathBuf,
    label: String,
    max_bytes: u64,
    max_age: Duration,
}

struct BusLog {
    opened_at: Instant,
    current_size: u64,
    writer: BufWriter<File>,
    path: PathBuf,
}

#[derive(Clone)]
struct InfluxLogConfig {
    url: String,
    token: String,
    org: String,
    bucket: String,
    batch_size: usize,
    batch_bytes: usize,
    flush_interval: Duration,
    recovery: Option<LogConfig>,
}

enum LogBackend {
    File(LogConfig),
    Influx(InfluxLogConfig),
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
    tx: Sender<String>,
}

impl LogSink {
    fn new(backend: LogBackend) -> std::io::Result<Self> {
        let (tx, rx) = channel();
        thread::Builder::new()
            .name("can-log-writer".into())
            .spawn(move || run_log_worker(backend, rx))?;
        Ok(Self { tx })
    }
}

impl EventSink for LogSink {
    fn handle(&mut self, event: &ProcessedEvent) {
        let line = format_processed_event_line_protocol(event);
        if let Err(e) = self.tx.send(line) {
            eprintln!("log: writer thread stopped; failed to enqueue line-protocol record: {e}");
        }
    }
}

struct EventProcessor {
    filters: Filters,
    bandwidth_bits: std::collections::HashMap<String, u64>,
    last_bandwidth_report: Instant,
    sinks: Vec<Box<dyn EventSink>>,
}

impl EventProcessor {
    fn new(filters: Filters, quiet: bool, json: bool, log_backend: Option<LogBackend>) -> Self {
        let mut sinks: Vec<Box<dyn EventSink>> = Vec::new();
        if !quiet {
            sinks.push(Box::new(StdoutSink { json }));
        }
        if let Some(backend) = log_backend {
            match LogSink::new(backend) {
                Ok(sink) => sinks.push(Box::new(sink)),
                Err(e) => eprintln!("log: failed to start writer thread: {e}"),
            }
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

        self.global_log = match open_bus_log(&cfg.tmp, &cfg.label) {
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

fn run_log_worker(backend: LogBackend, rx: Receiver<String>) {
    match backend {
        LogBackend::File(cfg) => run_file_log_worker(cfg, rx),
        LogBackend::Influx(cfg) => run_influx_log_worker(cfg, rx),
    }
}

fn run_file_log_worker(cfg: LogConfig, rx: Receiver<String>) {
    let mut manager = LogManager::new(Some(cfg));
    manager.recover_uncompressed_logs();
    while let Ok(line) = rx.recv() {
        manager.write_line(&line);
    }
}

struct DirectInfluxLogWriter {
    rt: Runtime,
    client: InfluxClient,
    bucket: String,
    disk_spool_tx: Option<Sender<DiskSpoolBatch>>,
    batch_size: usize,
    batch_bytes: usize,
    flush_interval: Duration,
    last_flush: Instant,
    body_capacity: usize,
    body: Vec<u8>,
    count: usize,
    failed_batches: Vec<BufferedInfluxBatch>,
    failed_count: usize,
    failed_bytes: usize,
    written: usize,
    spooled: usize,
    last_report: Instant,
}

struct BufferedInfluxBatch {
    body: Bytes,
}

struct DiskSpoolBatch {
    batches: Vec<BufferedInfluxBatch>,
    count: usize,
    bytes: usize,
}

const FAILED_INFLUX_SPOOL_IDLE_CLOSE: Duration = Duration::from_secs(5);

impl DirectInfluxLogWriter {
    fn new(
        cfg: &InfluxLogConfig,
        disk_spool_tx: Option<Sender<DiskSpoolBatch>>,
    ) -> Result<Self, Box<dyn std::error::Error>> {
        let rt = TokioRuntimeBuilder::new_current_thread()
            .enable_all()
            .build()?;
        let client = InfluxClientBuilder::new(&cfg.url, &cfg.org, &cfg.token).build()?;
        let batch_size = cfg.batch_size.max(1);
        let batch_bytes = cfg.batch_bytes.max(1);
        let flush_interval = cfg.flush_interval.max(Duration::from_millis(1));
        let body_capacity = batch_bytes.min(batch_size.saturating_mul(160).max(1));
        Ok(Self {
            rt,
            client,
            bucket: cfg.bucket.clone(),
            disk_spool_tx,
            batch_size,
            batch_bytes,
            flush_interval,
            last_flush: Instant::now(),
            body_capacity,
            body: Vec::with_capacity(body_capacity),
            count: 0,
            failed_batches: Vec::new(),
            failed_count: 0,
            failed_bytes: 0,
            written: 0,
            spooled: 0,
            last_report: Instant::now(),
        })
    }

    fn push_line(&mut self, line: String) {
        let next_len = line.len().saturating_add(1);
        if self.count > 0
            && (self.count >= self.batch_size
                || self.body.len().saturating_add(next_len) > self.batch_bytes)
        {
            self.flush();
        }

        self.body.extend_from_slice(line.as_bytes());
        self.body.push(b'\n');
        self.count += 1;

        if self.count >= self.batch_size || self.body.len() >= self.batch_bytes {
            self.flush();
        }
    }

    fn flush(&mut self) {
        if self.count == 0 {
            self.retry_failed_buffer();
            return;
        }

        let body = Bytes::from(std::mem::replace(
            &mut self.body,
            Vec::with_capacity(self.body_capacity),
        ));
        let count = std::mem::take(&mut self.count);
        let start = Instant::now();

        if !self.write_batch(body.clone(), count) {
            self.buffer_failed_batch(body, count);
        }
        self.last_flush = Instant::now();
        self.retry_failed_buffer();

        if self.last_report.elapsed() >= Duration::from_secs(60) {
            println!(
                "log: direct Influx stats written={} spooled={} failed_buffer_records={} failed_buffer_bytes={} last_flush={:?}",
                self.written,
                self.spooled,
                self.failed_count,
                self.failed_bytes,
                start.elapsed()
            );
            self.last_report = Instant::now();
        }
    }

    fn finish(&mut self) {
        self.flush();
        if self.failed_count > 0 && !self.retry_failed_buffer() {
            self.queue_failed_buffer_to_disk();
        }
    }

    fn recv_timeout(&self) -> Duration {
        if self.count == 0 {
            return Duration::from_secs(1);
        }

        let remaining = self.flush_interval.saturating_sub(self.last_flush.elapsed());
        if remaining.is_zero() {
            Duration::from_millis(1)
        } else {
            remaining
        }
    }

    fn should_flush_for_age(&self) -> bool {
        self.count > 0 && self.last_flush.elapsed() >= self.flush_interval
    }

    fn write_batch(&mut self, body: Bytes, count: usize) -> bool {
        match self.rt.block_on(self.client.write_line_protocol(
            &self.client.org,
            &self.bucket,
            body.clone(),
        )) {
            Ok(()) => {
                self.written = self.written.saturating_add(count);
                true
            }
            Err(_) => false,
        }
    }

    fn buffer_failed_batch(&mut self, body: Bytes, count: usize) {
        if body.is_empty() || count == 0 {
            return;
        }
        self.failed_bytes = self.failed_bytes.saturating_add(body.len());
        self.failed_count = self.failed_count.saturating_add(count);
        self.failed_batches.push(BufferedInfluxBatch { body });
    }

    fn retry_failed_buffer(&mut self) -> bool {
        if self.failed_count == 0 {
            return false;
        }

        let body = self.combined_failed_body();
        let count = self.failed_count;
        if self.write_batch(body, count) {
            self.failed_batches.clear();
            self.failed_count = 0;
            self.failed_bytes = 0;
            return true;
        }

        if self.failed_buffer_is_full() {
            self.queue_failed_buffer_to_disk();
        }
        false
    }

    fn combined_failed_body(&self) -> Bytes {
        if self.failed_batches.len() == 1 {
            return self.failed_batches[0].body.clone();
        }

        let mut body = Vec::with_capacity(self.failed_bytes);
        for batch in &self.failed_batches {
            body.extend_from_slice(&batch.body);
        }
        Bytes::from(body)
    }

    fn failed_buffer_is_full(&self) -> bool {
        self.failed_count >= self.batch_size || self.failed_bytes >= self.batch_bytes
    }

    fn queue_failed_buffer_to_disk(&mut self) {
        if self.failed_count == 0 {
            return;
        }

        let batch = DiskSpoolBatch {
            batches: std::mem::take(&mut self.failed_batches),
            count: std::mem::take(&mut self.failed_count),
            bytes: std::mem::take(&mut self.failed_bytes),
        };

        let Some(tx) = self.disk_spool_tx.as_ref() else {
            eprintln!(
                "log: no log-dir configured; keeping {} failed Influx records buffered in memory",
                batch.count
            );
            self.restore_failed_buffer(batch);
            return;
        };

        let spooled_count = batch.count;
        match tx.send(batch) {
            Ok(()) => {
                self.spooled = self.spooled.saturating_add(spooled_count);
            }
            Err(e) => {
                eprintln!("log: disk spool thread stopped; keeping failed Influx batch in memory");
                self.restore_failed_buffer(e.0);
            }
        }
    }

    fn restore_failed_buffer(&mut self, mut batch: DiskSpoolBatch) {
        if self.failed_batches.is_empty() {
            self.failed_batches = batch.batches;
        } else {
            batch.batches.append(&mut self.failed_batches);
            self.failed_batches = batch.batches;
        }
        self.failed_count = self.failed_count.saturating_add(batch.count);
        self.failed_bytes = self.failed_bytes.saturating_add(batch.bytes);
    }
}

fn run_influx_log_worker(cfg: InfluxLogConfig, rx: Receiver<String>) {
    if let Some(recovery) = cfg.recovery.clone() {
        LogManager::new(Some(recovery)).recover_uncompressed_logs();
    }
    let disk_spool_tx = cfg.recovery.clone().and_then(spawn_disk_spool_worker);

    let mut writer = match DirectInfluxLogWriter::new(&cfg, disk_spool_tx) {
        Ok(writer) => writer,
        Err(e) => {
            eprintln!("log: failed to initialize direct Influx writer; falling back to disk: {e}");
            if let Some(fallback) = cfg.recovery {
                run_file_log_worker(fallback, rx);
            } else {
                eprintln!("log: no log-dir configured; direct Influx fallback is unavailable");
            }
            return;
        }
    };

    loop {
        match rx.recv_timeout(writer.recv_timeout()) {
            Ok(line) => {
                writer.push_line(line);
                for queued_line in rx.try_iter() {
                    writer.push_line(queued_line);
                }
                if writer.should_flush_for_age() {
                    writer.flush();
                }
            }
            Err(RecvTimeoutError::Timeout) => writer.flush(),
            Err(RecvTimeoutError::Disconnected) => break,
        }
    }
    writer.finish();
}

fn spawn_disk_spool_worker(cfg: LogConfig) -> Option<Sender<DiskSpoolBatch>> {
    let (tx, rx) = channel();
    match thread::Builder::new()
        .name("can-log-spool".into())
        .spawn(move || run_disk_spool_worker(cfg, rx))
    {
        Ok(_) => Some(tx),
        Err(e) => {
            eprintln!("log: failed to start disk spool thread: {e}");
            None
        }
    }
}

fn run_disk_spool_worker(cfg: LogConfig, rx: Receiver<DiskSpoolBatch>) {
    let mut writer = FailedInfluxSpoolWriter::new(cfg);

    loop {
        match rx.recv_timeout(Duration::from_secs(1)) {
            Ok(batch) => {
                write_disk_spool_batch_with_retry(&mut writer, &batch);
                for queued_batch in rx.try_iter() {
                    write_disk_spool_batch_with_retry(&mut writer, &queued_batch);
                }
            }
            Err(RecvTimeoutError::Timeout) => {
                writer.close_if_idle(FAILED_INFLUX_SPOOL_IDLE_CLOSE);
                writer.roll_if_due();
            }
            Err(RecvTimeoutError::Disconnected) => {
                writer.finish();
                break;
            }
        }
    }
}

fn write_disk_spool_batch_with_retry(writer: &mut FailedInfluxSpoolWriter, batch: &DiskSpoolBatch) {
    loop {
        match writer.write_batch(batch) {
            Ok(()) => break,
            Err(e) => {
                eprintln!("log: failed to append failed Influx batch to disk; retrying: {e}");
                thread::sleep(Duration::from_secs(1));
            }
        }
    }
}

struct FailedInfluxSpoolWriter {
    cfg: LogConfig,
    current: Option<BusLog>,
    current_records: usize,
    last_write_at: Option<Instant>,
}

impl FailedInfluxSpoolWriter {
    fn new(cfg: LogConfig) -> Self {
        Self {
            cfg,
            current: None,
            current_records: 0,
            last_write_at: None,
        }
    }

    fn write_batch(&mut self, batch: &DiskSpoolBatch) -> std::io::Result<()> {
        self.ensure_log_ready()?;

        let start_size = self
            .current
            .as_ref()
            .map(|log| log.current_size)
            .unwrap_or(0);
        let write_result = {
            let log = self.current.as_mut().ok_or_else(|| {
                std::io::Error::new(
                    std::io::ErrorKind::Other,
                    "failed Influx spool log is not open",
                )
            })?;
            for buffered in &batch.batches {
                log.writer.write_all(buffered.body.as_ref())?;
            }
            log.writer.flush()
        };

        if let Err(e) = write_result {
            self.discard_failed_append(start_size);
            return Err(e);
        }

        if let Some(log) = self.current.as_mut() {
            log.current_size = log.current_size.saturating_add(batch.bytes as u64);
        }
        self.current_records = self.current_records.saturating_add(batch.count);
        self.last_write_at = Some(Instant::now());

        if self
            .current
            .as_ref()
            .is_some_and(|log| should_roll(log, &self.cfg))
        {
            if let Err(e) = self.roll_current("rollover") {
                eprintln!("log: failed to roll failed Influx spool: {e}");
            }
        }

        Ok(())
    }

    fn roll_if_due(&mut self) {
        if self
            .current
            .as_ref()
            .is_some_and(|log| should_roll(log, &self.cfg))
        {
            if let Err(e) = self.roll_current("rollover") {
                eprintln!("log: failed to roll failed Influx spool: {e}");
            }
        }
    }

    fn close_if_idle(&mut self, idle_after: Duration) {
        let Some(last_write_at) = self.last_write_at else {
            return;
        };
        if last_write_at.elapsed() < idle_after {
            return;
        }

        if let Err(e) = self.roll_current("idle") {
            eprintln!("log: failed to close idle failed Influx spool: {e}");
        }
    }

    fn finish(&mut self) {
        if let Err(e) = self.roll_current("shutdown") {
            eprintln!("log: failed to close failed Influx spool: {e}");
        }
    }

    fn ensure_log_ready(&mut self) -> std::io::Result<()> {
        if self
            .current
            .as_ref()
            .is_some_and(|log| should_roll(log, &self.cfg))
        {
            self.roll_current("rollover")?;
        }

        if self.current.is_none() {
            self.current = Some(open_failed_influx_spool_log(&self.cfg)?);
            self.current_records = 0;
        }

        Ok(())
    }

    fn roll_current(&mut self, reason: &str) -> std::io::Result<()> {
        let Some(log) = self.current.as_mut() else {
            return Ok(());
        };
        log.writer.flush()?;

        let Some(log) = self.current.take() else {
            return Ok(());
        };

        let path = log.path.clone();
        let size = log.current_size;
        let records = std::mem::take(&mut self.current_records);
        self.last_write_at = None;
        let (file, _) = log.writer.into_parts();

        if size == 0 {
            drop(file);
            let _ = remove_file(&path);
            return Ok(());
        }

        drop(file);
        println!(
            "log: closing failed Influx spool '{}' reason={} records={} bytes={}",
            path.display(),
            reason,
            records,
            size
        );
        spawn_compression(path.clone(), self.cfg.clone());
        Ok(())
    }

    fn discard_failed_append(&mut self, start_size: u64) {
        let Some(log) = self.current.take() else {
            return;
        };

        let path = log.path.clone();
        let (file, _) = log.writer.into_parts();
        if let Err(e) = file.set_len(start_size) {
            eprintln!(
                "log: failed to truncate partial failed Influx spool '{}': {e}",
                path.display()
            );
        }
        drop(file);
        self.last_write_at = None;

        if start_size == 0 {
            let _ = remove_file(&path);
        } else {
            spawn_compression(path, self.cfg.clone());
        }
        self.current_records = 0;
    }
}

fn open_failed_influx_spool_log(cfg: &LogConfig) -> std::io::Result<BusLog> {
    create_dir_all(&cfg.tmp)?;
    let stamp = Local::now().format("%Y-%m-%d_%H%M%S");
    let nanos = now_unix_nanos();
    let path = cfg
        .tmp
        .join(format!("{}-failed-{stamp}-{nanos}.lp", cfg.label));
    let file = OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(&path)?;
    Ok(BusLog {
        opened_at: Instant::now(),
        current_size: 0,
        writer: BufWriter::new(file),
        path,
    })
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

    let mut processor = EventProcessor::new(
        filters,
        args.quiet,
        args.json,
        build_log_backend(&args, binding.bus.as_str())?,
    );
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

fn build_file_log_config(args: &Args, bus_label: &str) -> Option<LogConfig> {
    args.log_dir.as_ref().map(|dir| LogConfig {
        dir: dir.clone(),
        tmp: args
            .tmp_dir
            .clone()
            .expect("tmp folder must be specified when logging"),
        label: bus_label.to_string(),
        max_age: Duration::from_secs((args.log_rollover * 60).into()),
        max_bytes: args.log_size,
    })
}

fn build_log_backend(
    args: &Args,
    bus_label: &str,
) -> Result<Option<LogBackend>, Box<dyn std::error::Error>> {
    if !args.log_to_influx {
        return Ok(build_file_log_config(args, bus_label).map(LogBackend::File));
    }

    let token = args.influx_token.clone().ok_or_else(|| {
        std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "--log-to-influx requires --influx-token",
        )
    })?;
    let org = args.influx_org.clone().ok_or_else(|| {
        std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "--log-to-influx requires --influx-org",
        )
    })?;
    let bucket = args.influx_bucket.clone().ok_or_else(|| {
        std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "--log-to-influx requires --influx-bucket",
        )
    })?;

    Ok(Some(LogBackend::Influx(InfluxLogConfig {
        url: args.influx_url.clone(),
        token,
        org,
        bucket,
        batch_size: args.influx_batch_size,
        batch_bytes: args.influx_batch_bytes,
        flush_interval: Duration::from_millis(args.influx_flush_ms.max(1)),
        recovery: build_file_log_config(args, bus_label),
    })))
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
    base.join(format!("{bus}-{stamp}.lp"))
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
    let archive_extension = match orig_path.extension().and_then(|ext| ext.to_str()) {
        Some("lp") => "lp.tar.gz",
        _ => "log.tar.gz",
    };
    gz_path.set_extension(archive_extension);

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
        if path
            .extension()
            .is_some_and(|ext| ext == "log" || ext == "lp")
        {
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

fn now_unix_nanos() -> u128 {
    SystemTime::now()
        .duration_since(SystemTime::UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos()
}
