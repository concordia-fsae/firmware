---
name: component-updates
description: Use when updating software for a controller or the vehicle stack, including inspecting BUCK files to identify conUDS, deployable, ota-agent, sideload, and whole-stack targets, then choosing the preferred flashing or OTA path.
---

# Component Updates

Use this skill when updating software for embedded controllers or the carputer stack.

This repo has multiple update paths:

- `conUDS` download targets for direct CAN flashing
- per-component `deploy` and `ota` targets for ota-agent delivery
- whole-stack carputer bundle targets for sideload/bootstrap flows

Prefer ota-agent targets by default. Use `conUDS` only when direct CAN flashing is appropriate. Favor sideloaded bundles when network interfaces do not change and faster runtime matters.

## Core rule

Always inspect the relevant `BUCK` file first. Do not guess target names if the component’s BUCK can tell you the exact pattern.

## What to look for in component BUCK files

For embedded components, inspect the component BUCK and supporting rules for:

- `conUDS_download(...)`
- `deployable_target(...)`
- `ota_agent(...)`
- aliases such as `download-cfr26`, `deploy-cfr26`, `ota-cfr26`

Typical component pattern:

1. a binary or CRC artifact is built
2. `conUDS_download` creates `download-*`
3. `deployable_target` creates `deploy-*`
4. `ota_agent` creates `ota-*`

Examples in this repo:

- `components/vc/front/BUCK`
- `components/vc/rear/BUCK`
- `components/vc/pdu/BUCK`
- `components/sws/BUCK`
- `components/bms_boss/BUCK`
- `components/bms_worker/BUCK`

For multi-node controllers such as `bms_worker`, the BUCK may generate per-node targets like:

- `download-cfr26-node-0`
- `deploy-cfr26-node-0`
- `ota-cfr26-node-0`

and convenience aliases such as:

- `download-cfr26`
- `deploy-cfr26`
- `ota-cfr26`

## Rule definitions to understand

Read these files when you need exact behavior:

- `components/vehicle_platform/defs.bzl`
- `drive-stack/conUDS/defs.bzl`
- `drive-stack/defs.bzl`
- `drive-stack/ota-agent/defs.bzl`
- `drive-stack/carputer/defs.bzl`

Important meanings:

- `conUDS_download`: direct ECU flashing over CAN
- `deployable_target`: wraps an artifact as a deployable node asset
- `ota_agent`: ota-agent client upload for a single target
- `ota_agent_batch`: batch update path, used for stack-style sideloading
- `ota_agent_bootstrap`: full bundle staging/bootstrap path
- `ota_agent_promote`: promote staged bundle to production

## Choosing the update path

### Preferred default: per-component ota-agent

Prefer per-component `ota-*` targets when:

- the component exposes an ota-agent target
- the user is updating one component or a small set of components
- the update should go through the normal network delivery path

Typical examples:

- `buckle run //components/vc/front:ota-cfr26`
- `buckle run //components/sws:ota-cfr26`

### Direct CAN flashing: conUDS

Use `download-*` / `conUDS` targets when:

- the user is connected to the CAN bus
- direct ECU flashing is desired
- ota-agent is unavailable or not preferred for the task

Important:

- laptop-side `conUDS` requires a local CAN device
- if the host does not have CAN, do not default to this path

Typical example:

- `buckle run //components/vc/front:download-cfr26`

### Faster stack updates: sideload / batch / whole bundle

Favor sideloading when:

- no network interfaces change
- the user wants faster runtime than repeated per-component OTA operations
- the update affects a larger portion of the vehicle stack

Whole-stack flows are defined in `drive-stack/carputer/BUCK` and `drive-stack/carputer/defs.bzl`.

Useful targets include:

- `//drive-stack/carputer:stage-carputer`
- `//drive-stack/carputer:ota-carputer`
- `//drive-stack/carputer:promote-carputer`
- platform-specific targets such as `ota-carputer-cfr26`
- batch OTA target from `carputer_platform_targets`, e.g. `cfr26-ota`

Interpretation:

- `stage-carputer`: alias to the stage/bootstrapable carputer OTA path
- `ota-carputer`: whole bundle bootstrap OTA path
- `promote-carputer`: promote the staged whole-stack bundle
- platform batch target: update bundle plus firmware deployables together

## Inspection workflow

1. Find the component BUCK.
2. Search for `conUDS_download`, `deployable_target`, and `ota_agent`.
3. Confirm whether there are generated aliases for `download-*`, `deploy-*`, and `ota-*`.
4. If the component is part of a broader stack update, inspect `drive-stack/carputer/BUCK`.
5. Decide between:
   - per-component OTA
   - direct `conUDS`
   - whole-stack sideload/bootstrap

Useful commands:

```bash
rg -n "conUDS_download\\(|deployable_target\\(|ota_agent\\(" components drive-stack -S
sed -n '1,220p' components/vc/front/BUCK
sed -n '1,220p' drive-stack/carputer/BUCK
sed -n '1,220p' components/vehicle_platform/defs.bzl
sed -n '1,220p' drive-stack/ota-agent/defs.bzl
sed -n '1,120p' drive-stack/conUDS/defs.bzl
```

## Practical guidance

- Prefer asking the user to flash if there is any risk in choosing the wrong path.
- Prefer `ota-*` over `download-*` unless direct CAN flashing is explicitly appropriate.
- Prefer sideload/batch/whole-stack paths when updating many targets and network interfaces are unchanged.
- For carputer and whole-vehicle flows, inspect root aliases and `drive-stack/carputer/BUCK` before improvising commands.
- Do not assume that `build` performs the update. Many aliases require `buckle run`.

## Validation after update

After any update:

- verify the target comes back online
- if applicable, check ota-agent status targets
- verify expected services or controllers are healthy before continuing with functional validation
