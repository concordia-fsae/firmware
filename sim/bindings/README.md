# Simulation bindings

`sim/bindings` is the firmware-repository implementation layer for
[Rig](../../tools/rig/README.md). It adapts the generic Rig scheduler,
dataflow, node, and model contracts to the firmware interfaces used by this
repository. Rig is intentionally unaware of these bindings.

## Ownership

The `firmware/` subtree owns the firmware-specific simulation interfaces:

- `can/` models CAN packets, messages, signals, network ingress, and CAN edge
  wakeups.
- `faults/`, `flash/`, `i2c/`, `io/`, `nvm/`, `power/`, `spi/`, `timer/`, and
  `uart/` implement the corresponding peripheral or platform adapters.
- `runtime/` provides the firmware runtime bridge, generated-node ABI,
  controller/cluster integration, C runtime shims, and firmware-facing Python
  facade.

Each binding keeps its implementation close to its interface. Where needed,
the directory contains C sources and headers, Rust adapters, and Python
helpers. `sim/bindings/defs.bzl` composes these firmware modules with the
generic Rust sources exported by `tools/rig`.

## Python use

Import firmware bindings from their public package paths:

```python
from sim.bindings.firmware.can import CanInterface
from sim.bindings.firmware.spi import SpiInterface
from sim.bindings.firmware.timer import TimerInterface
from sim.bindings.firmware.runtime import FirmwareClusterRig, require_peripheral_binding
```

The runtime package re-exports its public Python API. Consumers should import
from `sim.bindings.firmware.runtime`, not from the nested
`runtime/python/` implementation directory.

Bindings expose typed edges and events to Rig. For example, a CAN signal wake
is an ingress event on a dataflow edge; the generic Rig scheduler owns the
dependent algorithm wakeup after the binding notifies that edge. The binding
owns CAN-specific decoding and registration, while Rig owns waiting,
cancellation, ordering, and scheduler progress.

## Build and test

RTIC applications use `runtime/rust/rtic_controller.rs` with the pure Rust host
runtime. `RticApplication` supplies CAN and timer task entry points and reset
handling. `RticController` dispatches CAN before each millisecond timer interrupt,
catches up every tick during coarse steps and preserves fractional nanoseconds.
The SWS adapter invokes the same timer-task body and bounded CAN queues as the
STM32 RTIC application. This models task behavior and ordering; Cortex-M cycles,
NVIC preemption and physical CAN timing require hardware or a CPU emulator.

`rig_embedded_controller_model(rust_application = "//components/sws:rust-sources", ...)`
selects the Cargo host build path. Existing Python/model ABI and platform aliases
are retained. Controllers without `rust_application` continue using their C
runtime. SWS integration checks run with `buckle test //sim/tests:sws-cfr26`.

The binding graph is normally exercised through the complete simulation test
graph:

```bash
buckle test //sim/bindings/firmware/runtime/...
buckle test //sim/...
```

The first command covers the firmware runtime contract and native runtime
tests. The second also builds the model runtimes, controller integrations, and
all component and vehicle simulations. The bindings are not a standalone
simulation framework: they require the generic `rig` package and the generated
firmware/model artifacts supplied by the surrounding Buck targets.
