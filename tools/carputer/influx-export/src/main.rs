//! Carputer InfluxDB -> local CSV export (CLI).
//!
//! A small standalone command-line tool that pulls a time-range + signal slice of
//! decoded CAN telemetry out of the carputer's local InfluxDB (org CFR, bucket
//! CarTelemetry) and writes it to CSV for plotting/analysis. The "load signals ->
//! pick -> export" flow is two subcommands (`signals`, `export`).
//!
//! The carputer advertises `_carputer._tcp.local.` over mDNS (see
//! `net-detec-carputer.service` in the firmware), so when neither `--host` nor
//! `--url` is given this tool auto-discovers it on the LAN via the firmware's
//! `net-detec` crate.
//!
//! Telemetry data model (confirmed from firmware source):
//!   - measurement = decoded CAN message name (e.g. FUELRATE), or veh_msg if undecoded
//!   - fields      = the signals; each decoded signal name -> numeric value (plus dlc)
//!   - timestamp   = nanoseconds
//! "Select by signal" therefore means filtering Influx *field keys*. A signal can
//! appear under more than one measurement, so we keep _measurement in the output.

use std::collections::{BTreeSet, HashMap, HashSet};
use std::fs::File;
use std::io::{IsTerminal, Write};
use std::net::IpAddr;
use std::path::PathBuf;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use anyhow::{Context, Result, anyhow, bail};
use clap::{Args, Parser, Subcommand, ValueEnum};
use net_detec::{Client as MdnsClient, DiscoveryFilter};

const DEFAULT_PORT: u16 = 8086;
const DEFAULT_ORG: &str = "CFR";
const DEFAULT_BUCKET: &str = "CarTelemetry";
const DEFAULT_START: &str = "-15m";
const DEFAULT_STOP: &str = "now()";

// Housekeeping field written alongside the real signals; never a useful export column.
const HOUSEKEEPING_FIELDS: &[&str] = &["dlc"];

// Completion-cache settings. On a non-`--dry-run` export/signals run the tool
// refreshes the `--signals` tab-completion cache from the live field keys, but only
// when the existing cache is older than CACHE_MAX_AGE_NS. CACHE_LOOKBACK is the
// window used to enumerate field keys (schema.fieldKeys is metadata, so the width
// is cheap).
const CACHE_LOOKBACK: &str = "-1d";
const CACHE_MAX_AGE_NS: i64 = 24 * 3600 * 1_000_000_000;

/// Read-only `database-viewer-token` (the same value committed in
/// drive-stack/car-dashboard/dashboard.service). Low-sensitivity, read-only; used
/// as the token fallback so `--target` "just works" without `--token`.
const VIEWER_TOKEN: &str =
    "zhaf4VJ5oj42481tTbVM4QKfQLJ87aNnAJUIBeZKi0VRGS1nMcj7OhF7-FLPxF2txYCqCMB_IywQHSfT-8tnAQ==";

/// Which InfluxDB source to talk to. Selecting one auto-fills the mDNS service
/// type used for discovery and the read-only token (see [`Profile`]).
#[derive(Copy, Clone, Debug, PartialEq, Eq, ValueEnum)]
enum Target {
    /// The car's on-board carputer (advertises `_carputer._tcp.local.`).
    Carputer,
    /// The pit-side baseputer.
    Baseputer,
}

/// Connection defaults for a [`Target`]. `--service`/`--token` still override.
struct Profile {
    display: &'static str,
    service_type: &'static str,
    token: &'static str,
}

fn profile(target: Target) -> Profile {
    match target {
        Target::Carputer => Profile {
            display: "carputer",
            service_type: "_carputer._tcp.local.",
            token: VIEWER_TOKEN,
        },
        // NOTE: placeholder service type — the baseputer doesn't advertise yet.
        // Update `service_type` to whatever its firmware ends up publishing; until
        // then, pass `--service <type>` to override. `token` is per-profile so a
        // future divergent baseputer token is a one-line change.
        Target::Baseputer => Profile {
            display: "baseputer",
            service_type: "_baseputer._tcp.local.",
            token: VIEWER_TOKEN,
        },
    }
}

// --------------------------------------------------------------------------- //
// CLI
// --------------------------------------------------------------------------- //

/// Extra `--help` text shown after the auto-generated Options/Commands sections:
/// worked examples, the shared connection flags, and accepted time formats. Keep in
/// sync with the README.
const HELP_AFTER: &str = "\
EXAMPLES:
  # Find the carputer on the LAN (no IP needed)
  influx-export discover

  # List the signals present in the last 7 days
  influx-export signals --start=-7d

  # Export specific signals to CSV
  influx-export export --start=-1h --signals BMSB_packPower,VCFRONT_vehicleSpeed -o data.csv

  # Export every signal in the window
  influx-export export --all --start=-1h -o influx_data.csv

  # Talk to an explicit host (skips mDNS discovery)
  influx-export --host 192.168.100.115 signals

CONNECTION FLAGS (global; valid with any subcommand):
  --target <carputer|baseputer>  source; auto-fills mDNS service type + token [carputer]
  --host <ip/hostname>           explicit address (with --port); skips mDNS
  --url <url>                    full base URL, e.g. http://10.0.0.5:8086; skips mDNS
  --port <n>                     Influx port [8086]
  --token <tok>                  auth token (env INFLUX_TOKEN; viewer token via --target)
  --org <name>                   Influx org [CFR]
  --bucket <name>                Influx bucket [CarTelemetry]
  --interface <name>             NIC for mDNS discovery [all]
  --service <type>               mDNS service type; overrides the --target default
  --discover-timeout <s>         how long to listen for the mDNS advert [4]
  --timeout <s>                  per-query HTTP timeout; raise for big exports [120]

SUBCOMMANDS:
  discover                       browse mDNS and print the carputer's address
  signals  [--start --stop]      list the signal (field) names present in a window
  refresh-signals                (re)build the --signals tab-completion cache (see COMPLETION)
  export   [--start --stop       write selected signals to CSV
            --signals --all -o]    (needs --signals a,b,c OR --all; -o defaults to
                                    influx_export.csv in the tool's own directory)
  completions <bash>             print a shell completion script (see COMPLETION)

TIME VALUES (--start / --stop):
  Flux durations (-15m, -1h30m, 2d), RFC3339 (2026-06-13T10:33:00-04:00),
  epoch seconds, or now(). Durations start with '-', so use the '=' form when a
  value could be read as a flag, e.g. --start=-7d.

CHUNKING (export):
  Exports are split into time windows (default 10m) and queried one window at a
  time, streaming rows to the CSV so big pulls don't hit a single --timeout or
  hold everything in memory. --timeout applies PER QUERY. Control with:
    --chunk <dur>   window size, e.g. 30s, 5m, 1h     --chunks <N>  exactly N windows
    --no-chunk      one query over the whole range    --dry-run     show the planned
                                                                    windows + Flux, no query

  Signal count is unbounded: any selection over 39 signals is automatically split
  into batches of <=39 per window, each a fast pushed-down query, then merged
  (time-ordered) into the one CSV. No manual splitting needed.

PROGRESS (stderr):
  Discovery and each window print progress with a live 'waiting Ns' heartbeat.
  -v/--verbose also echoes the generated Flux for every query.

COMPLETION (bash):
  `completions bash` prints a completion script that tab-completes --signals from a
  local cache of the carputer's field keys. Populate the cache with `refresh-signals`
  (or let export/signals auto-refresh it when >24h old). Install:
    influx-export completions bash > ~/.local/share/bash-completion/completions/influx-export

NOTES:
  * With no --host/--url, the carputer is auto-discovered over mDNS.
";

#[derive(Parser)]
#[command(
    name = "influx-export",
    about = "Export carputer InfluxDB telemetry to CSV. Auto-discovers the carputer over mDNS.",
    after_help = HELP_AFTER,
)]
struct Cli {
    #[command(flatten)]
    conn: ConnArgs,
    #[command(subcommand)]
    cmd: Command,
}

#[derive(Args, Clone)]
struct ConnArgs {
    /// which InfluxDB source to talk to; auto-fills the mDNS service type + token
    #[arg(long, global = true, value_enum, default_value_t = Target::Carputer)]
    target: Target,
    /// carputer IP/hostname (combined with --port). Skips mDNS discovery.
    #[arg(long, global = true)]
    host: Option<String>,
    /// full Influx base URL, e.g. http://10.0.0.5:8086. Skips mDNS discovery.
    #[arg(long, global = true)]
    url: Option<String>,
    /// Influx port (combined with --host or a discovered address)
    #[arg(long, global = true, default_value_t = DEFAULT_PORT)]
    port: u16,
    /// Influx token (env INFLUX_TOKEN; the read-only database-viewer-token works).
    /// The INFLUX_TOKEN fallback is handled in `require_token` (the vendored clap
    /// has no `env` feature).
    #[arg(long, global = true)]
    token: Option<String>,
    /// org
    #[arg(long, global = true, default_value = DEFAULT_ORG)]
    org: String,
    /// bucket
    #[arg(long, global = true, default_value = DEFAULT_BUCKET)]
    bucket: String,
    /// network interface to run mDNS discovery on (default: all)
    #[arg(long, global = true)]
    interface: Option<String>,
    /// mDNS service type to discover (overrides the --target default)
    #[arg(long, global = true)]
    service: Option<String>,
    /// how long to listen for the carputer's mDNS advert, in seconds
    #[arg(long, global = true, default_value_t = 4)]
    discover_timeout: u64,
    /// HTTP timeout for each Influx query, in seconds (raise for large exports)
    #[arg(long, global = true, default_value_t = 120)]
    timeout: u64,
    /// print extra detail (the generated Flux for each query) to stderr
    #[arg(long, short = 'v', global = true)]
    verbose: bool,
}

