//! Cargo build-script support for Yamcan/bootloader-compatible firmware.
use std::{env, fs, path::PathBuf};

/// Stage the generated Yamcan wrapper and configure the embedded binary linker.
/// Applications own `memory.x` and `descriptor.x` in their package directory.
pub fn configure(binary: &str, firmware_feature: &str) {
    println!("cargo:rerun-if-env-changed=FIRMWARE_PROTOCOL");
    println!("cargo:rerun-if-changed=memory.x");
    println!("cargo:rerun-if-changed=descriptor.x");
    let protocol = env::var("FIRMWARE_PROTOCOL")
        .expect("set FIRMWARE_PROTOCOL to the Buckle-generated Yamcan wrapper directory");
    let protocol = PathBuf::from(protocol).join("rust_embedded_generated.rs");
    println!("cargo:rerun-if-changed={}", protocol.display());
    let out = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    fs::copy(protocol, out.join("protocol.rs")).unwrap();
    let feature = format!(
        "CARGO_FEATURE_{}",
        firmware_feature.to_uppercase().replace('-', "_")
    );
    if env::var_os(feature).is_some() {
        fs::copy("memory.x", out.join("memory.x")).unwrap();
        fs::copy("descriptor.x", out.join("descriptor.x")).unwrap();
        println!("cargo:rustc-link-search={}", out.display());
        println!("cargo:rustc-link-arg-bin={binary}=-Tlink.x");
        println!("cargo:rustc-link-arg-bin={binary}=-Tdescriptor.x");
        println!("cargo:rustc-link-arg-bin={binary}=-Map={binary}.map");
    }
}
