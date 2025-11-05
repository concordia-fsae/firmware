use std::collections::{HashMap, VecDeque};
use std::fs::OpenOptions;
use std::io::BufWriter;
use std::io::Write;
use std::fs::{create_dir_all, read_dir, rename, remove_file, metadata};
use std::fs::File;
use std::path::{Path, PathBuf};
use std::sync::{mpsc, Arc};
use std::time::{Instant, Duration, SystemTime};
use std::thread;

use clap::Parser;
use chrono::Local;
use libc::{CAN_EFF_FLAG, CAN_EFF_MASK, CAN_SFF_MASK};
use flate2::write::GzEncoder;
use flate2::Compression;
use ratatui::crossterm::event::{self, DisableMouseCapture, EnableMouseCapture, Event as CEvent, KeyCode, KeyEventKind, KeyModifiers};
use ratatui::crossterm::execute;
use ratatui::crossterm::terminal::{disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen};
use ratatui::prelude::{Constraint, CrosstermBackend, Direction, Layout, Rect, Style};
use ratatui::widgets::{Block, Borders, List, ListItem, Paragraph, Tabs, Wrap};
use ratatui::Terminal;
use tar::Builder;

use can_bridge::{
    BusDbc, BusState, Event, Filters,
    parse_bus_specs, spawn_workers, maybe_decode, format_can_line,
};

const LOG_BUS_LABEL: &str = "can";

/// Read multiple CAN buses; each input may be IFACE or IFACE=DBC.
/// Filters:
///  - --id / -i ID or START-END (repeatable; hex like 0x123 supported)
///  - --msg / -m SUBSTR         (repeatable; case-insensitive)
///  - --sig / -s SUBSTR         (repeatable; case-insensitive)
///
/// Rules in OUTPUT loop:
///  - No filters → print everything
///  - If ID filters set → print when ID matches (regardless of msg/sig)
///  - If only msg/sig filters set → print only when message name or any signal matches (needs DBC)
#[derive(Parser, Debug)]
#[command(
    name = "can-bridge",
    about = "Merge CAN frames from multiple interfaces; per-bus optional DBC decode; filters; decoding in main thread."
)]
struct Args {
    /// Quiet mode: suppress stdout/UI (still runs listeners & filtering/decoding)
    #[arg(long, short, default_value_t = false)]
    quiet: bool,

    /// Output JSON to stdout
    #[arg(long, short, default_value_t = false)]
    json: bool,

    /// Enable terminal UI mode
    #[arg(long, default_value_t = false)]
    tui: bool,

    /// Include only these CAN IDs or ranges (repeatable). Examples: -i 0x123 -i 0x100-0x1FF
    #[arg(long = "id", short = 'i', value_name = "ID|START-END")]
    ids: Vec<String>,

    /// Include only messages whose name contains this substring (repeatable, case-insensitive)
    #[arg(long = "msg", short = 'm', value_name = "SUBSTR")]
    msgs: Vec<String>,

    /// Include only signals whose name contains this substring (repeatable, case-insensitive)
    #[arg(long = "sig", short = 's', value_name = "SUBSTR")]
    sigs: Vec<String>,

    /// Bus specs: IFACE or IFACE=PATH/TO.dbc (repeatable)
    ///
    /// Examples:
    ///   can0
    ///   can1=chassis.dbc
    ///   vcan0=/etc/dbcs/powertrain.dbc
    #[arg(value_name = "IFACE[=DBC]", num_args = 1..)]
    inputs: Vec<String>,

    /// Max messages kept in the UI scrollback
    #[arg(long, default_value_t = 2000)]
    max_msgs: usize,

    /// Write rolling logs here prior to compression and moving into the log-dir
    #[arg(long = "tmp-dir")]
    tmp_dir: Option<PathBuf>,

    /// If set, write rolling per-bus logs to this directory
    #[arg(long = "log-dir")]
    log_dir: Option<PathBuf>,