impl ConnArgs {
    /// Connection defaults for the selected `--target`.
    fn profile(&self) -> Profile {
        profile(self.target)
    }

    /// mDNS service type to browse: explicit `--service` wins, else the target's.
    fn service_type(&self) -> String {
        self.service
            .clone()
            .unwrap_or_else(|| self.profile().service_type.to_string())
    }
}

#[derive(Subcommand)]
enum Command {
    /// Discover the carputer on the LAN over mDNS and print its address.
    Discover,
    /// List the signal (field) names present in a time window.
    Signals(RangeArgs),
    /// Fetch the carputer's signal names and (re)build the `--signals`
    /// tab-completion cache. Run this once (while on the car network) to make
    /// completion work; unlike export/signals it always refreshes.
    #[command(name = "refresh-signals")]
    RefreshSignals,
    /// Export selected signals to a CSV file.
    Export(ExportArgs),
    /// Print a shell completion script (currently: bash).
    Completions {
        /// shell to generate completions for
        #[arg(value_enum)]
        shell: CompletionShell,
    },
    /// Print cached signal names (optionally filtered by prefix), one per line.
    /// Powers `--signals` tab-completion; hidden implementation detail. The cache is
    /// refreshed by export/signals runs (see `completions`).
    #[command(name = "__complete-signals", hide = true)]
    CompleteSignals {
        /// only print signal names starting with this prefix
        prefix: Option<String>,
    },
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, ValueEnum)]
enum CompletionShell {
    Bash,
}

#[derive(Args)]
struct RangeArgs {
    /// range start: Flux duration (-15m), RFC3339, epoch seconds, or now()
    #[arg(long, default_value = DEFAULT_START, allow_hyphen_values = true)]
    start: String,
    /// range stop: Flux duration, RFC3339, epoch seconds, or now()
    #[arg(long, default_value = DEFAULT_STOP, allow_hyphen_values = true)]
    stop: String,
}

#[derive(Args)]
struct ExportArgs {
    #[command(flatten)]
    range: RangeArgs,
    /// signals to export (comma-separated, or repeat the flag)
    #[arg(long, value_delimiter = ',')]
    signals: Vec<String>,
    /// export every signal present in the window (ignores --signals)
    #[arg(long)]
    all: bool,
    /// output CSV path [default: influx_export.csv in the tool's own directory]
    #[arg(short = 'o', long = "out")]
    out: Option<PathBuf>,
    /// time-window size per query, e.g. 10m, 1h, 30s [default: 10m]
    #[arg(long, conflicts_with_all = ["chunks", "no_chunk"])]
    chunk: Option<String>,
    /// split the range into exactly N equal windows
    #[arg(long, conflicts_with = "no_chunk")]
    chunks: Option<u32>,
    /// disable chunking: run the whole range as one query
    #[arg(long)]
    no_chunk: bool,
    /// print the planned windows and their Flux, then exit without querying
    #[arg(long)]
    dry_run: bool,
}

impl ExportArgs {
    /// Resolve the output path: explicit `-o` wins, otherwise default to
    /// `influx_export.csv` in the current directory.
    fn out_path(&self) -> PathBuf {
        self.out.clone().unwrap_or_else(|| PathBuf::from("influx_export.csv"))
    }
}

// --------------------------------------------------------------------------- //
// Flux helpers
// --------------------------------------------------------------------------- //

/// Escape a string as a Flux string literal. serde_json::to_string produces a
/// correctly-escaped double-quoted string, the same trick flux_string_literal
/// uses in drive-stack/car-dashboard/src/lib.rs.
fn flux_string_literal(value: &str) -> String {
    serde_json::to_string(value).unwrap_or_else(|_| "\"\"".to_string())
}

/// Render a user-supplied time as a Flux range() argument.
///
/// Accepts Flux-native values verbatim: relative durations (-15m, -1h30m, 2d),
/// RFC3339 timestamps (2026-06-12T14:00:00Z), epoch seconds, and now().
/// Durations / now() / bare numbers pass through unquoted; RFC3339 is emitted as
/// a Flux time literal via quoting.
fn flux_time(value: &str) -> Result<String> {
    let v = value.trim();
    if v.is_empty() {
        bail!("empty time value");
    }
    if v == "now()" {
        return Ok(v.to_string());
    }
    let first = v.chars().next().unwrap();
    if first == '+' || first == '-' || first.is_ascii_digit() {
        // epoch seconds -> Flux wants an explicit time(); a bare int is a duration.
        // Heuristic mirrors the Python tool: a plain (optionally signed) integer
        // with no duration unit and length >= 6 is treated as epoch seconds;
        // short ints / units are durations.
        let body = if first == '+' || first == '-' { &v[1..] } else { v };
        let all_digits = !body.is_empty() && body.bytes().all(|b| b.is_ascii_digit());
        if all_digits && body.len() >= 6 && first != '-' {
            let n: i64 = v.parse().context("parsing epoch seconds")?;
            return Ok(format!("time(v: {} * 1000000000)", n)); // seconds -> ns
        }
        return Ok(v.to_string()); // duration literal
    }
    // otherwise assume RFC3339 timestamp
    Ok(flux_string_literal(v))
}

// --------------------------------------------------------------------------- //
// Absolute-time resolution (for client-side window planning)
// --------------------------------------------------------------------------- //

/// A range bound resolved to absolute nanoseconds since the Unix epoch, plus
/// whether the user wrote it as `now()` — so the final chunk can keep emitting the
/// live `now()` literal instead of a frozen client-clock timestamp.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
struct TimeBound {
    epoch_ns: i64,
    is_now: bool,
}

fn system_now_ns() -> Result<i64> {
    let d = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .context("system clock is before 1970")?;
    i64::try_from(d.as_nanos()).context("system time out of range")
}

/// Resolve a `--start`/`--stop` value to absolute epoch nanoseconds for window
/// planning, against a caller-supplied "now" (epoch ns). Accepts the same forms as
/// [`flux_time`]: `now()`, relative durations (`-6h`, `-1h30m`), epoch seconds, and
/// RFC3339. Errors (e.g. calendar durations like `2mo`) let the caller fall back to
/// a single un-chunked query. Passing one `now_ns` to both bounds keeps relative
/// ranges exact — `-30m`..`now()` spans exactly 30m, with no sliver window from
/// clock drift between two separate `now()` reads.
fn resolve_time_bound_at(value: &str, now_ns: i64) -> Result<TimeBound> {
    let v = value.trim();
    if v.is_empty() {
        bail!("empty time value");
    }
    if v == "now()" {
        return Ok(TimeBound { epoch_ns: now_ns, is_now: true });
    }
    // RFC3339 date-times contain 'T'/'t' or ':'. They also start with a digit, so
    // detect them before the numeric (epoch/duration) branch below.
    if v.contains(['T', 't', ':']) {
        return Ok(TimeBound { epoch_ns: parse_rfc3339_to_ns(v)?, is_now: false });
    }
    let first = v.chars().next().unwrap();
    if first == '+' || first == '-' || first.is_ascii_digit() {
        let body = if first == '+' || first == '-' { &v[1..] } else { v };
        let all_digits = !body.is_empty() && body.bytes().all(|b| b.is_ascii_digit());
        if all_digits && body.len() >= 6 && first != '-' {
            // epoch seconds (mirrors flux_time's heuristic)
            let secs: i64 = v.parse().context("parsing epoch seconds")?;
            let epoch_ns = secs.checked_mul(1_000_000_000).context("epoch out of range")?;
            return Ok(TimeBound { epoch_ns, is_now: false });
        }
        // relative duration, as an offset from now
        let offset = parse_duration_to_ns(v)?;
        let epoch_ns = now_ns.checked_add(offset).context("time out of range")?;
        return Ok(TimeBound { epoch_ns, is_now: false });
    }
    bail!("unrecognized time value: {:?}", v);
}

