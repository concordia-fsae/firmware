# Simulation models

`sim/models` contains this firmware repository's component, controller, and
vehicle behavior implemented on top of [Rig](../../tools/rig/README.md) and
the firmware bindings in [sim/bindings](../bindings/README.md).

## Model layers

- `components/` contains reusable physical and peripheral models such as the
  ASM330 sensor, battery source, DC load, and drivetrain. Each model exposes a
  Python model contract and may provide a native Rust implementation for the
  hot path.
- `controllers/` contains the simulation-facing wrappers for firmware
  controllers. Generated controller APIs are extended with typed peripheral
  bindings and model composition for each controller variant.
- `vehicle/` composes controller and component models into a vehicle-level
  scenario.
- `catalog.py`, fixtures, and platform/variant modules describe model
  selection and test configuration; they do not define generic Rig behavior.

`components/model_runtime.rs` is the standalone native runtime used by the
component tests. Controller build targets explicitly select the native model
modules they need, so a controller does not accidentally depend on every model
in the repository.

## How a model runs

A model declares typed `DataPath` inputs and outputs, binds those paths to
peripheral or controller interfaces, and registers algorithms with Rig. The
Rig scheduler then coordinates periodic work, event transforms, and ingress
wakes. Firmware-specific CAN, SPI, timer, IO, and power behavior comes from
`sim/bindings`; model equations and state transitions stay here.

Python model code is useful for readable specifications, fixtures, and
portable behavior. Native Rust model code is selected by the relevant Buck
target when performance or ABI integration requires it. Both use the same Rig
dataflow and model contracts.

## Running model tests

Run an individual component or controller test with Buck:

```bash
buckle test //sim/tests:dc_load
buckle test //sim/tests:drivetrain
buckle test //sim/tests:battery_source
buckle test //sim/tests:vcpdu_cluster
```

Run every model, controller, binding, and vehicle simulation test with:

```bash
buckle test //sim/...
```

Tests under `sim/tests/` are the integration surface. They exercise model
composition through the same typed binding and scheduler interfaces used by
the controller simulations, including periodic CAN feedback and event-driven
wake behavior.
