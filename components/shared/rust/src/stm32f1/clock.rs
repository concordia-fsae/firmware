use stm32f1xx_hal::{pac, prelude::*, rcc::Clocks};

pub struct Config {
    pub hse_mhz: u32,
    pub sysclk_mhz: u32,
    pub pclk1_mhz: u32,
    pub pclk2_mhz: u32,
}
pub fn configure(flash: pac::FLASH, rcc: pac::RCC, config: Config) -> Clocks {
    let mut flash = flash.constrain();
    rcc.constrain()
        .cfgr
        .use_hse(config.hse_mhz.MHz())
        .sysclk(config.sysclk_mhz.MHz())
        .pclk1(config.pclk1_mhz.MHz())
        .pclk2(config.pclk2_mhz.MHz())
        .freeze(&mut flash.acr)
}
/// Set the application's vector table and enable cycle accounting.
///
/// # Safety
/// `vectors` must be an aligned vector table for the currently running application.
pub unsafe fn initialize_core(core: &mut cortex_m::Peripherals, vectors: u32) {
    core.SCB.vtor.write(vectors);
    core.DCB.enable_trace();
    core.DWT.enable_cycle_counter();
}
pub fn cycles() -> u32 {
    cortex_m::peripheral::DWT::cycle_count()
}
pub fn elapsed_cycles(start: u32) -> u64 {
    cycles().wrapping_sub(start) as u64
}
