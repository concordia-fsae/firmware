#![no_std]
pub mod app;
pub mod can;
pub mod input;
pub mod screen;
pub use firmware_shared::uds;
#[allow(dead_code, non_upper_case_globals)]
pub mod protocol {
    include!(concat!(env!("OUT_DIR"), "/protocol.rs"));
}
#[cfg(test)]
mod tests;
