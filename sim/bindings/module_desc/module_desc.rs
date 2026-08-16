pub type ModuleTask = unsafe extern "C" fn();

#[repr(C)]
pub struct ModuleDesc {
    pub module_init: Option<ModuleTask>,
    pub periodic_1khz_clk: Option<ModuleTask>,
    pub periodic_100hz_clk: Option<ModuleTask>,
    pub periodic_10hz_clk: Option<ModuleTask>,
    pub periodic_1hz_clk: Option<ModuleTask>,
}

impl ModuleDesc {
    pub const fn new(
        module_init: Option<ModuleTask>,
        periodic_1khz_clk: Option<ModuleTask>,
        periodic_100hz_clk: Option<ModuleTask>,
        periodic_10hz_clk: Option<ModuleTask>,
        periodic_1hz_clk: Option<ModuleTask>,
    ) -> Self {
        Self {
            module_init,
            periodic_1khz_clk,
            periodic_100hz_clk,
            periodic_10hz_clk,
            periodic_1hz_clk,
        }
    }
}
