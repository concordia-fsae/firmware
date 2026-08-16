# Rig

Rig is a reusable simulation library for models, components, and controllers.

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
- `rust/lib.rs` is the standalone Rust crate. `rust::RigRuntime<B>` owns the
  generic nodes, dataflow graph, scheduler, waits, scalar interface, and
  simulation clock. `B: RigBackend` is the only extension point for a
  consuming backend to contribute additional dataflow algorithms.
- `rust::Rig<T>` is the reusable element ownership boundary. `T` implements
  `rust::RigElement`; the container owns element identity, online gating,
  reset, and elapsed time.

A consuming application supplies its own adapters, generated bindings, and model
implementations. Those adapters consume Rig; Rig does not import them. A backend
may specialize `RustClusterRuntime` to own its peripheral ABI while retaining
Rig's generic node, dataflow, scheduler, wait, wake, scalar, and lifecycle
contracts. The model lifecycle ABI and opaque datapath descriptor contract live
in `model_abi.rs` and `node_abi.rs`; a backend only assigns its own symbols and
interface-ID meaning. CAN, SPI, timer, power, and other peripheral interfaces
are backend bindings, not Rig concepts.

Build and test the standalone Rust core with:

```text
buckle test //tools/rig:core_unit
cargo test --manifest-path tools/rig/Cargo.toml
```