    /// Maximum time before rolling in minutes
    #[arg(long = "log-mins", default_value_t = 15)]
    log_rollover: u32,

    /// Maximum time before rolling in minutes
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

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args = Args::parse();

    // Parse bus specs
    let (buses, per_bus_bundles) = parse_bus_specs(&args.inputs)?;

    // Build filters (used in OUTPUT loop)
    let filters = Filters::from_parts(&args.ids, &args.msgs, &args.sigs)?;
    if filters.id_ranges.is_empty() && filters.msg_filters.is_empty() && filters.sig_filters.is_empty() {
        println!("[filters] none");
    } else {
        if !filters.id_ranges.is_empty() {
            let ids: Vec<String> = filters.id_ranges.iter().map(|r| {
                if r.start == r.end {
                    format!("0x{:X} ({})", r.start, r.start)
                } else {
                    format!("0x{:X}-0x{:X} ({}-{})", r.start, r.end, r.start, r.end)
                }
            }).collect();
            println!("[filters] id: {}", ids.join(", "));
        }
        if !filters.msg_filters.is_empty() {
            println!("[filters] msg: {}", filters.msg_filters.join(", "));
        }
        if !filters.sig_filters.is_empty() {
            println!("[filters] sig: {}", filters.sig_filters.join(", "));
        }
    }

    // Channels
    let (tx, rx) = mpsc::channel::<Event>();
    let (bus_states_emitter, bus_states) = mpsc::channel::<(String, BusState)>();

    // Initial bus status (UI shows Starting -> Active once a frame is seen)
    let mut bus_status: HashMap::<String, BusState> = HashMap::new();
    for bus in &buses {
        bus_status.insert(bus.clone(), BusState::Starting);
    }

    // Spawn workers
    spawn_workers(&buses, tx, bus_states_emitter)?;

    // Output / UI
    if args.tui {
        run_tui(
            rx,
            bus_states,
            buses,
            per_bus_bundles,
            filters,
            bus_status,
            args.max_msgs,
        )?;
    } else {
        let log_cfg = match args.log_dir {
            Some(_) => {
                args.log_dir.as_ref().map(|dir| LogConfig {
                    dir: dir.clone(),
                    tmp: args.tmp_dir.clone().expect("tmp folder must be specified when logging"),
                    max_age: Duration::from_secs((args.log_rollover * 60).into()),
                    max_bytes: args.log_size,
                })
            }
            _ => None,
        };
        run_headless(rx, bus_states, per_bus_bundles, filters, args.quiet, args.json, log_cfg);
    }

    Ok(())
}

// ----------------------- TUI -----------------------