/// Parse a Flux relative duration (`-6h`, `-1h30m`, `90m`, `500ms`, `-7d`) to
/// nanoseconds, sign preserved. Supports fixed-length units ns/us/µs/ms/s/m/h/d/w;
/// calendar units (`mo`, `y`) are rejected since they aren't a fixed span.
fn parse_duration_to_ns(value: &str) -> Result<i64> {
    let v = value.trim();
    let (sign, rest) = match v.strip_prefix('-') {
        Some(r) => (-1i64, r),
        None => (1i64, v.strip_prefix('+').unwrap_or(v)),
    };
    if rest.is_empty() {
        bail!("empty duration: {:?}", value);
    }
    let bytes = rest.as_bytes();
    let mut i = 0;
    let mut total: i64 = 0;
    while i < bytes.len() {
        let num_start = i;
        while i < bytes.len() && bytes[i].is_ascii_digit() {
            i += 1;
        }
        if i == num_start {
            bail!("invalid duration {:?}: expected a number", value);
        }
        let num: i64 = rest[num_start..i].parse().context("duration number")?;
        let unit_start = i;
        while i < bytes.len() && !bytes[i].is_ascii_digit() {
            i += 1;
        }
        let unit = &rest[unit_start..i];
        let per: i64 = match unit {
            "ns" => 1,
            "us" | "µs" => 1_000,
            "ms" => 1_000_000,
            "s" => 1_000_000_000,
            "m" => 60 * 1_000_000_000,
            "h" => 3_600 * 1_000_000_000,
            "d" => 86_400 * 1_000_000_000,
            "w" => 604_800 * 1_000_000_000,
            "" => bail!("invalid duration {:?}: missing unit", value),
            other => bail!("unsupported duration unit {:?} in {:?}", other, value),
        };
        let part = num.checked_mul(per).context("duration out of range")?;
        total = total.checked_add(part).context("duration out of range")?;
    }
    Ok(sign * total)
}

/// Days from the Unix epoch for a proleptic-Gregorian date. Howard Hinnant's
/// algorithm: https://howardhinnant.github.io/date_algorithms.html#days_from_civil
fn days_from_civil(y: i64, m: i64, d: i64) -> i64 {
    let y = if m <= 2 { y - 1 } else { y };
    let era = (if y >= 0 { y } else { y - 399 }) / 400;
    let yoe = y - era * 400; // [0, 399]
    let mm = if m > 2 { m - 3 } else { m + 9 }; // Mar=0 .. Feb=11
    let doy = (153 * mm + 2) / 5 + d - 1; // [0, 365]
    let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy; // [0, 146096]
    era * 146097 + doe - 719468
}

/// Split the time-and-offset tail of an RFC3339 string into (clock, offset_secs),
/// where the offset is `Z`, `±HH:MM`, `±HHMM`, or `±HH`.
fn split_rfc3339_offset(rest: &str) -> Result<(&str, i64)> {
    let err = || anyhow!("invalid RFC3339 offset in {:?}", rest);
    if let Some(clock) = rest.strip_suffix(['Z', 'z']) {
        return Ok((clock, 0));
    }
    let idx = rest.rfind(['+', '-']).ok_or_else(err)?;
    let (clock, off) = rest.split_at(idx);
    let sign = if off.starts_with('-') { -1 } else { 1 };
    let off = &off[1..];
    let (oh, om) = match off.split_once(':') {
        Some((h, m)) => (h, m),
        None if off.len() == 4 => (&off[..2], &off[2..]),
        None => (off, "0"),
    };
    let oh: i64 = oh.parse().map_err(|_| err())?;
    let om: i64 = om.parse().map_err(|_| err())?;
    Ok((clock, sign * (oh * 3_600 + om * 60)))
}

/// Parse an RFC3339 timestamp (`YYYY-MM-DDTHH:MM:SS[.fraction][Z|±HH:MM]`) to
/// nanoseconds since the Unix epoch. Hand-rolled to avoid a date-crate dependency.
fn parse_rfc3339_to_ns(value: &str) -> Result<i64> {
    let s = value.trim();
    let err = || anyhow!("invalid RFC3339 timestamp: {:?}", value);
    let (date, rest) = s.split_once(['T', 't', ' ']).ok_or_else(err)?;
    let mut dp = date.split('-');
    let year: i64 = dp.next().ok_or_else(err)?.parse().map_err(|_| err())?;
    let month: i64 = dp.next().ok_or_else(err)?.parse().map_err(|_| err())?;
    let day: i64 = dp.next().ok_or_else(err)?.parse().map_err(|_| err())?;
    if dp.next().is_some() {
        return Err(err());
    }
    let (clock, offset_secs) = split_rfc3339_offset(rest)?;
    let mut tp = clock.split(':');
    let hh: i64 = tp.next().ok_or_else(err)?.parse().map_err(|_| err())?;
    let mm: i64 = tp.next().ok_or_else(err)?.parse().map_err(|_| err())?;
    let sec_part = tp.next().ok_or_else(err)?;
    if tp.next().is_some() {
        return Err(err());
    }
    let (ss, frac_ns): (i64, i64) = match sec_part.split_once('.') {
        Some((int, frac)) => {
            if frac.is_empty() || !frac.bytes().all(|b| b.is_ascii_digit()) {
                return Err(err());
            }
            // pad/truncate the fraction to exactly 9 digits (nanoseconds)
            let f9: String = frac.chars().chain(std::iter::repeat('0')).take(9).collect();
            (int.parse().map_err(|_| err())?, f9.parse().map_err(|_| err())?)
        }
        None => (sec_part.parse().map_err(|_| err())?, 0),
    };
    let days = days_from_civil(year, month, day);
    let secs = days * 86_400 + hh * 3_600 + mm * 60 + ss - offset_secs;
    secs.checked_mul(1_000_000_000)
        .and_then(|n| n.checked_add(frac_ns))
        .context("timestamp out of range")
}

/// List the signal (field) names present in the given window.
fn build_fieldkeys_flux(bucket: &str, start: &str, stop: &str) -> Result<String> {
    Ok(format!(
        "import \"influxdata/influxdb/schema\"\n\
         schema.fieldKeys(\n  \
         bucket: {},\n  \
         start: {},\n  \
         stop: {},\n\
         )\n",
        flux_string_literal(bucket),
        flux_time(start)?,
        flux_time(stop)?,
    ))
}

/// Render an absolute epoch-ns instant as a Flux time expression.
fn flux_time_ns(ns: i64) -> String {
    format!("time(v: {})", ns)
}

/// Max signals per query. The `r._field == a or …` predicate is pushed down to the
/// storage engine (fast: Influx reads only the matching series), but a long chain
/// parses into a deeply nested tree and trips Flux's "Program is nested too deep"
/// limit. So `cmd_export` splits the selection into batches of this size; each
/// batch query stays pushed down. (The alternative — `contains(set: […])` — is one
/// flat call but is NOT pushed down, so Influx scans every series and is slow even
/// for tiny windows; batching lets us avoid it entirely.)
const BATCH_SIZE: usize = 39;

/// Build the export query from already-rendered Flux time expressions for the range
/// bounds (e.g. `time(v: …)`, `now()`, or an RFC3339 literal). Used per window by
/// the chunked export path.
///
/// The output is always the long/tall Influx shape (_time, _measurement, _field,
/// _value), one row per signal sample, which CoastDown's parse_influx.m consumes.
fn build_export_flux_rendered(
    bucket: &str,
    start_expr: &str,
    stop_expr: &str,
    signals: &[String],
) -> Result<String> {
    if signals.is_empty() {
        bail!("no signals selected");
    }
    // Always a pushed-down `or` chain. Callers (cmd_export) pass at most BATCH_SIZE
    // signals, so the chain never gets deep enough to trip Flux's nesting limit and
    // every query reads only the matching series.
    let predicate = signals
        .iter()
        .map(|s| format!("r._field == {}", flux_string_literal(s)))
        .collect::<Vec<_>>()
        .join(" or ");
    let mut query = format!(
        "from(bucket: {})\n  \
         |> range(start: {}, stop: {})\n  \
         |> filter(fn: (r) => {})\n  \
         |> keep(columns: [\"_time\", \"_measurement\", \"_field\", \"_value\"])\n",
        flux_string_literal(bucket),
        start_expr,
        stop_expr,
        predicate,
    );
    query.push_str("  |> sort(columns: [\"_time\"])\n");
    Ok(query)
}

// --------------------------------------------------------------------------- //
// Window planning
// --------------------------------------------------------------------------- //

/// Default per-chunk window when the user doesn't override it: 10 minutes.
const DEFAULT_CHUNK_NS: i64 = 10 * 60 * 1_000_000_000;

/// Decide the window size (ns) from the range span and the chunk flags, in
/// precedence order: `--no-chunk` (whole range) > `--chunks N` > `--chunk <dur>` >
/// default 10m.
fn window_size_ns(
    span_ns: i64,
    no_chunk: bool,
    chunks: Option<u32>,
    chunk: Option<&str>,
) -> Result<i64> {
    if no_chunk {
        return Ok(span_ns.max(1));
    }
    if let Some(n) = chunks {
        if n == 0 {
            bail!("--chunks must be >= 1");
        }
        // ceil(span / n) so exactly n windows cover the range
        let w = (span_ns as i128 + n as i128 - 1) / n as i128;
        return Ok((w as i64).max(1));
    }
    if let Some(d) = chunk {
        let w = parse_duration_to_ns(d)?.abs();
        if w == 0 {
            bail!("--chunk must be a positive duration");
        }
        return Ok(w);
    }
    Ok(DEFAULT_CHUNK_NS)
}

