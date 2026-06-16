# Carputer Influx → CSV export (`influx-export`)

Pull a slice of the carputer's CAN telemetry out of its on-board InfluxDB and write
it to a CSV for plotting/analysis on your laptop.

The carputer stores each decoded CAN **signal** as an Influx **field**, under a
**measurement** named after the CAN message (e.g. `BMSB`, `VCFRONT`). Selecting
"signals" therefore means picking Influx field keys like `BMSB_packPower`.

## Quick start

Install the `influx-export` binary onto your PATH — tab completion works once it's
installed.

```sh
# 1. install the binary onto your PATH (once, and after any update)
cargo install --path tools/carputer/influx-export

# 2. install bash tab-completion (once)
influx-export completions bash > ~/.local/share/bash-completion/completions/influx-export
#    open a new shell afterwards

# 3. populate the signal list for tab-completion (while on the car network)
influx-export refresh-signals

# 4. export some signals — <TAB> completes signal names after --signals
influx-export export --start=-1h \
  --signals BMSB_packPower,VCFRONT_vehicleSpeed
```

## Connecting to the carputer

The carputer's Influx must be reachable at `http://<ip>:8086`.

- **On the car / pit network (normal case):** nothing to configure. With no
  `--host`/`--url` the tool auto-discovers the carputer over mDNS.
- `--target carputer|baseputer` (default `carputer`) auto-fills the mDNS service type
  **and** the read-only Influx token, so you don't pass `--service` or `--token`.

### Over Tailscale / a remote network

**mDNS auto-discovery will not work over Tailscale.** mDNS relies on multicast, and
Tailscale only forwards **unicast** traffic — even through an exit node or subnet
router. Discovery times out.

Fix: skip discovery and give the carputer's **LAN IP** directly with `--host`. Unicast
to that IP routes fine through your exit node / subnet router:

```sh
influx-export export --host 192.168.100.115 --start=-1h --signals BMSB_packPower
```

## Commands

| Command | What it does |
|---|---|
| `discover` | Browse mDNS and print the carputer's address. |
| `signals [--start --stop]` | List the signal (field) names present in a time window. |
| `refresh-signals` | Fetch all signal names and (re)build the tab-completion cache. Run once to make completion work. |
| `export …` | Write selected signals to a CSV. Needs `--signals a,b,c` **or** `--all`. |
| `completions bash` | Print the bash completion script. |

## `export` options

```sh
# specific signals
influx-export export --start=-1h --signals BMSB_packPower,VCFRONT_vehicleSpeed -o data.csv

# everything present in the window
influx-export export --all --start=-1h -o influx_data.csv
```

- `--signals a,b,c` — comma-separated (or repeat the flag). `--all` exports every
  signal in the window instead.
- `-o <path>` — output CSV. Default `influx_export.csv` in the current directory.
- `--dry-run` — print the planned windows + generated Flux and exit **without
  querying** (works offline).
- `-v`/`--verbose` — also echo the generated Flux for each query.

### Connection flags (any command)

`--target` · `--host` · `--url` (full base URL, skips mDNS) · `--port` (8086) ·
`--token` / `INFLUX_TOKEN` · `--org` (CFR) · `--bucket` (CarTelemetry) ·
`--interface`, `--service`, `--discover-timeout` (mDNS) · `--timeout` (per-query
seconds).

## Time values (`--start` / `--stop`)

Flux durations (`-15m`, `-1h30m`, `2d`), RFC3339 (`2026-06-13T10:33:00-04:00`), epoch
seconds, or `now()`. Durations start with `-`, so use the `=` form when a value could
be read as a flag: `--start=-7d`.

## Tab completion — how it works

`--signals` tab-completes from a local cache of the **actual database field keys**, so
the casing is always right (e.g. workers are stored lowercase `bmsw0_cellTemp1`, not
`BMSW0_…`).

The pieces:

1. The completion script calls the hidden `influx-export __complete-signals <prefix>`.
1. That reads names from a local YAML cache:
   `${XDG_CACHE_HOME:-~/.cache}/influx-export/signals-<target>.yaml`.
1. The cache is populated by **`influx-export refresh-signals`** (or as a side effect
   of an `export`/`signals` run when it's >24h old). Because completion reads a file,
   it works **offline** once the cache exists — you don't need the carputer to tab.

So the setup is: install completion once, run `refresh-signals` once (on the car
network) to fill the cache, and thereafter `--signals <TAB>` just works.

```sh
influx-export export --signals bmsw0_<TAB>              # offers bmsw0_*
influx-export export --signals BMSB_packPower,bmsw1_<TAB>  # completes each segment
```

If completion comes up empty, run `influx-export refresh-signals` while on the car
network to (re)populate the cache.

## Output

Every export is the **long/tall** shape: columns `_time`, `_measurement`, `_field`,
`_value` — one row per signal sample, sorted by time.

## Session export example

A ready-to-run export of a useful brake / steering / pack / motor / speed signal set
over a session window (edit `--host`, `--start`, `--stop`, `--signals` as needed):

````sh
influx-export export --host 192.168.100.115 \
  --start=2026-06-20T11:50:00-04:00 --stop=2026-06-20T12:19:00-04:00 \
  --signals VCFRONT_brakePressure,VCREAR_brakePressure,VCFRONT_steeringAngle,BMSB_packPower,BMSB_packVoltage,BMSB_packCurrent,PM100DX_idFeedback,PM100DX_iqFeedback,VCFRONT_vehicleSpeed```

## Gotchas

- **mDNS won't cross Tailscale** — use `--host <LAN IP>` (see
  [Over Tailscale](#over-tailscale--a-remote-network)).
- **Tab completion needs the installed binary**
- **Big time windows are heavy.** Listing signals is cheap; exporting months of data
  is not. Nothing is silently truncated — narrow the range for big exports.
- **Empty window / no data** → check the range overlaps recorded data and that the
  host/token are correct.
````
