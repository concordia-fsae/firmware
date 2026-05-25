use std::collections::BTreeMap;
use std::fs::File;
use std::fs::remove_file;
use std::io::{BufRead, BufReader};
use std::path::{Path, PathBuf};
use std::time::Duration;
use std::time::Instant;

use anyhow::{Context, Result};
use flate2::read::GzDecoder;
use futures::StreamExt;
use futures::stream::FuturesUnordered;
use influxdb2::Client as InfluxClient;
use influxdb2::ClientBuilder as InfluxClientBuilder;
use influxdb2::models::DataPoint;
use influxdb2::models::WriteDataPoint;
use influxdb2::models::data_point::DataPointBuilder;
use serde::Deserialize;
use serde_json::Value;
use tar::Archive;
use tokio::sync::mpsc;
use tokio::task::JoinHandle;

/// Ingest one `.tar.gz` (JSON-lines files inside), writing to InfluxDB.
const INFLIGHT_WRITES: usize = 6;
const PARSER_CHANNEL_CAPACITY: usize = 4_096;
const LINE_BATCH_CHANNEL_CAPACITY: usize = 8;

#[derive(Debug, Deserialize)]
pub struct Bus {
    pub iface: Option<String>,
    pub name: Option<String>,
}

#[derive(Debug, Deserialize)]
pub struct Id {
    pub err: Option<bool>,
    pub ext: Option<bool>,
    pub rtr: Option<bool>,
    pub val: Option<i64>,
}

#[derive(Debug, Deserialize)]
pub struct Record {
    pub bus: Option<Bus>,
    pub data: Option<Vec<String>>,
    pub dlc: Option<u8>,
    pub id: Option<Id>,
    pub meas: Option<BTreeMap<String, Value>>,
    pub msg: Option<String>,
    /// epoch seconds (float)
    pub time: Option<f64>,
}

fn parse_line(line: &[u8]) -> Result<Option<Record>> {
    if line.iter().all(|b| b.is_ascii_whitespace()) {
        return Ok(None);
    }

    let rec: Record = serde_json::from_slice(line)
        .with_context(|| format!("invalid JSON: {}", String::from_utf8_lossy(line)))?;
    Ok(Some(rec))
}

fn timestamp_ns(ts_seconds: f64) -> Option<i64> {
    let ts_ns = (ts_seconds * 1_000_000_000.0).round();
    if ts_ns.is_finite() && ts_ns >= i64::MIN as f64 && ts_ns <= i64::MAX as f64 {
        Some(ts_ns as i64)
    } else {
        None
    }
}

fn record_to_point(rec: &Record) -> Option<DataPoint> {
    let measurement = rec.msg.as_deref().unwrap_or("veh_msg");
    let mut builder = DataPoint::builder(measurement);

    if let Some(bus) = &rec.bus {
        if let Some(iface) = &bus.iface {
            builder = builder.tag("iface", iface.clone());
        }
        if let Some(name) = &bus.name {
            builder = builder.tag("bus_name", name.clone());
        }
    }

    if let Some(id) = &rec.id {
        if let Some(v) = id.val {
            builder = builder.tag("can_id", v.to_string());
        }
        if let Some(b) = id.err {
            builder = builder.tag("id_err", if b { "1" } else { "0" });
        }
        if let Some(b) = id.ext {
            builder = builder.tag("id_ext", if b { "1" } else { "0" });
        }
        if let Some(b) = id.rtr {
            builder = builder.tag("id_rtr", if b { "1" } else { "0" });
        }
    }

    let mut has_fields = false;
    if let Some(dlc) = rec.dlc {
        builder = builder.field("dlc", f64::from(dlc));
        has_fields = true;
    }

    if let Some(meas) = &rec.meas {
        for (key, value) in meas {
            let (next_builder, added) = add_json_field(builder, key, value);
            builder = next_builder;
            has_fields |= added;
        }
    }

    if !has_fields {
        return None;
    }

    if let Some(ts_ns) = rec.time.and_then(timestamp_ns) {
        builder = builder.timestamp(ts_ns);
    }

    builder.build().ok()
}

