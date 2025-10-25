use std::collections::BTreeMap;
use std::fs::File;
use std::fs::remove_file;
use std::io::{BufRead, BufReader, Read};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::time::Instant;

use anyhow::{anyhow, Context, Result};
use flate2::read::GzDecoder;
use reqwest;
use reqwest::Url;
use serde::Deserialize;
use serde_json;
use serde_json::Value;
use tar::Archive;
use tokio::sync::Semaphore;
use tokio::sync::mpsc;

/// Ingest one `.tar.gz` (JSON-lines files inside), writing to InfluxDB via HTTP
const INFLIGHT_HTTP: usize = 6;
const PARSE_WORKERS: usize = 2;

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
    // skip blank/whitespace-only lines without allocating
    if line.iter().all(|b| b.is_ascii_whitespace()) {
        return Ok(None);
    }
    let rec: Record = serde_json::from_slice(line)
        .with_context(|| format!("invalid JSON: {}", String::from_utf8_lossy(line)))?;
    Ok(Some(rec))
}

fn as_f64(v: &Value) -> Option<f64> {
    match v {
        Value::Number(n) => n.as_f64(),
        Value::Object(m) => m.get("value").and_then(|x| x.as_f64()),
        Value::Bool(b) => Some(if *b { 1.0 } else { 0.0 }),
        _ => None,
    }
}

// ----- Influx Line Protocol helpers -----

fn escape_measurement(s: &str) -> String {
    s.replace(',', r"\,").replace(' ', r"\ ")
}

fn escape_tag_key_val(s: &str) -> String {
    s.replace(',', r"\,").replace(' ', r"\ ").replace('=', r"\=")
}

fn escape_field_key(s: &str) -> String {
    s.replace(',', r"\,").replace(' ', r"\ ")
}