/// Split `[start_ns, stop_ns)` into contiguous half-open windows of at most
/// `window_ns`, ascending in time. Always returns at least one window. Flux
/// `range()` is start-inclusive / stop-exclusive, so adjacent windows don't
/// double-count boundary samples.
fn plan_windows(start_ns: i64, stop_ns: i64, window_ns: i64) -> Result<Vec<(i64, i64)>> {
    if start_ns >= stop_ns {
        bail!("range start must be strictly before stop");
    }
    if window_ns <= 0 {
        bail!("window size must be positive");
    }
    let mut windows = Vec::new();
    let mut w = start_ns;
    while w < stop_ns {
        let end = w.saturating_add(window_ns).min(stop_ns);
        windows.push((w, end));
        w = end;
    }
    Ok(windows)
}

// --------------------------------------------------------------------------- //
// HTTP + CSV
// --------------------------------------------------------------------------- //

/// POST a Flux query and return the annotated-CSV body (error on non-2xx).
///
/// The rest of the tool is synchronous, so we keep this entry point sync and drive
/// the async `reqwest` client on a private current-thread tokio runtime built per
/// call. Queries are few (1 for `signals`, 1-2 for `export`), so the per-call
/// runtime is negligible. (The vendored `reqwest` has no `blocking` feature.)
fn run_query(base_url: &str, org: &str, token: &str, flux: String, timeout_secs: u64) -> Result<String> {
    let url = format!("{}/api/v2/query", base_url.trim_end_matches('/'));
    let rt = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .context("building tokio runtime")?;
    rt.block_on(async {
        let client = reqwest::Client::builder()
            .timeout(Duration::from_secs(timeout_secs))
            .build()
            .context("building HTTP client")?;
        let resp = client
            .post(&url)
            .query(&[("org", org)])
            .header("Authorization", format!("Bearer {}", token))
            .header("Accept", "application/csv")
            .header("Content-Type", "application/vnd.flux")
            .body(flux.into_bytes())
            .send()
            .await
            .with_context(|| format!("POST {}", url))?;
        let status = resp.status();
        if !status.is_success() {
            let text = resp.text().await.unwrap_or_default();
            bail!("Influx query failed ({}): {}", status.as_u16(), text.trim());
        }
        resp.text().await.context("reading Influx response body")
    })
}

/// Parse a single CSV line into its fields, RFC-4180 style: a field may be
/// double-quoted, and a literal `"` inside a quoted field is written `""`.
/// Influx's annotated CSV is one record per line, so single-line parsing is
/// sufficient (no embedded newlines to worry about).
fn parse_csv_line(line: &str) -> Vec<String> {
    let mut fields = Vec::new();
    let mut field = String::new();
    let mut in_quotes = false;
    let mut chars = line.chars().peekable();
    while let Some(c) = chars.next() {
        match c {
            '"' if in_quotes && chars.peek() == Some(&'"') => {
                field.push('"');
                chars.next();
            }
            '"' => in_quotes = !in_quotes,
            ',' if !in_quotes => {
                fields.push(std::mem::take(&mut field));
            }
            _ => field.push(c),
        }
    }
    fields.push(field);
    fields
}

/// Parse Influx annotated CSV into (header, rows).
///
/// Influx returns blocks of CSV: lines starting with '#' are annotations, blank
/// lines separate result tables, and the first non-annotation row of a block is
/// its header. The leading bookkeeping columns (an empty name, 'result', 'table')
/// are dropped from the output. Mirrors parse_influx_csv in the dashboard.
fn parse_annotated_csv(text: &str) -> (Vec<String>, Vec<HashMap<String, String>>) {
    let drop: HashSet<&str> = ["", "result", "table"].into_iter().collect();
    let mut header: Vec<String> = Vec::new();
    let mut rows: Vec<HashMap<String, String>> = Vec::new();
    for raw in text.lines() {
        if raw.is_empty() || raw.starts_with('#') {
            // blank line ends a table; next non-# line is a fresh header
            if raw.is_empty() {
                header.clear();
            }
            continue;
        }
        let fields = parse_csv_line(raw);
        if header.is_empty() {
            header = fields;
            continue;
        }
        let mut record = HashMap::new();
        for (k, v) in header.iter().zip(fields.iter()) {
            if !drop.contains(k.as_str()) {
                record.insert(k.clone(), v.clone());
            }
        }
        rows.push(record);
    }
    let clean_header = header
        .into_iter()
        .filter(|h| !drop.contains(h.as_str()))
        .collect();
    (clean_header, rows)
}

/// Stable column order for the output CSV: the fixed _time, _measurement, _field,
/// _value shape parse_influx.m expects, with any unanticipated columns Influx
/// returned appended.
fn order_columns(header: &[String]) -> Vec<String> {
    let mut cols: Vec<String> = vec![
        "_time".into(),
        "_measurement".into(),
        "_field".into(),
        "_value".into(),
    ];
    for h in header {
        if !cols.contains(h) {
            cols.push(h.clone());
        }
    }
    cols
}

/// Quote a CSV field RFC-4180 style: wrap in `"` and double any embedded `"` when
/// the value contains a comma, quote, CR, or LF; otherwise emit it verbatim.
fn escape_csv_field(s: &str) -> String {
    if s.contains([',', '"', '\n', '\r']) {
        format!("\"{}\"", s.replace('"', "\"\""))
    } else {
        s.to_string()
    }
}

/// Join one record's already-ordered fields into a CSV line.
fn csv_record(fields: &[&str]) -> String {
    fields
        .iter()
        .map(|f| escape_csv_field(f))
        .collect::<Vec<_>>()
        .join(",")
}

/// Write the CSV header row (the ordered column names) to a fresh file.
fn write_csv_header(file: &mut File, cols: &[String]) -> Result<()> {
    let col_refs: Vec<&str> = cols.iter().map(|c| c.as_str()).collect();
    writeln!(file, "{}", csv_record(&col_refs))?;
    Ok(())
}

/// Append data rows to an already-open CSV file using a fixed column order.
/// Returns the number of rows written. Used per window for chunked exports so only
/// one window's rows are held in memory at a time.
fn append_csv_rows(
    file: &mut File,
    cols: &[String],
    rows: &[HashMap<String, String>],
) -> Result<usize> {
    let empty = String::new();
    for r in rows {
        let rec: Vec<&str> = cols
            .iter()
            .map(|c| r.get(c).unwrap_or(&empty).as_str())
            .collect();
        writeln!(file, "{}", csv_record(&rec))?;
    }
    Ok(rows.len())
}

/// Single-shot write: header + all rows to a fresh file. Retained for tests; the
/// export path streams per-window via `write_csv_header` + `append_csv_rows`.
#[cfg(test)]
fn write_csv(
    header: &[String],
    rows: &[HashMap<String, String>],
    out_path: &PathBuf,
) -> Result<usize> {
    let cols = order_columns(header);
    let mut file = File::create(out_path)
        .with_context(|| format!("creating {}", out_path.display()))?;
    write_csv_header(&mut file, &cols)?;
    let n = append_csv_rows(&mut file, &cols, rows)?;
    file.flush()?;
    Ok(n)
}

fn fetch_field_keys(
    base_url: &str,
    org: &str,
    token: &str,
    bucket: &str,
    start: &str,
    stop: &str,
    timeout_secs: u64,
) -> Result<Vec<String>> {
    let flux = build_fieldkeys_flux(bucket, start, stop)?;
    let csv_text = run_query(base_url, org, token, flux, timeout_secs)?;
    let (_, rows) = parse_annotated_csv(&csv_text);
    // schema.fieldKeys returns one row per key in a "_value" column.
    let housekeeping: HashSet<&str> = HOUSEKEEPING_FIELDS.iter().copied().collect();
    let keys: BTreeSet<String> = rows
        .iter()
        .filter_map(|r| r.get("_value"))
        .filter(|v| !v.is_empty() && !housekeeping.contains(v.as_str()))
        .cloned()
        .collect();
    Ok(keys.into_iter().collect())
}

// --------------------------------------------------------------------------- //
// Connection resolution (mDNS discovery via net-detec)
// --------------------------------------------------------------------------- //

/// Discover the selected target over mDNS and return the resolved service.
fn discover_carputer(conn: &ConnArgs) -> Result<net_detec::DiscoveredService> {
    let service = conn.service_type();
    let name = conn.profile().display;
    let client = MdnsClient::new(
        conn.interface.clone(),
        Some(Duration::from_secs(conn.discover_timeout)),
    )
    .map_err(|e| anyhow!("starting mDNS client: {e}"))?;
    client
        .discover(DiscoveryFilter {
            service_type: Some(service.clone()),
            host_name: None,
        })
        .map_err(|e| {
            anyhow!(
                "could not find the {name} via mDNS ({e}). \
                 Pass --host or --url, or check --interface/--service \
                 ({name} advertises {service}).",
            )
        })
}

/// Pick the address to connect to: prefer IPv4, fall back to the first address.
fn pick_address(svc: &net_detec::DiscoveredService) -> Option<IpAddr> {
    svc.addresses
        .iter()
        .find(|a| a.is_ipv4())
        .or_else(|| svc.addresses.first())
        .copied()
}