fn add_json_field(builder: DataPointBuilder, key: &str, value: &Value) -> (DataPointBuilder, bool) {
    match value {
        Value::Number(n) => match n.as_f64().filter(|f| f.is_finite()) {
            Some(f) => (builder.field(key.to_owned(), f), true),
            None => (builder.field(key.to_owned(), n.to_string()), true),
        },
        Value::Bool(b) => (
            builder.field(key.to_owned(), if *b { 1.0 } else { 0.0 }),
            true,
        ),
        Value::String(s) => (builder.field(key.to_owned(), s.clone()), true),
        Value::Object(obj) => {
            if let Some(v) = obj
                .get("value")
                .and_then(|x| x.as_f64())
                .filter(|f| f.is_finite())
            {
                (builder.field(key.to_owned(), v), true)
            } else {
                let s = serde_json::to_string(obj).unwrap_or_else(|_| "{}".to_string());
                (builder.field(key.to_owned(), s), true)
            }
        }
        other => (builder.field(key.to_owned(), other.to_string()), true),
    }
}

type WriteTask = JoinHandle<WriteBatchResult>;
type FileTask = JoinHandle<Result<FileIngestResult>>;

struct FileIngestResult {
    ok: usize,
    bad: usize,
}

#[derive(Debug, Default)]
struct WriteStats {
    ok: usize,
    bad: usize,
    batches: usize,
    bytes: usize,
    request_elapsed: Duration,
    wall_elapsed: Duration,
}

struct WriteBatchResult {
    count: usize,
    bytes: usize,
    started_at: Instant,
    finished_at: Instant,
    elapsed: Duration,
    result: Result<()>,
}

struct WriteScheduler {
    client: InfluxClient,
    bucket: String,
    in_flight: FuturesUnordered<WriteTask>,
    stats: WriteStats,
    first_write_started_at: Option<Instant>,
    last_write_finished_at: Option<Instant>,
}

struct BatchWriter {
    scheduler: WriteScheduler,
    batch_size: usize,
    batch_bytes: usize,
    batch_body_capacity: usize,
    batch_body: Vec<u8>,
    line_buf: Vec<u8>,
    batch_points: usize,
}

impl WriteScheduler {
    fn new(client: InfluxClient, bucket: &str) -> Self {
        Self {
            client,
            bucket: bucket.to_owned(),
            in_flight: FuturesUnordered::new(),
            stats: WriteStats::default(),
            first_write_started_at: None,
            last_write_finished_at: None,
        }
    }

    async fn finish(mut self) -> WriteStats {
        while !self.in_flight.is_empty() {
            self.collect_one().await;
        }
        if let (Some(start), Some(end)) = (self.first_write_started_at, self.last_write_finished_at)
        {
            self.stats.wall_elapsed = end.duration_since(start);
        }
        self.stats
    }

    async fn push_body(&mut self, count: usize, body: Vec<u8>) {
        if count == 0 || body.is_empty() {
            return;
        }

        while self.in_flight.len() >= INFLIGHT_WRITES {
            self.collect_one().await;
        }

        let bytes = body.len();
        let client = self.client.clone();
        let bucket = self.bucket.clone();

        self.in_flight.push(tokio::spawn(async move {
            let start = Instant::now();
            let result = write_line_protocol_body(&client, &bucket, body).await;
            let finished_at = Instant::now();
            WriteBatchResult {
                count,
                bytes,
                started_at: start,
                finished_at,
                elapsed: finished_at.duration_since(start),
                result,
            }
        }));
    }

    async fn collect_one(&mut self) {
        match self.in_flight.next().await {
            Some(Ok(result)) => {
                self.stats.batches += 1;
                self.stats.bytes += result.bytes;
                self.stats.request_elapsed += result.elapsed;
                self.first_write_started_at = Some(
                    self.first_write_started_at
                        .map_or(result.started_at, |prev| prev.min(result.started_at)),
                );
                self.last_write_finished_at = Some(
                    self.last_write_finished_at
                        .map_or(result.finished_at, |prev| prev.max(result.finished_at)),
                );

                match result.result {
                    Ok(()) => {
                        self.stats.ok += result.count;
                    }
                    Err(e) => {
                        eprintln!("Influx write error: {e:?}");
                        self.stats.bad += result.count;
                    }
                }
            }
            Some(Err(e)) => {
                eprintln!("Influx write task error: {e}");
                self.stats.bad += 1;
            }
            None => {}
        }
    }
}

impl BatchWriter {
    fn new(client: InfluxClient, bucket: &str, batch_size: usize, batch_bytes: usize) -> Self {
        let batch_size = batch_size.max(1);
        let batch_bytes = batch_bytes.max(1);
        let batch_body_capacity = batch_body_capacity(batch_size, batch_bytes);
        Self {
            scheduler: WriteScheduler::new(client, bucket),
            batch_size,
            batch_bytes,
            batch_body_capacity,
            batch_body: Vec::with_capacity(batch_body_capacity),
            line_buf: Vec::with_capacity(160),
            batch_points: 0,
        }
    }

