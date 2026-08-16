# Rig

Rig is the reusable simulation library used by firmware models.

The project has one ownership root and two implementation languages:

- `python/` is installed and imported as the `rig` Python package.
- `python/cluster.py` provides the backend-independent `ClusterRig` and
  portable Python dataflow edge implementation for composing nodes.
- `python/cluster.py` also defines the typed `Rig[T]` container boundary;
  concrete nodes and components must satisfy the `RigElement` contract and
  backend implementations override lifecycle hooks rather than reimplementing
  topology or time control.
- `python/runtime.py` is the first-class Python interface to the generic
  Rust-backed Rig runtime. It owns node registration, scalar edges and scalar
  events, dataflow compilation, scheduler execution, waits, and elapsed time.
  A repository binding subclasses `RustClusterRuntime` only to add its own
  peripheral ABI; it does not duplicate these generic operations.
- `rust/lib.rs` is the standalone Rust core target. The remaining Rust
  modules (`algorithms.rs`, `dataflow.rs`, `datapath.rs`, `interfaces.rs`,
  `model.rs`, `model_abi.rs`, `node.rs`, `node_abi.rs`, `rig.rs`, `scalar.rs`,
  and `scheduler.rs`) are Rig-owned backend-composition modules. They depend
  only on contracts supplied by a consuming backend and are linked by that
  backend's Rust assembly target.
- `rust::Rig<T>` is the reusable Rust ownership boundary. `T` implements
  `rust::RigElement`; the container owns element identity, online gating,
  reset, and elapsed time.

Firmware-specific adapters, generated bindings, and model implementations stay
under `sim/bindings` and `sim/models`. They consume Rig; Rig does not import
them. `sim/bindings/core/firmware_cluster.py` specializes `ClusterRig` with
`FirmwareRuntime`, which owns only the firmware CAN, SPI, timer, and composite
peripheral ABI. The generic model lifecycle ABI and opaque datapath descriptor
contract live in `model_abi.rs` and `node_abi.rs`; firmware bindings only assign
their own symbols and interface-ID meaning. CAN, SPI, timer, power, and other
peripheral interfaces are backend bindings, not Rig concepts.

Build and test the standalone Rust core with:

```text
buckle test //tools/rig:core_unit
```
