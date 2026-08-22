# Yamcan

Yamcan is the repository's CAN network compiler and artifact generator. It
reads the YAML network definition, validates buses, nodes, messages, signals,
discrete values, templates, and forwarding rules, then produces the artifacts
needed by firmware and host applications.

Yamcan is a code-generation tool, not a CAN transport or runtime. It does not
schedule messages, own a CAN driver, or implement the simulation dataflow. The
generated C and Rust code is consumed by the target application; the canonical
inputs remain under `network/definition/`.

## Inputs and outputs

The network definition contains:

- `buses/*.yaml` for physical and virtual buses, baud rates, and endianness.
- `data/components/<node>/` for node messages, signals, and receive/
  forwarding rules.
- `discrete_values.yaml` and `data/templates/` for shared signal and
  message definitions.

From those inputs Yamcan can generate:

- C headers and sources for message types, packing, unpacking, signal RX/TX,
  network constants, and temporary stubs.
- Optional Rust wrappers and generated decode, model, and fault modules.
- DBC files and per-bus load statistics.
- Filtered node manifests, including the UDS manifest used by this repository.
- A validated serialized network cache used to avoid reparsing the YAML for
  each downstream generation step.

Validation is part of generation. Duplicate YAML keys, invalid enum values,
missing message or signal references, invalid bus configuration, ambiguous
manifest filters, and excessive physical-bus utilization fail the build.

## Buck integration

Consumers normally use the macros in
[`defs.bzl`](defs.bzl), rather than invoking the Python implementation
directly:

- `build_network` parses and caches a network definition directory.
- `generate_code` creates node-specific C artifacts and libraries.
- `generate_resources` creates the generated artifact set, with optional Rust
  wrappers.
- `generate_c_library` and `generate_rust_library` expose generated artifacts
  to downstream targets.
- `generate_dbcs`, `generate_stats`, and `generate_manifest` create analysis
  and integration artifacts.

The repository-level network targets demonstrate the normal workflow:

```bash
buckle build //network:network
buckle build //network:dbc --out dbc/
buckle build //network:stats --out stats/
buckle build //network:manifest-uds --out manifest-uds.yaml
```

Firmware components and drive-stack applications use `generate_code` or the
lower-level generation macros in their `BUCK` files. When adding a CAN
message, update the YAML definition, then rebuild the affected target; do not
edit generated C/Rust files by hand.

## Repository boundary

Yamcan is reusable within the repository's build graph: it owns network
definition parsing, validation, and generation. Firmware drivers, Rig
scheduling, simulation bindings, and model behavior belong to their respective
layers and consume Yamcan's outputs.