    async fn push(&mut self, point: DataPoint) -> Result<()> {
        self.line_buf.clear();
        point
            .write_data_point_to(&mut self.line_buf)
            .context("serializing influx data point")?;

        let line_len = self.line_buf.len();
        if self.batch_points > 0
            && self.batch_body.len().saturating_add(line_len) > self.batch_bytes
        {
            self.flush_batch().await;
        }

        self.batch_body.extend_from_slice(&self.line_buf);
        self.batch_points += 1;

        if self.batch_points >= self.batch_size || self.batch_body.len() >= self.batch_bytes {
            self.flush_batch().await;
        }

        Ok(())
    }

    async fn finish(mut self) -> WriteStats {
        self.flush_batch().await;
        self.scheduler.finish().await
    }

    async fn flush_batch(&mut self) {
        if self.batch_points == 0 {
            return;
        }

        let count = self.batch_points;
        self.batch_points = 0;
        let body = std::mem::replace(
            &mut self.batch_body,
            Vec::with_capacity(self.batch_body_capacity),
        );
        self.scheduler.push_body(count, body).await;
    }
}

fn batch_body_capacity(batch_size: usize, batch_bytes: usize) -> usize {
    batch_bytes.min(batch_size.saturating_mul(160).max(1))
}

async fn write_line_protocol_body(
    client: &InfluxClient,
    bucket: &str,
    body: Vec<u8>,
) -> Result<()> {
    client
        .write_line_protocol(&client.org, bucket, body)
        .await
        .context("influx write failed")
}

#[derive(Debug, Default)]
struct ParseStats {
    bad: usize,
    entry_files: usize,
    lines: usize,
    points: usize,
    elapsed: Duration,
}

struct LineProtocolBatch {
    count: usize,
    body: Vec<u8>,
}

fn duration_ms(duration: Duration) -> f64 {
    duration.as_secs_f64() * 1_000.0
}

fn trim_line_ending(line: &mut Vec<u8>) {
    if line.last() == Some(&b'\n') {
        line.pop();
    }
    if line.last() == Some(&b'\r') {
        line.pop();
    }
}

fn parse_tar_gz_blocking(path: PathBuf, tx: mpsc::Sender<DataPoint>) -> Result<ParseStats> {
    let start = Instant::now();
    let file = File::open(&path).with_context(|| format!("open {}", path.display()))?;
    let gz = GzDecoder::new(file);
    let stream = BufReader::new(gz);
    let mut archive = Archive::new(stream);
    let mut stats = ParseStats::default();

    for entry in archive
        .entries()
        .with_context(|| format!("reading archive {}", path.display()))?
    {
        let entry = entry.with_context(|| format!("reading entry in {}", path.display()))?;
        if !entry.header().entry_type().is_file() {
            continue;
        }
        stats.entry_files += 1;

        let mut reader = BufReader::new(entry);
        let mut line = Vec::with_capacity(1024);
        loop {
            line.clear();
            let bytes_read = reader
                .read_until(b'\n', &mut line)
                .with_context(|| format!("reading lines from {}", path.display()))?;
            if bytes_read == 0 {
                break;
            }
            stats.lines += 1;
            trim_line_ending(&mut line);

            match parse_line(&line) {
                Ok(Some(rec)) => {
                    if let Some(point) = record_to_point(&rec) {
                        if tx.blocking_send(point).is_err() {
                            anyhow::bail!("writer stopped while reading {}", path.display());
                        }
                        stats.points += 1;
                    }
                }
                Ok(None) => {}
                Err(_) => {
                    stats.bad += 1;
                }
            }
        }
    }

    stats.elapsed = start.elapsed();
    Ok(stats)
}

fn send_line_protocol_batch(
    tx: &mpsc::Sender<LineProtocolBatch>,
    path: &Path,
    body: &mut Vec<u8>,
    count: &mut usize,
    capacity: usize,
) -> Result<()> {
    if *count == 0 {
        return Ok(());
    }

    let batch = LineProtocolBatch {
        count: *count,
        body: std::mem::replace(body, Vec::with_capacity(capacity)),
    };
    *count = 0;
    if tx.blocking_send(batch).is_err() {
        anyhow::bail!("writer stopped while reading {}", path.display());
    }
    Ok(())
}