/// Resolve the Influx base URL from explicit flags or mDNS discovery.
fn resolve_base_url(conn: &ConnArgs) -> Result<String> {
    if let Some(u) = &conn.url {
        return Ok(u.clone());
    }
    if let Some(h) = &conn.host {
        return Ok(format!("http://{}:{}", h, conn.port));
    }
    let name = conn.profile().display;
    eprintln!(
        "Discovering the {name} over mDNS (timeout {}s)…",
        conn.discover_timeout
    );
    let t = Instant::now();
    let svc = discover_carputer(conn)?;
    let addr = pick_address(&svc)
        .ok_or_else(|| anyhow!("{name} found ({}) but it advertised no IP address", svc.host_name))?;
    eprintln!(
        "Found {} '{}' at {} ({}) -> using http://{}:{} for Influx",
        name,
        svc.host_name,
        addr,
        fmt_elapsed(t.elapsed()),
        addr,
        conn.port
    );
    Ok(format!("http://{}:{}", addr, conn.port))
}

/// Resolve the Influx token: `--token` wins, then the `INFLUX_TOKEN` env var,
/// then the selected target's baked-in read-only viewer token. (Env handled by
/// hand rather than clap's `env` feature, which isn't enabled on the firmware's
/// vendored clap.) The profile fallback means it cannot actually fail today, but
/// the `Result` is kept so an empty profile token would surface a clear error.
fn require_token(conn: &ConnArgs) -> Result<String> {
    conn.token
        .clone()
        .or_else(|| std::env::var("INFLUX_TOKEN").ok().filter(|s| !s.is_empty()))
        .or_else(|| {
            let t = conn.profile().token;
            (!t.is_empty()).then(|| t.to_string())
        })
        .ok_or_else(|| anyhow!("no token; pass --token or set INFLUX_TOKEN (the read-only database-viewer-token works)"))
}

// --------------------------------------------------------------------------- //
// Progress reporting
// --------------------------------------------------------------------------- //

/// Format a Duration compactly: "0.8s", "4.3s", or "1m02s" past a minute.
fn fmt_elapsed(d: Duration) -> String {
    let secs = d.as_secs_f64();
    if secs < 60.0 {
        format!("{:.1}s", secs)
    } else {
        let s = d.as_secs();
        format!("{}m{:02}s", s / 60, s % 60)
    }
}

/// Inverse of [`days_from_civil`]: (year, month, day) for days since the epoch.
fn civil_from_days(z: i64) -> (i64, i64, i64) {
    let z = z + 719468;
    let era = (if z >= 0 { z } else { z - 146096 }) / 146097;
    let doe = z - era * 146097; // [0, 146096]
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
    let y = yoe + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100); // [0, 365]
    let mp = (5 * doy + 2) / 153; // [0, 11]
    let d = doy - (153 * mp + 2) / 5 + 1; // [1, 31]
    let m = if mp < 10 { mp + 3 } else { mp - 9 }; // [1, 12]
    (if m <= 2 { y + 1 } else { y }, m, d)
}

/// Render absolute epoch nanoseconds as a UTC RFC3339 timestamp (second precision),
/// for human-readable window labels in progress output.
fn fmt_instant_utc(ns: i64) -> String {
    let secs = ns.div_euclid(1_000_000_000);
    let days = secs.div_euclid(86_400);
    let tod = secs.rem_euclid(86_400);
    let (y, m, d) = civil_from_days(days);
    format!(
        "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z",
        y,
        m,
        d,
        tod / 3_600,
        (tod % 3_600) / 60,
        tod % 60
    )
}

/// Run a blocking operation while showing a live "… waiting Ns" heartbeat on
/// stderr (animated with `\r` only on a TTY; periodic lines otherwise). Returns the
/// operation's result and elapsed time; the heartbeat thread is always stopped
/// before returning.
fn run_with_progress<T>(label: &str, f: impl FnOnce() -> T) -> (T, Duration) {
    let start = Instant::now();
    let stop = Arc::new(AtomicBool::new(false));
    let animate = std::io::stderr().is_terminal();
    let handle = {
        let stop = Arc::clone(&stop);
        let label = label.to_string();
        std::thread::spawn(move || {
            let mut shown = false;
            let mut last_logged = 0u64;
            while !stop.load(Ordering::Relaxed) {
                std::thread::sleep(Duration::from_millis(200));
                if stop.load(Ordering::Relaxed) {
                    break;
                }
                let elapsed = start.elapsed().as_secs();
                if animate {
                    eprint!("\r  {label}… waiting {elapsed}s");
                    let _ = std::io::stderr().flush();
                    shown = true;
                } else if elapsed >= 5 && elapsed != last_logged && elapsed % 5 == 0 {
                    eprintln!("  {label}… waiting {elapsed}s");
                    last_logged = elapsed;
                }
            }
            if animate && shown {
                eprint!("\r\x1b[K"); // clear the heartbeat line
                let _ = std::io::stderr().flush();
            }
        })
    };
    let result = f();
    stop.store(true, Ordering::Relaxed);
    let _ = handle.join();
    (result, start.elapsed())
}

// --------------------------------------------------------------------------- //
// Signal-name completion cache
// --------------------------------------------------------------------------- //

/// Per-target completion cache path:
/// `${XDG_CACHE_HOME:-$HOME/.cache}/influx-export/signals-<target>.yaml`.
fn cache_path(target: Target) -> Result<PathBuf> {
    let base = std::env::var_os("XDG_CACHE_HOME")
        .map(PathBuf::from)
        .filter(|p| !p.as_os_str().is_empty())
        .or_else(|| std::env::var_os("HOME").map(|h| PathBuf::from(h).join(".cache")))
        .ok_or_else(|| anyhow!("cannot locate a cache dir (set HOME or XDG_CACHE_HOME)"))?;
    Ok(base
        .join("influx-export")
        .join(format!("signals-{}.yaml", profile(target).display)))
}

/// Write the field-key list to the cache as a small flat YAML doc (we own the
/// format, so it's hand-written — see `parse_cache_signals` for the reader).
fn write_signal_cache(path: &PathBuf, keys: &[String], target: Target) -> Result<()> {
    if let Some(dir) = path.parent() {
        std::fs::create_dir_all(dir).with_context(|| format!("creating {}", dir.display()))?;
    }
    let mut out = format!(
        "# influx-export signal cache — generated {} from {} (lookback {})\n",
        fmt_instant_utc(system_now_ns()?),
        profile(target).display,
        CACHE_LOOKBACK,
    );
    out.push_str("signals:\n");
    for k in keys {
        out.push_str("  - ");
        out.push_str(k);
        out.push('\n');
    }
    std::fs::write(path, out).with_context(|| format!("writing {}", path.display()))?;
    Ok(())
}

/// Read signal names from a cache file (missing/unreadable -> empty).
fn read_signal_cache(path: &PathBuf) -> Vec<String> {
    std::fs::read_to_string(path)
        .map(|t| parse_cache_signals(&t))
        .unwrap_or_default()
}

/// Parse `  - <name>` list items from cache YAML text.
fn parse_cache_signals(text: &str) -> Vec<String> {
    text.lines()
        .filter_map(|l| l.strip_prefix("  - "))
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty())
        .collect()
}

/// Epoch-ns the cache was generated, parsed from its `generated <RFC3339>` header
/// (None if missing/unparseable).
fn read_cache_generated(path: &PathBuf) -> Option<i64> {
    parse_cache_generated(&std::fs::read_to_string(path).ok()?)
}

fn parse_cache_generated(text: &str) -> Option<i64> {
    // First line: "# influx-export signal cache — generated <RFC3339> from ..."
    let stamp = text.lines().next()?.split("generated ").nth(1)?.split_whitespace().next()?;
    parse_rfc3339_to_ns(stamp).ok()
}

/// True when a cache generated at `generated_ns` is younger than CACHE_MAX_AGE_NS
/// relative to `now_ns` (a future timestamp from clock skew counts as not fresh).
fn cache_is_fresh(generated_ns: i64, now_ns: i64) -> bool {
    let age = now_ns.saturating_sub(generated_ns);
    (0..CACHE_MAX_AGE_NS).contains(&age)
}

/// Force-refresh the completion cache from the live field keys, regardless of the
/// existing cache's age. Returns the number of signals written — 0 when the lookback
/// window had no data, in which case any existing cache is left intact so a bad
/// window/host can't wipe a good cache. Shared by `refresh-signals` and by
/// `maybe_refresh_signal_cache`'s staleness path.
fn refresh_signal_cache(conn: &ConnArgs, base_url: &str, token: &str) -> Result<usize> {
    let path = cache_path(conn.target)?;
    let (keys, elapsed) = run_with_progress("refreshing completion cache", || {
        fetch_field_keys(
            base_url,
            &conn.org,
            token,
            &conn.bucket,
            CACHE_LOOKBACK,
            DEFAULT_STOP,
            conn.timeout,
        )
    });
    let keys = keys?;
    if keys.is_empty() {
        eprintln!(
            "completion cache not refreshed: no signals in the last {} (kept existing cache)",
            CACHE_LOOKBACK
        );
        return Ok(0);
    }
    write_signal_cache(&path, &keys, conn.target)?;
    eprintln!(
        "refreshed completion cache: {} signal(s) -> {} ({})",
        keys.len(),
        path.display(),
        fmt_elapsed(elapsed)
    );
    Ok(keys.len())
}

