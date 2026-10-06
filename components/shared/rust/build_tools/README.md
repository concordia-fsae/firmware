# Shared firmware build tools

`defs.bzl` provides `rust_firmware_targets` for a component's Buckle file. It
creates the standard `rust-protocol`, `rust-firmware`, `rust-tests`,
`rust-build-tests`, `elf`, `bin` and `bin_crc` targets, plus the application alias. Optional platform variants add
the existing download/deploy/OTA targets; a configured MCU adds GDB targets.

The application supplies its source map, repository-relative Cargo package path,
CRC start address, GCC toolchain and platform variants.
Rust version, embedded target, feature, Yamcan node/network, dependency source
providers and additional host test packages are configurable. Existing SWS target
names and deployment commands are preserved.

```python
load("//components/shared/rust/build_tools:defs.bzl", "rust_firmware_targets")

rust_firmware_targets(
    app_name = "controller",
    package_path = "components/controller/rust",
    srcs = {path[len("rust/"):]: path for path in glob([
        "rust/src/**/*.rs", "rust/*.rs", "rust/*.toml", "rust/Cargo.lock", "rust/*.x",
    ])},
    app_start_address = 0x08002000,
    gcc_toolchain = "@toolchains//:gcc-14.2.rel1-arm-none-eabi",
)
```

The lower-level `firmware_cargo` rule supports other declared Cargo layouts.
Each dependency uses `cargo_sources` with a source map and repository-relative
`path`. The shared runtime and build-script helper have source providers by
default. Additional local Cargo dependencies must also be declared this way.

`build_firmware.py` stages only declared sources, preserves local dependency
paths, runs the selected Rust toolchain with `--locked`, and exposes the binary's
ELF/map or a passing host-test marker. `FIRMWARE_BUILD_TMPDIR` selects the scratch
volume; set it before starting the Buckle daemon. The runner clears ambient
Rust flags, target selection and output-directory overrides.

This directory also contains the `firmware-build` Cargo build dependency. A
consumer's `build.rs` calls:

```rust
fn main() {
    firmware_build::configure(env!("CARGO_PKG_NAME"), "firmware");
}
```

It stages the Yamcan wrapper from `FIRMWARE_PROTOCOL` and configures the linker
for the named binary when the supplied feature is enabled. The application owns
its `memory.x` and `descriptor.x` linker scripts. Buckle supplies the protocol
directory; direct Cargo commands must set `FIRMWARE_PROTOCOL` themselves.

The application owns the bootloader/linker layout. `bin_crc` appends the STM32 CRC
using the existing hextools rule; deployment consumes that artifact directly.

`rust-tests` runs application/shared host tests. `rust-build-tests` checks that
staging rejects paths outside its directory and supports a differently named
package, binary, target, feature and dependency layout.