fn run_tui(
    rx: mpsc::Receiver<Event>,
    states: mpsc::Receiver<(String, BusState)>,
    buses: Vec<String>,
    per_bus_bundles: HashMap<String, Arc<BusDbc>>,
    filters: Filters,
    mut bus_status: HashMap<String, BusState>,
    max_msgs: usize,
) -> std::io::Result<()> {
    // Terminal setup
    enable_raw_mode()?;
    let mut std_fd = std::io::stderr();
    execute!(std_fd, EnterAlternateScreen, EnableMouseCapture)?;
    let backend = CrosstermBackend::new(std_fd);
    let mut terminal = Terminal::new(backend)?;

    // Message buffer
    let mut messages: VecDeque<String> = VecDeque::with_capacity(max_msgs);

    // Tabs (single tab "main")
    let tabs = vec!["main".to_string()];
    let mut active_tab_idx = 0usize;

    // UI loop
    'ui: loop {
        // Drain all pending events from CAN workers
        while let Ok((node, state)) = states.try_recv() {
            bus_status.insert(node, state);
        }
        while let Ok(ev) = rx.try_recv() {
            let id_masked = if (ev.frame.can_id & CAN_EFF_FLAG) != 0 {
                ev.frame.can_id & CAN_EFF_MASK
            } else {
                ev.frame.can_id & CAN_SFF_MASK
            } as u32;

            let has_id_filters = !filters.id_ranges.is_empty();
            let has_msg_sig_filters = !filters.msg_filters.is_empty() || !filters.sig_filters.is_empty();

            let maybe_line = if !has_msg_sig_filters {
                if has_id_filters && !filters.match_id(id_masked) {
                    None
                } else {
                    let decoded = maybe_decode(
                        per_bus_bundles.get(&ev.bus),
                        &ev.frame,
                        id_masked,
                        true,   // allow empty
                        true,   // ignore msg filter
                        &[],
                        &[],
                    );
                    Some(format_can_line(
                            &ev.bus,
                            per_bus_bundles.get(&ev.bus).map(|b| b.dbc_name.as_str()),
                            &ev.frame,
                            ev.ts_opt,
                            decoded,
                            false
                            ))
                }
            } else {
                let decoded = maybe_decode(
                    per_bus_bundles.get(&ev.bus),
                    &ev.frame,
                    id_masked,
                    false,  // don't allow empty
                    false,  // enforce msg filter
                    &filters.msg_filters,
                    &filters.sig_filters,
                );
                decoded.map(|d| format_can_line(
                        &ev.bus,
                        per_bus_bundles.get(&ev.bus).map(|b| b.dbc_name.as_str()),
                        &ev.frame,
                        ev.ts_opt,
                        Some(d),
                        false
                        ))
            };

            if let Some(line) = maybe_line {
                if messages.len() == max_msgs { messages.pop_front(); }
                messages.push_back(line);
            }
        }

        // Draw
        terminal.draw(|f| {
            // Layout: [top status (rows ≈ buses + 2) | messages | tabs]
            let chunks = Layout::default()
                .direction(Direction::Vertical)
                .constraints([
                    Constraint::Length((buses.len() + 2).try_into().unwrap()),
                    Constraint::Min(1),
                    Constraint::Length(1),
                ])
                .split(f.size());

            // Top: status box
            let mut status_lines: Vec<String> = Vec::with_capacity(buses.len());
            for b in &buses {
                let dbc_name = per_bus_bundles
                    .get(b)
                    .map(|x| x.dbc_name.as_str())
                    .unwrap_or("no-dbc");
                let s = match bus_status.get(b) {
                    Some(BusState::Active) => "Active".to_string(),
                    Some(BusState::Failed) => "Failed".to_string(),
                    Some(BusState::Error) => "Error".to_string(),
                    _ => "Starting".to_string(),
                };
                status_lines.push(format!("{b} [{dbc_name}] Status: {s}\n"));
            }
            let status = Paragraph::new(status_lines.join("  "))
                .block(Block::default().borders(Borders::ALL).title("bus status"))
                .wrap(Wrap { trim: true });
            f.render_widget(status, chunks[0]);

            // Middle: message list
            let list_items: Vec<ListItem> = messages
                .iter()
                .rev() // newest on top
                .map(|m| ListItem::new(m.clone()))
                .collect();
            let list = List::new(list_items)
                .block(Block::default().borders(Borders::ALL).title("messages"));
            f.render_widget(list, chunks[1]);

            // Bottom: tabs strip (single tab "main")
            let t = Tabs::new(tabs.iter().map(|t| t.as_str()).collect::<Vec<_>>())
                .select(active_tab_idx)
                .block(Block::default().borders(Borders::NONE))
                .style(Style::default())
                .divider(" ")
                .padding("", "");
            f.render_widget(t, chunks[2]);
        })?;

        // Input (non-blocking poll)
        if event::poll(Duration::from_millis(50))? {
            match event::read()? {
                CEvent::Resize(w, h) => {
                    terminal.resize(Rect::new(0, 0, w, h))?;
                }
                CEvent::Key(key) => {
                    if key.kind == KeyEventKind::Press {
                        match key.code {
                            KeyCode::Char('q') => break 'ui,
                            KeyCode::Char('c') if key.modifiers.contains(KeyModifiers::CONTROL) => break 'ui,
                            _ => {}
                        }
                    }
                }
                _ => {}
            }
        }
    }

    // teardown
    disable_raw_mode()?;
    execute!(
        terminal.backend_mut(),
        LeaveAlternateScreen,
        DisableMouseCapture
    )?;
    terminal.show_cursor()?;
    Ok(())
}