/// Refresh the completion cache from the live field keys, but only if the existing
/// cache is missing or >= CACHE_MAX_AGE_NS old. Best-effort and chatty; callers warn
/// on error but never fail the run because of it. An empty result (no recent data)
/// leaves any existing cache intact.
fn maybe_refresh_signal_cache(conn: &ConnArgs, base_url: &str, token: &str) -> Result<()> {
    let path = cache_path(conn.target)?;
    if let Some(generated) = read_cache_generated(&path) {
        let now = system_now_ns()?;
        if cache_is_fresh(generated, now) {
            let age = now.saturating_sub(generated).max(0) as u64;
            eprintln!("completion cache is {} old (fresh)", fmt_elapsed(Duration::from_nanos(age)));
            return Ok(());
        }
    }
    refresh_signal_cache(conn, base_url, token)?;
    Ok(())
}

// --------------------------------------------------------------------------- //
// Subcommands
// --------------------------------------------------------------------------- //
fn cmd_discover(conn: &ConnArgs) -> Result<()> {
    let svc = discover_carputer(conn)?;
    println!("host:    {}", svc.host_name);
    println!("service: {}", svc.fullname);
    let addrs = svc
        .addresses
        .iter()
        .map(|a| a.to_string())
        .collect::<Vec<_>>()
        .join(", ");
    println!("address: {}", if addrs.is_empty() { "(none)".into() } else { addrs });
    println!("adv port:{}", svc.port);
    if !svc.txt.is_empty() {
        let mut txt: Vec<_> = svc.txt.iter().collect();
        txt.sort_by(|a, b| a.0.cmp(b.0));
        for (k, v) in txt {
            println!("txt:     {}={}", k, v);
        }
    }
    if let Some(addr) = pick_address(&svc) {
        println!();
        println!("Influx would be reached at http://{}:{}", addr, conn.port);
    }
    Ok(())
}

fn cmd_signals(conn: &ConnArgs, range: &RangeArgs) -> Result<()> {
    let token = require_token(conn)?;
    let base_url = resolve_base_url(conn)?;
    let keys = fetch_field_keys(
        &base_url, &conn.org, &token, &conn.bucket, &range.start, &range.stop, conn.timeout,
    )?;
    if keys.is_empty() {
        eprintln!(
            "No signals found in that window. Check the range overlaps recorded \
             data, and that the host/token are correct."
        );
    } else {
        eprintln!("{} signal(s) available in this window:", keys.len());
        for k in &keys {
            println!("{}", k);
        }
    }
    if let Err(e) = maybe_refresh_signal_cache(conn, &base_url, &token) {
        eprintln!("warning: couldn't update completion cache: {e}");
    }
    Ok(())
}

/// `refresh-signals`: (re)build the `--signals` tab-completion cache from the
/// carputer's live field keys, unconditionally (ignores the 24h freshness gate that
/// export/signals runs use). This is the one command whose only job is to make tab
/// completion work.
fn cmd_refresh_signals(conn: &ConnArgs) -> Result<()> {
    let token = require_token(conn)?;
    let base_url = resolve_base_url(conn)?;
    refresh_signal_cache(conn, &base_url, &token)?;
    Ok(())
}

/// One query's worth of work: already-rendered Flux range bounds plus a
/// human-readable label for progress/dry-run output.
struct WindowSpec {
    start_expr: String,
    stop_expr: String,
    label: String,
}

/// A single un-windowed query over the original range strings (used by
/// `--no-chunk` and as the fallback when bounds can't be resolved for chunking).
fn single_spec(start: &str, stop: &str) -> Result<WindowSpec> {
    Ok(WindowSpec {
        start_expr: flux_time(start)?,
        stop_expr: flux_time(stop)?,
        label: format!("[{start}, {stop}]"),
    })
}

/// Render planned epoch windows into queryable specs. The final window keeps the
/// live `now()` literal as its stop when the user's `--stop` was `now()`, so the
/// newest samples aren't truncated by client/server clock skew.
fn build_window_specs(windows: &[(i64, i64)], stop_is_now: bool) -> Vec<WindowSpec> {
    let last = windows.len().saturating_sub(1);
    windows
        .iter()
        .enumerate()
        .map(|(i, &(w0, w1))| {
            let final_now = stop_is_now && i == last;
            let (stop_expr, stop_label) = if final_now {
                ("now()".to_string(), "now()".to_string())
            } else {
                (flux_time_ns(w1), fmt_instant_utc(w1))
            };
            WindowSpec {
                start_expr: flux_time_ns(w0),
                stop_expr,
                label: format!("[{}, {}]", fmt_instant_utc(w0), stop_label),
            }
        })
        .collect()
}

/// Build the list of windows to query for an export. No network access.
fn plan_export_windows(args: &ExportArgs) -> Result<Vec<WindowSpec>> {
    let start = &args.range.start;
    let stop = &args.range.stop;
    if args.no_chunk {
        return Ok(vec![single_spec(start, stop)?]);
    }
    // Resolve both bounds against a single "now" so relative ranges are exact.
    let now_ns = system_now_ns()?;
    match (resolve_time_bound_at(start, now_ns), resolve_time_bound_at(stop, now_ns)) {
        (Ok(a), Ok(b)) => {
            let span = b.epoch_ns - a.epoch_ns;
            if span <= 0 {
                bail!("range start ({start}) is not before stop ({stop})");
            }
            let window = window_size_ns(span, false, args.chunks, args.chunk.as_deref())?;
            let windows = plan_windows(a.epoch_ns, b.epoch_ns, window)?;
            Ok(build_window_specs(&windows, b.is_now))
        }
        (a, b) => {
            // A bound couldn't be resolved for chunking (e.g. a calendar duration
            // like 2mo). Fall back to one query over the original range.
            let why = a.err().or(b.err()).map(|e| e.to_string()).unwrap_or_default();
            eprintln!("note: not chunking ({why}); running a single query over the full range");
            Ok(vec![single_spec(start, stop)?])
        }
    }
}

/// Resolve the signal selection. `--all` lists the window's field keys live
/// (shown with progress); `--signals` is deduped, order preserved. Under
/// `--dry-run` the field-key query is skipped and a placeholder is used.
fn resolve_export_signals(
    conn: &ConnArgs,
    args: &ExportArgs,
    conn_info: Option<&(String, String)>,
) -> Result<Vec<String>> {
    if args.all {
        let Some((base_url, token)) = conn_info else {
            eprintln!("note: --dry-run with --all can't list field keys offline; using a placeholder signal");
            return Ok(vec!["<all-signals>".to_string()]);
        };
        let (keys, elapsed) = run_with_progress("fetching signal list", || {
            fetch_field_keys(
                base_url,
                &conn.org,
                token,
                &conn.bucket,
                &args.range.start,
                &args.range.stop,
                conn.timeout,
            )
        });
        let keys = keys?;
        if keys.is_empty() {
            bail!("--all: no signals found in that window");
        }
        eprintln!("  {} signal(s) to export ({})", keys.len(), fmt_elapsed(elapsed));
        Ok(keys)
    } else {
        if args.signals.is_empty() {
            bail!("select signals with --signals a,b,c (or use --all)");
        }
        let mut seen = HashSet::new();
        Ok(args
            .signals
            .iter()
            .map(|s| s.trim().to_string())
            .filter(|s| !s.is_empty() && seen.insert(s.clone()))
            .collect())
    }
}

