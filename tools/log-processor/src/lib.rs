use std::collections::BTreeMap;
use std::fs::File;
use std::fs::remove_file;
use std::io::{BufRead, BufReader};
use std::path::{Path, PathBuf};
use std::time::Instant;

use anyhow::{Context, Result};
use flate2::read::GzDecoder;
use futures::StreamExt;
use futures::stream;
use futures::stream::FuturesUnordered;
use influxdb2::Client as InfluxClient;
use influxdb2::ClientBuilder as InfluxClientBuilder;
use influxdb2::models::DataPoint;
use influxdb2::models::data_point::DataPointBuilder;
use serde::Deserialize;
use serde_json::Value;
use tar::Archive;
use tokio::task::JoinHandle;

/// Ingest one `.tar.gz` (JSON-lines files inside), writing to InfluxDB.
const INFLIGHT_WRITES: usize = 6;

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

type WriteTask = JoinHandle<(usize, Result<()>)>;

struct BatchWriter {
    client: InfluxClient,
    bucket: String,
    batch_size: usize,
    batch: Vec<DataPoint>,
    in_flight: FuturesUnordered<WriteTask>,
    ok: usize,
    bad: usize,
}

impl BatchWriter {
    fn new(client: InfluxClient, bucket: &str, batch_size: usize) -> Self {
        let batch_size = batch_size.max(1);
        Self {
            client,
            bucket: bucket.to_owned(),
            batch_size,
            batch: Vec::with_capacity(batch_size),
            in_flight: FuturesUnordered::new(),
            ok: 0,
            bad: 0,
        }
    }

    async fn push(&mut self, point: DataPoint) {
        self.batch.push(point);
        if self.batch.len() >= self.batch_size {
            self.flush_batch().await;
        }
    }

    async fn finish(mut self) -> (usize, usize) {
        self.flush_batch().await;
        while !self.in_flight.is_empty() {
            self.collect_one().await;
        }
        (self.ok, self.bad)
    }

    async fn flush_batch(&mut self) {
        if self.batch.is_empty() {
            return;
        }

        while self.in_flight.len() >= INFLIGHT_WRITES {
            self.collect_one().await;
        }

        let points = std::mem::replace(&mut self.batch, Vec::with_capacity(self.batch_size));
        let client = self.client.clone();
        let bucket = self.bucket.clone();

        self.in_flight.push(tokio::spawn(async move {
            let count = points.len();
            let result = client
                .write(&bucket, stream::iter(points))
                .await
                .context("influx write failed");
            (count, result)
        }));
    }

    async fn collect_one(&mut self) {
        match self.in_flight.next().await {
            Some(Ok((count, Ok(())))) => {
                self.ok += count;
            }
            Some(Ok((count, Err(e)))) => {
                eprintln!("Influx write error: {e:?}");
                self.bad += count;
            }
            Some(Err(e)) => {
                eprintln!("Influx write task error: {e}");
                self.bad += 1;
            }
            None => {}
        }
    }
}

pub async fn ingest_tar_gz(
    client: &InfluxClient,
    bucket: &str,
    path: &Path,
    batch_size: usize,
) -> Result<(usize, usize)> {
    let file = File::open(path).with_context(|| format!("open {}", path.display()))?;
    let gz = GzDecoder::new(file);
    let stream = BufReader::new(gz);
    let mut archive = Archive::new(stream);
    let mut writer = BatchWriter::new(client.clone(), bucket, batch_size);
    let mut bad = 0usize;

    for entry in archive
        .entries()
        .with_context(|| format!("reading archive {}", path.display()))?
    {
        let entry = entry.with_context(|| format!("reading entry in {}", path.display()))?;
        if !entry.header().entry_type().is_file() {
            continue;
        }

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
            if line.last() == Some(&b'\n') {
                line.pop();
            }
            if line.last() == Some(&b'\r') {
                line.pop();
            }

            match parse_line(&line) {
                Ok(Some(rec)) => {
                    if let Some(point) = record_to_point(&rec) {
                        writer.push(point).await;
                    }
                }
                Ok(None) => {}
                Err(_) => {
                    bad += 1;
                }
            }
        }
    }

    let (ok, write_bad) = writer.finish().await;
    Ok((ok, bad + write_bad))
}

/// Ingest multiple files: only `.tar.gz`; others/missing are skipped with a message.
pub async fn ingest_files(
    url: &str,
    token: &str,
    org: &str,
    bucket: &str,
    files: &[PathBuf],
    batch_size: usize,
    delete: bool,
) -> Result<(usize, usize, usize, usize)> {
    let influx = InfluxClientBuilder::new(url, org, token).build()?;

    let mut ok = 0usize;
    let mut bad = 0usize;
    let mut skipped = 0usize;
    let mut missing = 0usize;

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

        println!("Start ingesting '{}'", p.display());
        let start_time = Instant::now();
        let (file_ok, file_bad) = match ingest_tar_gz(&influx, bucket, p, batch_size)
            .await
            .with_context(|| format!("ingesting {}", p.display()))
        {
            Ok((file_ok, file_bad)) => (file_ok, file_bad),
            Err(e) => {
                eprintln!("Error ingesting tar file {}: {:?}", p.display(), e);
                continue;
            }
        };
        println!(
            "Finished ingesting '{}', duration: {:?}",
            p.display(),
            start_time.elapsed()
        );
        ok += file_ok;
        bad += file_bad;

        println!("Complete. wrote_points={ok} bad_records={bad}");

        if delete && file_bad <= 1 {
            match remove_file(p) {
                Ok(_) => {
                    println!("log: deleted log '{}'", p.display());
                }
                Err(e) => {
                    eprintln!("log: failed to delete '{}': {}", p.display(), e);
                }
            }
        }
    }

    Ok((ok, bad, skipped, missing))
}