fn escape_field_string(s: &str) -> String {
    s.replace('\\', r"\\").replace('"', r#"\""#)
}

fn to_line_protocol(
    measurement: &str,
    tags: &BTreeMap<String, String>,
    fields: &BTreeMap<String, Value>,
    ts_ns: Option<i64>,
) -> Option<String> {
    if fields.is_empty() {
        return None;
    }

    // measurement and tags
    let mut line = escape_measurement(measurement);
    for (k, v) in tags {
        let k = escape_tag_key_val(k);
        let v = escape_tag_key_val(v);
        line.push(',');
        line.push_str(&k);
        line.push('=');
        line.push_str(&v);
    }

    // fields
    line.push(' ');
    let mut first = true;
    for (k, v) in fields {
        if !first { line.push(','); }
        first = false;

        let key = escape_field_key(k);
        // numeric/bool -> numeric/bool; strings quoted
        match v {
            Value::Number(n) => {
                // Prefer integer if possible, else float
                if let Some(i) = n.as_i64() {
                    line.push_str(&format!("{key}={}", i));
                } else if let Some(f) = n.as_f64() {
                    // Influx float requires decimal + trailing '0' is fine; appending 'i' is ONLY for integers
                    line.push_str(&format!("{key}={}", f));
                } else {
                    // fallback as string
                    line.push_str(&format!("{key}=\"{}\"", escape_field_string(&n.to_string())));
                }
            }
            Value::Bool(b) => {
                line.push_str(&format!("{key}={}", if *b { "true" } else { "false" }));
            }
            Value::String(s) => {
                line.push_str(&format!("{key}=\"{}\"", escape_field_string(s)));
            }
            Value::Object(obj) => {
                if let Some(v) = obj.get("value").and_then(|x| x.as_f64()) {
                    line.push_str(&format!("{key}={}", v));
                } else {
                    let s = serde_json::to_string(obj).unwrap_or_else(|_| "{}".to_string());
                    line.push_str(&format!("{key}=\"{}\"", escape_field_string(&s)));
                }
            }
            other => {
                line.push_str(&format!("{key}=\"{}\"", escape_field_string(&other.to_string())));
            }
        }
    }

    // timestamp
    if let Some(ns) = ts_ns {
        line.push(' ');
        line.push_str(&ns.to_string());
    }

    Some(line)
}

// Build tags/fields from a Record
fn record_to_line(rec: &Record) -> Option<String> {
    let measurement = rec.msg.as_deref().unwrap_or("veh_msg");

    let mut tags = BTreeMap::<String, String>::new();
    if let Some(bus) = &rec.bus {
        if let Some(iface) = &bus.iface { tags.insert("iface".into(), iface.clone()); }
        if let Some(name)  = &bus.name  { tags.insert("bus_name".into(), name.clone()); }
    }
    if let Some(id) = &rec.id {
        if let Some(v) = id.val { tags.insert("can_id".into(), v.to_string()); }
        if let Some(b) = id.err { tags.insert("id_err".into(), if b { "1" } else { "0" }.into()); }
        if let Some(b) = id.ext { tags.insert("id_ext".into(), if b { "1" } else { "0" }.into()); }
        if let Some(b) = id.rtr { tags.insert("id_rtr".into(), if b { "1" } else { "0" }.into()); }
    }

    let mut fields = BTreeMap::<String, Value>::new();
    if let Some(dlc) = rec.dlc {
        fields.insert("dlc".into(), Value::Number((dlc as i64).into()));
    }
    if let Some(meas) = &rec.meas {
        for (k, v) in meas {
            if let Some(f) = as_f64(v) {
                fields.insert(k.clone(), Value::Number(serde_json::Number::from_f64(f).unwrap()));
            } else if let Some(s) = v.as_str() {
                fields.insert(k.clone(), Value::String(s.to_string()));
            } else {
                // keep as-is (object/bool/etc.) and let to_line_protocol handle
                fields.insert(k.clone(), v.clone());
            }
        }
    }

    let ts_ns = rec.time.map(|ts| (ts * 1_000_000_000.0).round() as i64);
    to_line_protocol(measurement, &tags, &fields, ts_ns)
}

// ----- HTTP write -----
async fn write_batch_http(
    client: &reqwest::Client,
    base_url: &str,
    org: &str,
    bucket: &str,
    token: &str,
    lines: &[String],
) -> Result<()> {
    if lines.is_empty() {
        return Ok(());
    }

    // Ensure using http:// (no TLS)
    if base_url.starts_with("https://") {
        return Err(anyhow!(
            "This build is HTTP-only; use an http:// URL or add a TLS backend."
        ));
    }

    // Build the /api/v2/write URL safely with reqwest::Url
    let mut url = Url::parse(base_url.trim_end_matches('/'))
        .with_context(|| format!("invalid base URL: {base_url}"))?;
    url.set_path("api/v2/write");

    url.query_pairs_mut()
        .append_pair("org", org)
        .append_pair("bucket", bucket)
        .append_pair("precision", "ns");

    let body = lines.join("\n");
    let resp = client
        .post(url.clone())
        .header("Authorization", format!("Token {}", token))
        .header("Content-Type", "text/plain; charset=utf-8")
        .body(body)
        .send()
        .await
        .with_context(|| format!("POST {}", url))?;

    let status = resp.status();
    let text = resp.text().await.unwrap_or_default();
    if !status.is_success() {
        return Err(anyhow!("influx write failed: {} | {}", status, text));
    }

    Ok(())
}

pub async fn ingest_tar_gz(
    client: &reqwest::Client,
    base_url: &str,
    org: &str,
    bucket: &str,
    token: &str,
    path: &Path,
    batch_size: usize,
) -> Result<(usize, usize)> {
    let file = File::open(path).with_context(|| format!("open {}", path.display()))?;
    let gz   = GzDecoder::new(file);
    let stream = BufReader::new(gz);
    let mut archive = Archive::new(stream);

    // Raw line channel from IO → parsers
    let (tx_lines, mut rx_lines) = mpsc::channel::<Vec<u8>>(16_384);

    // Parsed LP channel from parsers → HTTP
    let (tx_lp, mut rx_lp) = mpsc::channel::<String>(16_384);

    // Spawn parser worker
    tokio::spawn(async move {
        while let Some(line) = rx_lines.recv().await {
            if let Ok(Some(rec)) = parse_line(&line) {
                if let Some(lp) = record_to_line(&rec) {
                    // ignore send error if receiver closed
                    let _ = tx_lp.send(lp).await;
                }
            }
        }
    });

    // HTTP writer with limited concurrency
    let sem = Arc::new(Semaphore::new(INFLIGHT_HTTP));
    let mut ok = 0usize;
    let mut bad = 0usize;

    let http = client.clone();
    let base_url = base_url.to_string();
    let org = org.to_string();
    let bucket = bucket.to_string();
    let token = token.to_string();

    // Batch aggregator task
    let writer = tokio::spawn({
        let sem = sem.clone();
        async move {
            let mut batch = Vec::with_capacity(batch_size);
            let mut tasks = vec![];

            while let Some(lp) = rx_lp.recv().await {
                batch.push(lp);
                if batch.len() >= batch_size {
                    let permit = sem.clone().acquire_owned().await.unwrap();
                    let body = std::mem::take(&mut batch);

                    // fire off write concurrently
                    let http = http.clone();
                    let base_url = base_url.clone();
                    let org = org.clone();
                    let bucket = bucket.clone();
                    let token = token.clone();
                    tasks.push(tokio::spawn(async move {
                        let res = write_batch_http(&http, &base_url, &org, &bucket, &token, &body).await;
                        drop(permit);
                        (res, body.len())
                    }));
                }
            }

            // flush final
            if !batch.is_empty() {
                let permit = sem.acquire_owned().await.unwrap();
                let body = std::mem::take(&mut batch);
                let http2 = http.clone();
                let base_url2 = base_url.clone();
                let org2 = org.clone();
                let bucket2 = bucket.clone();
                let token2 = token.clone();
                tasks.push(tokio::spawn(async move {
                    let res = write_batch_http(&http2, &base_url2, &org2, &bucket2, &token2, &body).await;
                    drop(permit);
                    (res, body.len())
                }));
            }

            // collect results
            let mut ok = 0usize;
            let mut bad = 0usize;
            for t in tasks {
                match t.await {
                    Ok((res, n)) => {
                        if res.is_ok() {
                            ok += n;
                        } else {
                            eprintln!("Result error: {:?} {n}", res);
                            bad += n;
                        }
                    }
                    Err(e) => {
                        eprintln!("Task error: {e}");
                        bad += 1;
                    }
                }
            }
            (ok, bad)
        }
    });

    // IO stage: read entries and push lines quickly
    for entry in archive.entries()? {
        let mut entry = entry?;
        if !entry.header().entry_type().is_file() { continue; }

        let mut buf = Vec::with_capacity(256 * 1024);
        entry.read_to_end(&mut buf)?;
        for slice in buf.split(|&b| b == b'\n') {
            if slice.is_empty() { continue; }
            if tx_lines.send(slice.to_vec()).await.is_err() { break; }
        }
    }
    drop(tx_lines); // close, signal parsers to finish

    let (o, b) = writer.await.expect("writer task").clone();
    Ok((o, b))
}

/// Ingest multiple files — only `.tar.gz`; others/missing are skipped with a message.
pub async fn ingest_files(
    url: &str,
    token: &str,
    org: &str,
    bucket: &str,
    files: &[PathBuf],
    batch_size: usize,
    delete: bool,
) -> Result<(usize, usize, usize, usize)> {
    let http = reqwest::Client::builder()
        .pool_max_idle_per_host(8)
        .pool_idle_timeout(std::time::Duration::from_secs(30))
        .tcp_nodelay(true)
        .build()?;

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
        let (o, b) = match ingest_tar_gz(&http, url, org, bucket, token, p, batch_size)
                .await
                .with_context(|| format!("ingesting {}", p.display())) {
            Ok((o, b)) => (o, b),
            Err(e) => {
                eprintln!("Error ingesting tar file {}: {:?}", p.display(), e); 
                continue;
            }
        };
        println!("Finished ingesting '{}', duration: {:?}", p.display(), start_time.elapsed());
        ok += o;
        bad += b;

        println!("Complete. wrote_points={ok} bad_records={bad}");

        if delete && bad <= 1 {
            match remove_file(&p) {
                Ok(_) => {
                    println!("log: deleted log '{}'", p.display());
                }
                Err(e) => {
                    eprintln!("log: failed to delete '{}': {}", p.display(), e);
                    // keep trying next files
                }
            }
        }
    }

    Ok((ok, bad, skipped, missing))
}