fn cmd_export(conn: &ConnArgs, args: &ExportArgs) -> Result<()> {
    let out = args.out_path();
    let specs = plan_export_windows(args)?;

    // Resolve the connection up front (skipped entirely under --dry-run so it
    // never touches the network / mDNS).
    let conn_info: Option<(String, String)> = if args.dry_run {
        None
    } else {
        let token = require_token(conn)?;
        let base_url = resolve_base_url(conn)?;
        Some((base_url, token))
    };

    let signals = resolve_export_signals(conn, args, conn_info.as_ref())?;
    // Split the selection into <=BATCH_SIZE groups so every query stays on the fast
    // pushed-down `or`-chain path (see BATCH_SIZE). <=39 signals -> one batch.
    let batches: Vec<&[String]> = signals.chunks(BATCH_SIZE).collect();
    let nbatch = batches.len();

    if args.dry_run {
        println!("Planned {} window(s) x {} batch(es) over [{}, {}]:", specs.len(), nbatch, args.range.start, args.range.stop);
        for (i, sp) in specs.iter().enumerate() {
            for (j, batch) in batches.iter().enumerate() {
                let flux = build_export_flux_rendered(&conn.bucket, &sp.start_expr, &sp.stop_expr, batch)?;
                println!("\n# window {}/{} batch {}/{}  {}", i + 1, specs.len(), j + 1, nbatch, sp.label);
                print!("{flux}");
            }
        }
        println!("\n(dry run: no queries sent, nothing written to {})", out.display());
        eprintln!("dry-run: completion cache not refreshed");
        return Ok(());
    }

    let (base_url, token) = conn_info.expect("connection resolved when not dry-run");
    // The export query keeps exactly _time,_measurement,_field,_value, so the
    // column set is fixed and stable across windows — write the header once and
    // append each window's rows (memory bounded to one window).
    let cols = order_columns(&[]);
    let mut file = File::create(&out).with_context(|| format!("creating {}", out.display()))?;
    write_csv_header(&mut file, &cols)?;

    let total = specs.len();
    eprintln!(
        "Exporting {} signal(s) in {} batch(es) across {} window(s)…",
        signals.len(),
        nbatch,
        total
    );
    let export_start = Instant::now();
    let mut total_rows = 0usize;
    for (i, sp) in specs.iter().enumerate() {
        // Gather every batch's rows for this window, then sort by _time so the
        // window stays time-ordered even though it arrived across several queries.
        let mut window_rows: Vec<HashMap<String, String>> = Vec::new();
        for (j, batch) in batches.iter().enumerate() {
            let flux = build_export_flux_rendered(&conn.bucket, &sp.start_expr, &sp.stop_expr, batch)?;
            if conn.verbose {
                eprintln!("--- window {}/{} batch {}/{} Flux ---\n{}", i + 1, total, j + 1, nbatch, flux.trim_end());
            }
            let label = format!("window {}/{} batch {}/{} {}", i + 1, total, j + 1, nbatch, sp.label);
            let (res, elapsed) =
                run_with_progress(&label, || run_query(&base_url, &conn.org, &token, flux, conn.timeout));
            let csv_text = res.with_context(|| format!("querying {} (batch {}/{})", sp.label, j + 1, nbatch))?;
            let (_header, rows) = parse_annotated_csv(&csv_text);
            let n = rows.len();
            window_rows.extend(rows);
            eprintln!("  {label} -> {n} row(s) ({})", fmt_elapsed(elapsed));
        }
        sort_rows_by_time(&mut window_rows);
        let n = append_csv_rows(&mut file, &cols, &window_rows)?;
        total_rows += n;
        if nbatch > 1 {
            eprintln!("  window {}/{} total -> {} row(s)", i + 1, total, n);
        }
    }
    file.flush()?;
    eprintln!(
        "Wrote {} row(s) across {} window(s) to {} ({})",
        total_rows,
        total,
        out.display(),
        fmt_elapsed(export_start.elapsed())
    );
    if let Err(e) = maybe_refresh_signal_cache(conn, &base_url, &token) {
        eprintln!("warning: couldn't update completion cache: {e}");
    }
    Ok(())
}

/// Stable-sort rows ascending by their `_time` field. Influx's annotated CSV emits
/// `_time` as RFC3339 UTC (`…Z`), whose lexical order is chronological, so a string
/// compare suffices — no timestamp parsing needed. Keeps a window time-ordered
/// after its rows are merged from several batch queries.
fn sort_rows_by_time(rows: &mut [HashMap<String, String>]) {
    rows.sort_by(|a, b| {
        let empty = String::new();
        a.get("_time").unwrap_or(&empty).cmp(b.get("_time").unwrap_or(&empty))
    });
}

// --------------------------------------------------------------------------- //
// Shell completion
// --------------------------------------------------------------------------- //

/// `__complete-signals [prefix]`: print cached signal names (optionally filtered)
/// one per line — read from the carputer cache, refreshed by export/signals runs.
fn cmd_complete_signals(prefix: Option<&str>) -> Result<()> {
    let names = read_signal_cache(&cache_path(Target::Carputer)?);
    if names.is_empty() {
        eprintln!(
            "no signal cache yet; run an export or `signals` while connected to refresh it"
        );
        return Ok(());
    }
    let pfx = prefix.unwrap_or("");
    for name in names {
        if name.starts_with(pfx) {
            println!("{name}");
        }
    }
    Ok(())
}

/// Hand-written bash completion script. Completes subcommands, common flags, and
/// `--signals` values (from the cache, including the comma-separated list form).
const BASH_COMPLETION: &str = r#"# bash completion for influx-export
# Install:  influx-export completions bash > ~/.local/share/bash-completion/completions/influx-export
#    or:    source <(influx-export completions bash)   # in ~/.bashrc
#
# Signal names come from a local cache that export/signals runs refresh (daily)
# from the carputer's database, via `influx-export __complete-signals`.
_influx_export() {
    local cur prev words cword
    _init_completion -n : 2>/dev/null || {
        cur="${COMP_WORDS[COMP_CWORD]}"
        prev="${COMP_WORDS[COMP_CWORD-1]}"
    }

    local subcommands="discover signals refresh-signals export completions"
    local global_flags="--target --host --url --port --token --org --bucket --interface --service --discover-timeout --timeout --verbose --help"

    # Complete the value for --signals (incl. comma-separated lists).
    if [[ "$prev" == "--signals" ]]; then
        # Split the current word on the last comma: keep everything up to and
        # including it as a fixed prefix, complete only the trailing stub.
        local prefix="" stub="$cur"
        if [[ "$cur" == *,* ]]; then
            prefix="${cur%,*},"
            stub="${cur##*,}"
        fi
        local cands
        cands="$(influx-export __complete-signals "$stub" 2>/dev/null)"
        local c
        COMPREPLY=()
        while IFS= read -r c; do
            [[ -z "$c" ]] && continue
            COMPREPLY+=( "${prefix}${c}" )
        done <<< "$cands"
        compopt -o nospace 2>/dev/null
        return 0
    fi

    case "$cur" in
        -*)
            COMPREPLY=( $(compgen -W "$global_flags --signals --all --out --start --stop --chunk --chunks --no-chunk --dry-run" -- "$cur") )
            return 0
            ;;
    esac

    # First non-flag word -> subcommand.
    local i cmd=""
    for (( i=1; i < COMP_CWORD; i++ )); do
        case "${COMP_WORDS[i]}" in
            -*) ;;
            discover|signals|refresh-signals|export|completions) cmd="${COMP_WORDS[i]}"; break ;;
        esac
    done

    case "$cmd" in
        completions) COMPREPLY=( $(compgen -W "bash" -- "$cur") ) ;;
        export)      COMPREPLY=( $(compgen -W "--signals --all --out --start --stop --chunk --chunks --no-chunk --dry-run $global_flags" -- "$cur") ) ;;
        signals)     COMPREPLY=( $(compgen -W "--start --stop $global_flags" -- "$cur") ) ;;
        "")          COMPREPLY=( $(compgen -W "$subcommands" -- "$cur") ) ;;
    esac
    return 0
}
complete -F _influx_export influx-export
"#;

