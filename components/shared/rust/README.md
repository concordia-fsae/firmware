# Shared Rust firmware support

`firmware-shared` is an allocation-free `no_std` crate for embedded applications.
SWS consumes it as a local Cargo dependency. Buckle declares every shared source
through `//components/shared/rust:sources` and copies the crate alongside the
application into the isolated Cargo build directory.

The hardware-independent modules are:

- `can`: frame type, receive validation and freshness, plus periodic transmit
  deadlines and rolling counters. Applications adapt their Yamcan-generated
  message and signal types to the `Message` and `Signal` traits. Yamcan supplies
  the wire encoding, decoding and checksum implementation.
- `uds`: ISO-TP request assembly, diagnostic sessions, reset timing, bootloader
  descriptor access and repository-compatible diagnostic services. Applications
  supply request and response CAN IDs and their bootloader descriptor.
- `input`: debounced input arrays with configurable press/release thresholds and
  input counts.
- `time`: wrapping millisecond timers and hold detection.

The optional `stm32f1` feature adds clock/vector setup and cycle accounting,
active-low GPIO sampling, CAN configuration and interrupt receive buffering,
and transmit queues that coalesce periodic state while preserving diagnostic
response order. The application supplies clock values, CAN timing, filter IDs,
queue capacities and physical pin assignments. It owns RTIC interrupts and locks.

The current hardware feature supports STM32F103 medium-density devices. Other
microcontrollers can reuse the hardware-independent modules and add their own
hardware module.

SWS retains its pin map, Yamcan adapters, button thresholds, telemetry contents,
driver combinations, screen behavior and vehicle sleep policy. For integration
examples, see `components/sws/rust/src/can.rs`, `input.rs`, `app.rs` and `main.rs`.

`buckle build //components/sws:rust-tests` runs both SWS and shared library tests.

Shared Cargo/Buckle build rules, build-script support and image validation are
in [`build_tools`](build_tools/README.md). Consumers use `rust_firmware_targets`
in their Buckle file and retain their own source layout and linker configuration.
