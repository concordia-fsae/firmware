fn main() {
    firmware_build::configure(env!("CARGO_PKG_NAME"), "firmware");
}
