______________________________________________________________________

## name: vcfront-accelerator-tuning description: Use when tuning or validating the VCFRONT accelerator pedal mapping in the pedal monitor driver, including APPS voltage-to-percent maps, low and high deadzones, shaping the middle of the curve, and post-flash validation.

# VCFRONT Accelerator Tuning

Use this skill for APPS calibration in:

- `components/vc/front/src/HW/drv_pedalMonitor_componentSpecific.c`
- `components/vc/front/src/apps.c`

Use the `vehicle-signals` skill for signal discovery, capture, and dashboard access. This skill focuses on calibration decisions and code changes.

The accelerator path is:

1. `drv_pedalMonitor` maps each APPS voltage to `0.0f..1.0f`
1. `apps.c` compares `APPS1` and `APPS2`
1. if they agree, VCFRONT averages them into `acceleratorPosition`

## Important behavior

- `apps.c` faults on disagreement greater than `PEDAL_TOLERANCE`
- current logic expects both sensor maps to be aligned, not merely monotonic
- use `saturate_left` and `saturate_right` for deadzones instead of repeated interpolation percentages
- keep interpolation tables at clean `5%` increments unless there is a strong reason not to
- when the pedal is physically released, the resulting accelerator reading must clamp to `0%`

## Tuning workflow

1. Confirm VCFRONT is online and capture released pedal, slow sweep, and 3 full press/release cycles.
1. Derive low-end anchor voltages from stable released samples.
1. Derive top-end anchor voltages from held full-pedal samples.
1. Shape the middle so `apps1` and `apps2` track closely through the sweep.
1. Keep each table monotonic and at `5%` increments.
1. Use saturation to create:
   - low deadzone below the `0%` point
   - high deadzone above the `100%` point
1. Reflash and re-measure.

## Practical heuristics

- If released pedal reads above `0-1%`, move the `0%` point upward slightly.
- Do not accept a released reading above `0%` as the final calibration target; released should settle at `0%`.
- If full pedal does not reliably hit `100%`, move the top few points downward slightly.
- If mid-stroke disagreement is high, reshape the middle points rather than only changing endpoints.
- Avoid changing both low end and high end at once unless the trace clearly justifies it.

## Validation targets

- preferred released target: `0%`
- acceptable released reading during iteration: `0-1%`
- held full pedal: `100%`
- mid-stroke average APPS delta: low single digits
- `acceleratorState`: `OK` throughout the sweep

## Flashing guidance

Preferred order:

1. Ask the user to flash the component.
1. If automating, prefer the component `buckle` OTA path for `cfr26`.

Do not default to laptop-side `conUDS` unless a local CAN device is present.

Be cautious with carputer-side direct `conUDS` downloads. In this workflow, transfer reached the ECU but failed at completion, and the controller dropped offline afterward.

## File edit rules

When editing the pedal map:

- preserve monotonic voltage order
- keep one point per `5%` increment
- do not duplicate `0%` or `100%` entries
- ensure declared array size matches point count
- re-run `git diff --check`
