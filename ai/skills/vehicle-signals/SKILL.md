______________________________________________________________________

## name: vehicle-signals description: Use when you need to inspect live vehicle signals through the carputer dashboard, discover exact signal IDs from the manifest, capture SSE signal streams, or review controller online and fault state over the network.

# Vehicle Signals

Use this skill for live signal inspection through the carputer dashboard and carputer SSH access.

## Quick checks

- Dashboard health: `curl -sSf http://carputer:8091/healthz`
- Controller session: `curl -sSf http://carputer:8091/api/controllers/vcfront/current-session`
- SSH to carputer: `ssh carputer hostname`
- Dashboard service: `ssh carputer systemctl status dashboard --no-pager --full`

The dashboard normally runs on `http://carputer:8091`.

## Signal discovery

Always fetch the exact manifest IDs before subscribing. The dashboard expects full signal IDs, not guessed names.

Example:

```bash
curl -sSf http://carputer:8091/api/signals/manifest \
  | jq -r '.signals[] | select(.message_name=="VCFRONT_pedalPosition" or .message_name=="VCFRONT_pedalInformation") | [.id,.message_name,.signal_name] | @tsv'
```

For VCFRONT pedal work, use a discovery query first:

```bash
curl -sSf http://carputer:8091/api/signals/manifest \
  | jq -r '.signals[]
    | select(.message_name=="VCFRONT_pedalPosition" or .message_name=="VCFRONT_pedalInformation")
    | select(
        .signal_name=="VCFRONT_apps1" or
        .signal_name=="VCFRONT_apps1State" or
        .signal_name=="VCFRONT_apps2" or
        .signal_name=="VCFRONT_apps2State" or
        .signal_name=="VCFRONT_acceleratorPosition" or
        .signal_name=="VCFRONT_acceleratorState" or
        .signal_name=="VCFRONT_apps1Voltage" or
        .signal_name=="VCFRONT_apps2Voltage"
      )
    | [.id, .message_name, .signal_name, .unit, .kind]
    | @tsv'
```

Then derive a subscription query from the same filter:

```bash
curl -sSf http://carputer:8091/api/signals/manifest \
  | jq -r '
    [.signals[]
      | select(.message_name=="VCFRONT_pedalPosition" or .message_name=="VCFRONT_pedalInformation")
      | select(
          .signal_name=="VCFRONT_apps1" or
          .signal_name=="VCFRONT_apps1State" or
          .signal_name=="VCFRONT_apps2" or
          .signal_name=="VCFRONT_apps2State" or
          .signal_name=="VCFRONT_acceleratorPosition" or
          .signal_name=="VCFRONT_acceleratorState" or
          .signal_name=="VCFRONT_apps1Voltage" or
          .signal_name=="VCFRONT_apps2Voltage"
        )
      | .id]
    | join(",")
    | @uri'
```

## Live capture

Use `/signal-events` for SSE capture.

Example:

```bash
SIGNALS="$(curl -sSf http://carputer:8091/api/signals/manifest \
  | jq -r '
    [.signals[]
      | select(.message_name=="VCFRONT_pedalPosition" or .message_name=="VCFRONT_pedalInformation")
      | select(
          .signal_name=="VCFRONT_apps1" or
          .signal_name=="VCFRONT_apps1State" or
          .signal_name=="VCFRONT_apps2" or
          .signal_name=="VCFRONT_apps2State" or
          .signal_name=="VCFRONT_acceleratorPosition" or
          .signal_name=="VCFRONT_acceleratorState" or
          .signal_name=="VCFRONT_apps1Voltage" or
          .signal_name=="VCFRONT_apps2Voltage"
        )
      | .id]
    | join(",")
    | @uri')"

timeout 35 curl -N -s \
  "http://carputer:8091/signal-events?signals=${SIGNALS}" \
  > /tmp/vcfront_capture.sse
```

When collecting pedal data:

- Ask for held release and held full-pedal points.
- Prefer 3 full press/release cycles for validation.
- Capture at least one slow sweep when deriving a new map.

## Parsing

The stream is `event:signal-sample` with JSON in `data:` lines. Parse only those lines.

Common pattern:

```bash
grep '^data:' /tmp/vcfront_capture.sse | sed 's/^data://'
```

Then use `jq` or a short Python script to:

- join `VCFRONT_pedalPosition` and `VCFRONT_pedalInformation` by nearest timestamp
- inspect low, mid, and high ranges
- compute per-channel disagreement
- verify `acceleratorState` stays `OK`

## Failure patterns

- If `vcfront` is offline in `/events`, stop tuning and wait for the controller to come back.
- If `/signal-events` returns no useful samples, verify the exact signal IDs from the manifest again.
- If laptop-side `conUDS` says `No CAN devices detected`, do not use it for flashing from the laptop.

## Preferred flashing guidance

For firmware changes discovered during signal review:

- Prefer telling the user to flash.
- If using automation, prefer the component `buckle` OTA path for `cfr26`.
- Avoid ad hoc carputer-side `conUDS` flashing as the first choice.