fn cmd_completions(shell: CompletionShell) -> Result<()> {
    match shell {
        CompletionShell::Bash => print!("{}", BASH_COMPLETION),
    }
    Ok(())
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    match &cli.cmd {
        Command::Discover => cmd_discover(&cli.conn),
        Command::Signals(range) => cmd_signals(&cli.conn, range),
        Command::RefreshSignals => cmd_refresh_signals(&cli.conn),
        Command::Export(args) => cmd_export(&cli.conn, args),
        Command::Completions { shell } => cmd_completions(*shell),
        Command::CompleteSignals { prefix } => cmd_complete_signals(prefix.as_deref()),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_duration_to_ns_units_and_compounds() {
        assert_eq!(parse_duration_to_ns("-6h").unwrap(), -21_600_000_000_000);
        assert_eq!(parse_duration_to_ns("-1h30m").unwrap(), -5_400_000_000_000);
        assert_eq!(parse_duration_to_ns("90m").unwrap(), 5_400_000_000_000);
        assert_eq!(parse_duration_to_ns("-7d").unwrap(), -604_800_000_000_000);
        assert_eq!(parse_duration_to_ns("500ms").unwrap(), 500_000_000);
        assert_eq!(parse_duration_to_ns("-500ms").unwrap(), -500_000_000);
        assert!(parse_duration_to_ns("2mo").is_err()); // calendar unit rejected
        assert!(parse_duration_to_ns("5").is_err()); // missing unit
    }

    #[test]
    fn days_from_civil_spot_checks() {
        assert_eq!(days_from_civil(1970, 1, 1), 0);
        assert_eq!(days_from_civil(1970, 1, 2), 1);
        assert_eq!(days_from_civil(2000, 1, 1), 10957);
        assert_eq!(days_from_civil(2000, 2, 29), 11016); // leap day exists
    }

    #[test]
    fn civil_from_days_round_trips() {
        for &(y, m, d) in &[(1970, 1, 1), (2000, 2, 29), (2026, 6, 13), (1969, 12, 31)] {
            let z = days_from_civil(y, m, d);
            assert_eq!(civil_from_days(z), (y, m, d));
        }
        // epoch-ns formatting (UTC, second precision)
        assert_eq!(fmt_instant_utc(0), "1970-01-01T00:00:00Z");
        assert_eq!(fmt_instant_utc(946_684_800_000_000_000), "2000-01-01T00:00:00Z");
    }

    #[test]
    fn parse_rfc3339_to_ns_forms() {
        assert_eq!(parse_rfc3339_to_ns("1970-01-01T00:00:00Z").unwrap(), 0);
        assert_eq!(parse_rfc3339_to_ns("1970-01-01T00:00:01Z").unwrap(), 1_000_000_000);
        assert_eq!(parse_rfc3339_to_ns("1970-01-02T00:00:00Z").unwrap(), 86_400_000_000_000);
        assert_eq!(
            parse_rfc3339_to_ns("2000-01-01T00:00:00Z").unwrap(),
            946_684_800_000_000_000
        );
        // -01:00 offset: local is behind UTC, so the instant is 1h later.
        assert_eq!(
            parse_rfc3339_to_ns("2000-01-01T00:00:00-01:00").unwrap(),
            946_688_400_000_000_000
        );
        // fractional seconds (half a second)
        assert_eq!(parse_rfc3339_to_ns("1970-01-01T00:00:00.5Z").unwrap(), 500_000_000);
        assert!(parse_rfc3339_to_ns("2000-01-01 garbage").is_err());
    }

    #[test]
    fn plan_windows_splits_and_clamps() {
        let min = 60 * 1_000_000_000i64;
        // 6h range, 1h windows -> 6 windows, last clamped exactly to stop.
        let start = 0;
        let stop = 6 * 60 * min;
        let w = plan_windows(start, stop, 60 * min).unwrap();
        assert_eq!(w.len(), 6);
        assert_eq!(w[0], (0, 60 * min));
        assert_eq!(w[5], (5 * 60 * min, 6 * 60 * min));
        // contiguous, no gaps/overlaps
        for pair in w.windows(2) {
            assert_eq!(pair[0].1, pair[1].0);
        }
        // window larger than range -> single window.
        assert_eq!(plan_windows(0, 5 * min, 60 * min).unwrap(), vec![(0, 5 * min)]);
        // non-even division -> last window short.
        let w = plan_windows(0, 25 * min, 10 * min).unwrap();
        assert_eq!(w, vec![(0, 10 * min), (10 * min, 20 * min), (20 * min, 25 * min)]);
        assert!(plan_windows(10, 10, min).is_err()); // empty range
    }

    #[test]
    fn signal_batching_sizes() {
        let sigs: Vec<String> = (0..92).map(|i| format!("s{i}")).collect();
        let batches: Vec<usize> = sigs.chunks(BATCH_SIZE).map(|b| b.len()).collect();
        assert_eq!(batches, vec![39, 39, 14]); // BATCH_SIZE = 39
        // <= one batch for small selections (unchanged behavior)
        let small: Vec<String> = (0..10).map(|i| format!("s{i}")).collect();
        assert_eq!(small.chunks(BATCH_SIZE).count(), 1);
    }

    #[test]
    fn sort_rows_by_time_orders_ascending() {
        let mk = |t: &str, f: &str| {
            let mut m = HashMap::new();
            m.insert("_time".to_string(), t.to_string());
            m.insert("_field".to_string(), f.to_string());
            m
        };
        // Rows arriving from two batches, interleaved out of order.
        let mut rows = vec![
            mk("2026-06-20T12:00:10Z", "b"),
            mk("2026-06-20T12:00:00Z", "a"),
            mk("2026-06-20T12:00:05Z", "b"),
            mk("2026-06-20T12:00:00Z", "a2"),
        ];
        sort_rows_by_time(&mut rows);
        let times: Vec<&str> = rows.iter().map(|r| r["_time"].as_str()).collect();
        assert_eq!(
            times,
            vec![
                "2026-06-20T12:00:00Z",
                "2026-06-20T12:00:00Z",
                "2026-06-20T12:00:05Z",
                "2026-06-20T12:00:10Z",
            ]
        );
        // stable: the two equal-time rows keep insertion order (a before a2)
        assert_eq!(rows[0]["_field"], "a");
        assert_eq!(rows[1]["_field"], "a2");
    }

    #[test]
    fn window_size_precedence() {
        let span = 6 * 3_600 * 1_000_000_000i64; // 6h
        // no-chunk -> whole span (one window)
        assert_eq!(window_size_ns(span, true, Some(3), Some("1h")).unwrap(), span);
        // chunks beats chunk
        assert_eq!(window_size_ns(span, false, Some(3), Some("1h")).unwrap(), span / 3);
        // chunk duration
        assert_eq!(
            window_size_ns(span, false, None, Some("1h")).unwrap(),
            3_600 * 1_000_000_000
        );
        // default
        assert_eq!(window_size_ns(span, false, None, None).unwrap(), DEFAULT_CHUNK_NS);
        assert!(window_size_ns(span, false, Some(0), None).is_err());
    }

    #[test]
    fn resolve_time_bound_marks_now() {
        let b = resolve_time_bound_at("now()", 0).unwrap();
        assert!(b.is_now);
        assert_eq!(b.epoch_ns, 0);
        // a relative duration is an offset from the supplied now
        assert_eq!(resolve_time_bound_at("-10m", 0).unwrap().epoch_ns, -600_000_000_000);
        let e = resolve_time_bound_at("2000-01-01T00:00:00Z", 12345).unwrap();
        assert!(!e.is_now);
        assert_eq!(e.epoch_ns, 946_684_800_000_000_000);
    }

    #[test]
    fn cache_roundtrip_and_freshness() {
        let yaml = "\
# influx-export signal cache — generated 2026-06-20T15:50:00Z from carputer (lookback -1d)
signals:
  - BMSB_packPower
  - bmsw0_cellTemp1
  - bmsw7_tempMax
";
        assert_eq!(
            parse_cache_signals(yaml),
            vec![
                "BMSB_packPower".to_string(),
                "bmsw0_cellTemp1".to_string(),
                "bmsw7_tempMax".to_string(),
            ]
        );
        // header timestamp parses to the right epoch-ns
        let g = parse_cache_generated(yaml).unwrap();
        assert_eq!(g, parse_rfc3339_to_ns("2026-06-20T15:50:00Z").unwrap());
        // freshness: <24h fresh, >=24h or future stale
        let day = 24 * 3600 * 1_000_000_000i64;
        assert!(cache_is_fresh(g, g + day - 1));
        assert!(!cache_is_fresh(g, g + day)); // exactly 24h -> stale
        assert!(!cache_is_fresh(g, g + 5 * day));
        assert!(!cache_is_fresh(g, g - 1)); // future cache -> not fresh
        // malformed / missing -> no signals, no timestamp
        assert!(parse_cache_signals("signals:\n").is_empty());
        assert!(parse_cache_generated("no header here").is_none());
    }

    #[test]
    fn cache_path_shape() {
        // ends with the per-target relative path regardless of HOME/XDG base
        let p = cache_path(Target::Carputer).unwrap();
        assert!(p.ends_with("influx-export/signals-carputer.yaml"), "{}", p.display());
        let p = cache_path(Target::Baseputer).unwrap();
        assert!(p.ends_with("influx-export/signals-baseputer.yaml"), "{}", p.display());
    }

    #[test]
    fn parse_csv_line_basics() {
        assert_eq!(parse_csv_line("a,b,c"), vec!["a", "b", "c"]);
        assert_eq!(parse_csv_line(",,0"), vec!["", "", "0"]);
        assert_eq!(parse_csv_line("\"x,y\",z"), vec!["x,y", "z"]);
        assert_eq!(parse_csv_line("\"he\"\"llo\""), vec!["he\"llo"]);
    }

    // A realistic Influx annotated-CSV response for a long-format export
    // (from |> range |> filter |> keep |> sort), CRLF line endings included.
    const LONG_RESPONSE: &str = "#group,false,false,false,true,true,false\r\n\
#datatype,string,long,dateTime:RFC3339,string,string,double\r\n\
#default,_result,,,,,\r\n\
,result,table,_time,_measurement,_field,_value\r\n\
,,0,2026-06-13T14:33:01Z,BMSB,BMSB_packPower,12.3\r\n\
,,0,2026-06-13T14:33:02Z,BMSB,BMSB_packPower,12.4\r\n\
,,0,2026-06-13T14:33:03Z,BMSB,BMSB_packPower,12.5\r\n";

    #[test]
    fn parse_long_export_rows() {
        let (header, rows) = parse_annotated_csv(LONG_RESPONSE);
        assert_eq!(header, vec!["_time", "_measurement", "_field", "_value"]);
        assert_eq!(rows.len(), 3, "expected 3 data rows, got {}", rows.len());
        assert_eq!(rows[0].get("_field").map(String::as_str), Some("BMSB_packPower"));
        assert_eq!(rows[0].get("_value").map(String::as_str), Some("12.3"));
    }

    #[test]
    fn write_csv_long_roundtrip() {
        let (header, rows) = parse_annotated_csv(LONG_RESPONSE);
        let out = std::env::temp_dir().join("influx_export_test_long.csv");
        let n = write_csv(&header, &rows, &out).unwrap();
        assert_eq!(n, 3);
        let written = std::fs::read_to_string(&out).unwrap();
        let lines: Vec<&str> = written.lines().collect();
        assert_eq!(lines[0], "_time,_measurement,_field,_value");
        assert_eq!(lines.len(), 4); // header + 3 rows
        let _ = std::fs::remove_file(&out);
    }
}
