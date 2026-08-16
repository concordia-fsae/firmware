# Rig

Rig is a standalone simulation and scheduling library for models, components,
controllers, and composed systems. It provides the simulation semantics; it
does not know about CAN, SPI, timers, firmware tasks, vehicle peripherals, or
this repository.

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

## What Rig provides

- Typed `Rig`, `Node`, `Cluster`, `Model`, `Component`, and `DataPath`
  composition primitives.
- A generic dataflow graph with scalar edges, event edges, event transforms,
  route compilation, and native or Python algorithm execution.
- A scheduler with simulation time, periodic algorithms, event-driven waits,
  ingress wakeups, cancellation, bounded `run_until` execution, and explicit
  rejection of zero-period scheduled work.
- Shared Python and Rust interfaces. Python models can use the portable Python
  implementation, while a backend can attach the Rust runtime and native model
  ABI for production-like execution.
- Generic periodic scalar sources and model fixtures that can be tested without
  any firmware repository or peripheral implementation.

## Standalone use

The Python package is exposed from `tools/rig/python` as `rig`:

```bash
PYTHONPATH=tools/rig/python python -c \
  "from rig import ClusterRig, Dataflow, RigRuntime; print('Rig is available')"
```

Applications normally construct a `ClusterConfig`, create typed nodes or
components, connect their `DataPath` edges, and advance the resulting
`ClusterRig` with `run_until`. A backend-specific runtime may provide native
symbols and peripheral edges, but the graph, scheduling, wait, wake, and model
lifecycle contracts remain Rig APIs.

## Build and test

Run all standalone Rig tests with:

```text
buckle test //tools/rig/...
cargo test --manifest-path tools/rig/Cargo.toml
```

The Buck targets cover both the Rust core (`core_unit`) and Python facade
(`python_unit`). Firmware-specific tests belong under `sim/`, not in this
standalone package.