fn parse_line_protocol_tar_gz_blocking(
    path: PathBuf,
    tx: mpsc::Sender<LineProtocolBatch>,
    batch_size: usize,
    batch_bytes: usize,
) -> Result<ParseStats> {
    let start = Instant::now();
    let file = File::open(&path).with_context(|| format!("open {}", path.display()))?;
    let gz = GzDecoder::new(file);
    let stream = BufReader::new(gz);
    let mut archive = Archive::new(stream);
    let mut stats = ParseStats::default();
    let batch_size = batch_size.max(1);
    let batch_bytes = batch_bytes.max(1);
    let capacity = batch_body_capacity(batch_size, batch_bytes);
    let mut body = Vec::with_capacity(capacity);
    let mut count = 0usize;

    for entry in archive
        .entries()
        .with_context(|| format!("reading archive {}", path.display()))?
    {
        let entry = entry.with_context(|| format!("reading entry in {}", path.display()))?;
        if !entry.header().entry_type().is_file() {
            continue;
        }
        stats.entry_files += 1;

        let mut reader = BufReader::new(entry);
        let mut line = Vec::with_capacity(256);
        loop {
            line.clear();
            let bytes_read = reader
                .read_until(b'\n', &mut line)
                .with_context(|| format!("reading lines from {}", path.display()))?;
            if bytes_read == 0 {
                break;
            }
            stats.lines += 1;
            trim_line_ending(&mut line);
            if line.iter().all(|b| b.is_ascii_whitespace()) {
                continue;
            }

            let next_len = line.len().saturating_add(1);
            if count > 0
                && (count >= batch_size || body.len().saturating_add(next_len) > batch_bytes)
            {
                send_line_protocol_batch(&tx, &path, &mut body, &mut count, capacity)?;
            }

            body.extend_from_slice(&line);
            body.push(b'\n');
            count += 1;
            stats.points += 1;

            if count >= batch_size || body.len() >= batch_bytes {
                send_line_protocol_batch(&tx, &path, &mut body, &mut count, capacity)?;
            }
        }
    }

    send_line_protocol_batch(&tx, &path, &mut body, &mut count, capacity)?;
    stats.elapsed = start.elapsed();
    Ok(stats)
}

pub async fn ingest_tar_gz(
    client: &InfluxClient,
    bucket: &str,
    path: &Path,
    batch_size: usize,
    batch_bytes: usize,
) -> Result<(usize, usize)> {
    let (tx, mut rx) = mpsc::channel(PARSER_CHANNEL_CAPACITY);
    let parser_path = path.to_path_buf();
    let parser = tokio::task::spawn_blocking(move || parse_tar_gz_blocking(parser_path, tx));

    let mut writer = BatchWriter::new(client.clone(), bucket, batch_size, batch_bytes);
    let mut serialization_bad = 0usize;

    while let Some(point) = rx.recv().await {
        if let Err(e) = writer.push(point).await {
            eprintln!("Influx point serialization error: {e:?}");
            serialization_bad += 1;
        }
    }

    let parser_result = parser.await;
    let write_stats = writer.finish().await;
    let parse_stats = parser_result.context("archive parser task failed")??;
    println!(
        "log: timings '{}' untar_ms={:.3} offload_wall_ms={:.3} offload_request_ms={:.3} untar_files={} lines={} parsed_points={} write_batches={} write_bytes={}",
        path.display(),
        duration_ms(parse_stats.elapsed),
        duration_ms(write_stats.wall_elapsed),
        duration_ms(write_stats.request_elapsed),
        parse_stats.entry_files,
        parse_stats.lines,
        parse_stats.points,
        write_stats.batches,
        write_stats.bytes,
    );
    Ok((
        write_stats.ok,
        parse_stats.bad + serialization_bad + write_stats.bad,
    ))
}

pub async fn ingest_line_protocol_tar_gz(
    client: &InfluxClient,
    bucket: &str,
    path: &Path,
    batch_size: usize,
    batch_bytes: usize,
) -> Result<(usize, usize)> {
    let (tx, mut rx) = mpsc::channel(LINE_BATCH_CHANNEL_CAPACITY);
    let parser_path = path.to_path_buf();
    let parser = tokio::task::spawn_blocking(move || {
        parse_line_protocol_tar_gz_blocking(parser_path, tx, batch_size, batch_bytes)
    });

    let mut writer = WriteScheduler::new(client.clone(), bucket);

    while let Some(batch) = rx.recv().await {
        writer.push_body(batch.count, batch.body).await;
    }

    let parser_result = parser.await;
    let write_stats = writer.finish().await;
    let parse_stats = parser_result.context("line protocol archive parser task failed")??;
    println!(
        "log: timings '{}' format=line_protocol untar_ms={:.3} offload_wall_ms={:.3} offload_request_ms={:.3} untar_files={} lines={} parsed_points={} write_batches={} write_bytes={}",
        path.display(),
        duration_ms(parse_stats.elapsed),
        duration_ms(write_stats.wall_elapsed),
        duration_ms(write_stats.request_elapsed),
        parse_stats.entry_files,
        parse_stats.lines,
        parse_stats.points,
        write_stats.batches,
        write_stats.bytes,
    );
    Ok((write_stats.ok, parse_stats.bad + write_stats.bad))
}

