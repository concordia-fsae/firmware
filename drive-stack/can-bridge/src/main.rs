use std::collections::{HashMap, VecDeque};
use std::sync::{mpsc, Arc};
use std::time::{Instant, Duration};

use clap::Parser;
use libc::{CAN_EFF_FLAG, CAN_EFF_MASK, CAN_SFF_MASK};
use ratatui::crossterm::event::{self, DisableMouseCapture, EnableMouseCapture, Event as CEvent, KeyCode, KeyEventKind, KeyModifiers};
use ratatui::crossterm::execute;
use ratatui::crossterm::terminal::{disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen};
use ratatui::prelude::{Constraint, CrosstermBackend, Direction, Layout, Rect, Style};
use ratatui::widgets::{Block, Borders, List, ListItem, Paragraph, Tabs, Wrap};
use ratatui::Terminal;

use can_bridge::{
    BusDbc, BusState, Event, Filters,
    parse_bus_specs, spawn_workers, maybe_decode, format_can_line,
};

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
        run_headless(rx, bus_states, per_bus_bundles, filters, args.quiet, args.json);
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
                    Some(format_can_line(&ev.bus, &ev.frame, ev.ts_opt, decoded, false))
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
                decoded.map(|d| format_can_line(&ev.bus, &ev.frame, ev.ts_opt, Some(d), false))
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

/// Headless (non-TUI) output loop; used when `--tui` is not set.
pub fn run_headless(
    rx: mpsc::Receiver<Event>,
    bus_states: mpsc::Receiver<(String, BusState)>,
    per_bus_bundles: HashMap<String, Arc<BusDbc>>,
    filters: Filters,
    quiet: bool,
    json: bool,
) {
    let mut last_cycle = Instant::now();
    let mut bandwidth: HashMap<String, u32> = HashMap::new();

    loop {
        let time_delta = Instant::now() - last_cycle;
        if time_delta >= Duration::from_secs(60) {
            for (bus, bps) in bandwidth.drain() {
                println!("[{}] transmits {:.3} bits/s in {:?}s", bus, bps as f64 / time_delta.as_secs_f64(), time_delta);
            }
            last_cycle = Instant::now();
        }

        while let Ok((node, state)) = bus_states.try_recv() {
            let s = match state {
                BusState::Active => "Active".to_string(),
                BusState::Failed => "Failed".to_string(),
                BusState::Error => "Error".to_string(),
                _ => "Starting".to_string(),
            };
            println!("[{}] reported: {}", node, s);
        }

        while let Ok(ev) = rx.try_recv() {
            // SOF(1), RTR(1), IDE(1), DLC(4), CRC(15)
            // DEL(1), ACK(1), DEL(1), EOF(7), ITM(11)
            let mut bit_length = 43;
            bit_length += ev.frame.can_dlc as u32 * 8;
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

            if !has_msg_sig_filters {
                if has_id_filters && !filters.match_id(id_masked) {
                    continue;
                }
                let decoded = maybe_decode(
                    per_bus_bundles.get(&ev.bus),
                    &ev.frame,
                    id_masked,
                    true,  // allow empty signals
                    true,  // ignore msg filter
                    &[],   // msg filters
                    &[],   // sig filters
                );

                let line = format_can_line(&ev.bus, &ev.frame, ev.ts_opt, decoded, json);
                if !quiet { println!("{line}"); }
            } else {
                let decoded = maybe_decode(
                    per_bus_bundles.get(&ev.bus),
                    &ev.frame,
                    id_masked,
                    false, // don't allow empty results
                    false, // enforce message filter
                    &filters.msg_filters,
                    &filters.sig_filters,
                );
                if decoded.is_none() {
                    continue;
                }

                let line = format_can_line(&ev.bus, &ev.frame, ev.ts_opt, decoded, json);
                if !quiet { println!("{line}"); }
            }
        }
    }
}