fn log_file_path(base: &Path, bus: &str) -> PathBuf {
    let stamp = Local::now().format("%Y-%m-%d_%H%M%S");
    base.join(format!("{bus}-{stamp}.log"))
}

fn open_bus_log(base: &Path, bus: &str) -> std::io::Result<BusLog> {
    create_dir_all(base)?;
    let path = log_file_path(base, bus);
    let file = OpenOptions::new()
        .create(true)
        .append(true)
        .open(&path)?;
    eprintln!("log: opening file {:?}", path);
    let size = file.metadata().ok().map(|m| m.len()).unwrap_or(0);
    Ok(BusLog {
        opened_at: Instant::now(),
        current_size: size,
        writer: BufWriter::new(file),
        path,
    })
}

/// Only respect time/size for rollovers (no daily rotation)
fn should_roll(bl: &BusLog, cfg: &LogConfig) -> bool {
    if bl.current_size >= cfg.max_bytes {
        return true;
    }
    if bl.opened_at.elapsed() >= cfg.max_age {
        return true;
    }
    false
}

/// Headless (non-TUI) output loop
/// Single rolling log for ALL buses. The log file contains ONLY data lines, one per message
pub fn run_headless(
    rx: mpsc::Receiver<Event>,
    bus_states: mpsc::Receiver<(String, BusState)>,
    per_bus_bundles: HashMap<String, Arc<BusDbc>>,
    filters: Filters,
    quiet: bool,
    json: bool,
    log_cfg: Option<LogConfig>,
) {
    let mut last_cycle = Instant::now();
    let mut bandwidth: HashMap<String, u32> = HashMap::new();

    // Use a single BusLog for ALL buses. We do not change the BusLog type or helpers.
    let mut global_log: Option<BusLog> = None;

    if let Some(cfg) = log_cfg.clone() {
        match find_uncompressed_logs(&cfg.tmp) {
            Ok(paths) => {
                if !paths.is_empty() {
                    println!("log: startup: found {} uncompressed logs; compressing...", paths.len());
                }
                for p in paths.iter() {
                    let p = p.clone();
                    let cfg_cloned = cfg.clone();
                    if let Err(e) = thread::Builder::new()
                        .name("log-compress".into())
                        .spawn(move || {
                            let start_time = Instant::now();
                            match compress_and_remove(&p, &cfg_cloned.dir) {
                                Ok(gz) => {
                                    // Best-effort size reporting
                                    let size_mb = metadata(&gz)
                                        .map(|m| m.len() / (1024 * 1024))
                                        .unwrap_or(0);
                                    println!(
                                        "log: compressed '{}' → '{}', size: {}MB duration: {:?}",
                                        p.display(),
                                        gz.display(),
                                        size_mb,
                                        start_time.elapsed(),
                                    );
                                }
                                Err(e) => eprintln!("log: compression failed for '{}': {e}", p.display()),
                            }
                        })
                    {
                        eprintln!("log: failed to spawn compression thread: {e}");
                    }
                }
            }
            Err(e) => eprintln!("log: startup: failed to enumerate '{}': {e}", cfg.dir.display()),
        }
    }

    loop {
        let time_delta = Instant::now() - last_cycle;
        if time_delta >= Duration::from_secs(60) {
            for (bus, bps) in bandwidth.drain() {
                println!(
                    "[{}] transmits {:.3} bits/s in {:?}",
                    bus,
                    bps as f64 / time_delta.as_secs_f64(),
                    time_delta
                );
            }
            last_cycle = Instant::now();
        }

        while let Ok((node, state)) = bus_states.try_recv() {
            let s = match state {
                BusState::Active => "Active",
                BusState::Failed => "Failed",
                BusState::Error => "Error",
                _ => "Starting",
            };
            println!("[{}] reported: {}", node, s);
        }

        while let Ok(ev) = rx.try_recv() {
            let mut bit_length = 43 + ev.frame.can_dlc as u32 * 8;
            let id_masked = if (ev.frame.can_id & CAN_EFF_FLAG) != 0 {
                bit_length += 29;
                ev.frame.can_id & CAN_EFF_MASK
            } else {
                bit_length += 11;
                ev.frame.can_id & CAN_SFF_MASK
            };

            let has_id_filters = !filters.id_ranges.is_empty();
            let has_msg_sig_filters = !filters.msg_filters.is_empty() || !filters.sig_filters.is_empty();

            match bandwidth.get(&ev.bus) {
                Some(bits) => { bandwidth.insert(ev.bus.clone(), (bit_length + *bits).into()); }
                None => { bandwidth.insert(ev.bus.clone(), bit_length.into()); }
            }

            let line = if !has_msg_sig_filters {
                if has_id_filters && !filters.match_id(id_masked) { continue; }
                let decoded = maybe_decode(
                    per_bus_bundles.get(&ev.bus),
                    &ev.frame,
                    id_masked,
                    true,
                    true,
                    &[],
                    &[],
                );
                format_can_line(
                    &ev.bus,
                    per_bus_bundles.get(&ev.bus).map(|b| b.dbc_name.as_str()),
                    &ev.frame,
                    ev.ts_opt,
                    decoded,
                    json,
                )
            } else {
                let decoded = maybe_decode(
                    per_bus_bundles.get(&ev.bus),
                    &ev.frame,
                    id_masked,
                    false,
                    false,
                    &filters.msg_filters,
                    &filters.sig_filters,
                );
                if decoded.is_none() { continue; }
                format_can_line(
                    &ev.bus,
                    per_bus_bundles.get(&ev.bus).map(|b| b.dbc_name.as_str()),
                    &ev.frame,
                    ev.ts_opt,
                    decoded,
                    json,
                )
            };

            if !quiet {
                println!("{line}");
            }

            // ---- Single rolling log (size/time only); rollover info -> stdout only ----
            if let Some(cfg) = &log_cfg {
                let old_path = match &global_log {
                    Some(bl) => Some(bl.path.clone()),
                    None => None,
                };

                // Determine if we need to (re)open due to size/time threshold
                let need_open = match global_log.as_ref() {
                    None => {
                        println!("log: creating new log...");
                        true
                    }
                    Some(bl) => should_roll(bl, &cfg),
                };

                if need_open {
                    // If this is a rollover (not the initial open), announce why on stdout
                    if let Some(bl) = &global_log {
                        let elapsed = bl.opened_at.elapsed();
                        let secs = elapsed.as_secs_f64();
                        let size = bl.current_size / (1024*1024);
                        let time_triggered = elapsed >= cfg.max_age;
                        let size_triggered = size >= cfg.max_bytes;
                        let reason = match (time_triggered, size_triggered) {
                            (true, true) => "time and size",
                            (true, false) => "time",
                            (false, true) => "size",
                            (false, false) => "policy",
                        };
                        println!(
                            "log: opening new log (reason: {reason}, duration: {:.1}s, size: {} MB)",
                            secs, size
                        );
                    }

                    // Drop old and open new single shared log file
                    global_log = match open_bus_log(&cfg.tmp, LOG_BUS_LABEL) {
                        Ok(new_log) => Some(new_log),
                        Err(e) => {
                            eprintln!("log: failed to open global log: {e}");
                            // Skip logging this line; try again on the next message
                            continue;
                        }
                    };
                    if let Some(path) = old_path.clone() {
                        let cfg_cloned = cfg.clone();
                        if let Err(e) = thread::Builder::new()
                            .name("log-compress".into())
                            .spawn(move || {
                                let start_time = Instant::now();
                                match compress_and_remove(&path, &cfg_cloned.dir) {
                                    Ok(gz) => {
                                        // Best-effort size reporting
                                        let size_mb = metadata(&gz)
                                            .map(|m| m.len() / (1024 * 1024))
                                            .unwrap_or(0);
                                        println!(
                                            "log: compressed '{}' → '{}', size: {}MB duration: {:?}",
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
                }

                // Write the data line to the single file (no headers/markers)
                if let Some(bl) = global_log.as_mut() {
                    if let Err(e) = writeln!(bl.writer, "{line}") {
                        eprintln!("log: write failed for single log: {e}");
                        // Drop writer so the next message reopens
                        global_log = None;
                    } else {
                        bl.current_size = bl.current_size.saturating_add((line.len() + 1) as u64);
                    }
                }
            }
        }
    }
}

fn gb_to_bytes(gb: f64) -> u128 {
    // Decimal GB as requested: 1 GB = 1_000_000_000 bytes
    (gb * 1_000_000_000f64) as u128
}

fn list_log_files(dir: &Path) -> std::io::Result<Vec<(PathBuf, u64, SystemTime)>> {
    let mut files = Vec::new();
    for entry in read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();
        if !path.is_file() { continue; }
        if path.extension().and_then(|s| s.to_str()) != Some("log") { continue; }
        let md = entry.metadata()?;
        let len = md.len();
        let modified = md.modified().unwrap_or(SystemTime::UNIX_EPOCH);
        files.push((path, len, modified));
    }
    // Oldest first
    files.sort_by_key(|(_, _, m)| *m);
    Ok(files)
}

fn dir_usage_bytes(dir: &Path) -> std::io::Result<u128> {
    let mut total: u128 = 0;
    for entry in read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();
        if !path.is_file() { continue; }
        if path.extension().and_then(|s| s.to_str()) != Some("log") { continue; }
        if let Ok(md) = entry.metadata() {
            total = total.saturating_add(md.len() as u128);
        }
    }
    Ok(total)
}

fn compress_and_remove(orig_path: &Path, dest_folder: &Path) -> std::io::Result<PathBuf> {
    // new path: "<name>.tar.gz"
    let mut gz_path = orig_path.to_path_buf();
    gz_path.set_extension("log.tar.gz"); // if original ends with .log, this becomes .log.tar.gz

    // Create gzip stream wrapped in a tar builder
    let out = File::create(&gz_path)?;
    let enc = GzEncoder::new(out, Compression::default());
    let mut tar = Builder::new(enc);

    // Add the file under its base name inside the archive
    let base_name = orig_path.file_name().unwrap_or_default();
    tar.append_path_with_name(orig_path, base_name)?;
    tar.finish()?; // finish tar
    // finalize gzip (drop GzEncoder via into_inner chain)
    let _enc = tar.into_inner()?; // GzEncoder<File>
    let mut _out = _enc.finish()?; // File

    // Move log to log folder
    let dest_path = dest_folder.join(gz_path.file_name().unwrap_or_default());
    rename(&gz_path, &dest_path)?;
    // Delete original .log
    remove_file(orig_path)?;

    Ok(dest_path)
}

fn find_uncompressed_logs(dir: &Path) -> Result<Vec<PathBuf>, std::io::Error> {
    let mut out = Vec::new();
    for entry in read_dir(dir)? {
        let entry = entry?;
        let p = entry.path();
        if !p.is_file() { continue; }

        // true if the extension is ".log"
        let is_log = p.extension()
            .and_then(|s| s.to_str())
            .map(|s| s == "log")
            .unwrap_or(false);

        if is_log {
            out.push(p);
        }
    }
    Ok(out)
}