async fn ingest_archive(
    client: &InfluxClient,
    bucket: &str,
    path: &Path,
    batch_size: usize,
    batch_bytes: usize,
) -> Result<(usize, usize)> {
    let fname = path
        .file_name()
        .and_then(|f| f.to_str())
        .unwrap_or_default();
    if fname.ends_with(".lp.tar.gz") {
        ingest_line_protocol_tar_gz(client, bucket, path, batch_size, batch_bytes).await
    } else {
        ingest_tar_gz(client, bucket, path, batch_size, batch_bytes).await
    }
}

async fn ingest_one_file(
    influx: InfluxClient,
    bucket: String,
    path: PathBuf,
    batch_size: usize,
    batch_bytes: usize,
    delete: bool,
) -> Result<FileIngestResult> {
    println!("Start ingesting '{}'", path.display());
    let start_time = Instant::now();
    let (ok, bad) = ingest_archive(&influx, &bucket, &path, batch_size, batch_bytes)
        .await
        .with_context(|| format!("ingesting {}", path.display()))?;
    println!(
        "Finished ingesting '{}', duration: {:?}",
        path.display(),
        start_time.elapsed()
    );

    if delete && bad <= 1 {
        match remove_file(&path) {
            Ok(_) => {
                println!("log: deleted log '{}'", path.display());
            }
            Err(e) => {
                eprintln!("log: failed to delete '{}': {}", path.display(), e);
            }
        }
    }

    Ok(FileIngestResult { ok, bad })
}

fn spawn_file_ingest(
    influx: InfluxClient,
    bucket: String,
    path: PathBuf,
    batch_size: usize,
    batch_bytes: usize,
    delete: bool,
) -> FileTask {
    tokio::spawn(ingest_one_file(
        influx,
        bucket,
        path,
        batch_size,
        batch_bytes,
        delete,
    ))
}

/// Ingest multiple files: only `.tar.gz`; others/missing are skipped with a message.
pub async fn ingest_files(
    url: &str,
    token: &str,
    org: &str,
    bucket: &str,
    files: &[PathBuf],
    batch_size: usize,
    batch_bytes: usize,
    file_concurrency: usize,
    delete: bool,
) -> Result<(usize, usize, usize, usize)> {
    let influx = InfluxClientBuilder::new(url, org, token).build()?;
    let bucket = bucket.to_owned();
    let file_concurrency = file_concurrency.max(1);

    let mut ok = 0usize;
    let mut bad = 0usize;
    let mut skipped = 0usize;
    let mut missing = 0usize;
    let mut ready = Vec::new();

    for p in files {
        if !p.exists() {
            missing += 1;
            eprintln!("Skipping missing file: {}", p.display());
            continue;
        }
        let fname = p.file_name().and_then(|f| f.to_str()).unwrap_or_default();
        if !fname.ends_with(".tar.gz") {
            skipped += 1;
            eprintln!("Skipping non-tar.gz file: {}", p.display());
            continue;
        }

        ready.push(p.clone());
    }

    let mut next_file = ready.into_iter();
    let mut in_flight: FuturesUnordered<FileTask> = FuturesUnordered::new();

    loop {
        while in_flight.len() < file_concurrency {
            let Some(path) = next_file.next() else {
                break;
            };
            in_flight.push(spawn_file_ingest(
                influx.clone(),
                bucket.clone(),
                path,
                batch_size,
                batch_bytes,
                delete,
            ));
        }

        match in_flight.next().await {
            Some(Ok(Ok(result))) => {
                ok += result.ok;
                bad += result.bad;
                println!("Complete. wrote_points={ok} bad_records={bad}");
            }
            Some(Ok(Err(e))) => {
                eprintln!("Error ingesting tar file: {e:?}");
            }
            Some(Err(e)) => {
                eprintln!("File ingest task error: {e}");
            }
            None => break,
        }
    }

    Ok((ok, bad, skipped, missing))
}
