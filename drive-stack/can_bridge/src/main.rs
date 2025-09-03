use socketcan;

fn main() {
    println!("CAN bridge Rust test");
    println!("target = {}-{}-{}", std::env::consts::ARCH, std::env::consts::OS, std::env::consts::FAMILY);

    #[cfg(target_arch = "x86_64")]
    println!("OK: compiled for x86_64");
}
