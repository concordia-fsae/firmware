unsafe extern "C" {
    fn rig_runtime_advance_time_ns(elapsed_ns: u64);
}

/// Firmware application metadata exported alongside the controller model.
#[repr(C)]
pub struct AppDesc {
    pub app_start: u32,
    pub app_end: u32,
    pub app_crc_location: u32,
    pub app_component_id: u16,
    pub app_variant_id: u16,
}

impl AppDesc {
    pub const fn new(
        app_start: u32,
        app_end: u32,
        app_crc_location: u32,
        app_component_id: u16,
        app_variant_id: u16,
    ) -> Self {
        Self {
            app_start,
            app_end,
            app_crc_location,
            app_component_id,
            app_variant_id,
        }
    }
}

/// Firmware module lifecycle callbacks consumed by the controller scheduler.
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

pub type TaskFn = unsafe fn();

pub const MAX_PERIODIC_TASKS: usize = 16;

#[derive(Clone, Copy)]
pub struct PeriodicTask {
    pub period_ns: u64,
    pub task: TaskFn,
}

impl PeriodicTask {
    pub const fn new(period_ns: u64, task: TaskFn) -> Self {
        Self { period_ns, task }
    }
}

pub struct TaskCallbacks {
    pub init: TaskFn,
    pub periodic_tasks: &'static [PeriodicTask],
}

pub trait Scheduler {
    unsafe fn reset(&mut self);
    unsafe fn run_for_ns(&mut self, elapsed_ns: u64);
}

unsafe extern "C" {
    fn Module_Init();
    fn Module_1kHz_TSK();
    fn Module_100Hz_TSK();
    fn Module_10Hz_TSK();
    fn Module_1Hz_TSK();
}

unsafe fn embedded_module_init() {
    unsafe { Module_Init() };
}

unsafe fn embedded_module_tick_1khz() {
    unsafe { Module_1kHz_TSK() };
}

unsafe fn embedded_module_tick_100hz() {
    unsafe { Module_100Hz_TSK() };
}

unsafe fn embedded_module_tick_10hz() {
    unsafe { Module_10Hz_TSK() };
}

unsafe fn embedded_module_tick_1hz() {
    unsafe { Module_1Hz_TSK() };
}

const EMBEDDED_MODULE_PERIODIC_TASKS: [PeriodicTask; 4] = [
    PeriodicTask::new(1_000_000, embedded_module_tick_1khz),
    PeriodicTask::new(10_000_000, embedded_module_tick_100hz),
    PeriodicTask::new(100_000_000, embedded_module_tick_10hz),
    PeriodicTask::new(1_000_000_000, embedded_module_tick_1hz),
];

const EMBEDDED_MODULE_CALLBACKS: TaskCallbacks = TaskCallbacks {
    init: embedded_module_init,
    periodic_tasks: &EMBEDDED_MODULE_PERIODIC_TASKS,
};

pub struct RTController {
    callbacks: TaskCallbacks,
    task_remainders_ns: [u64; MAX_PERIODIC_TASKS],
}

impl super::model::ModelRuntime for RTController {
    unsafe fn reset_backend(&mut self) {
        super::spi::reset();
        super::timer::reset();
        super::spi::reset_chip_selects();
    }

    fn reset_runtime(&mut self) {
        RTController::reset_runtime(self);
    }

    unsafe fn reset_scheduler(&mut self) {
        unsafe { <Self as Scheduler>::reset(self) };
    }

    unsafe fn run_for_ns(&mut self, elapsed_ns: u64) {
        unsafe { <Self as Scheduler>::run_for_ns(self, elapsed_ns) };
    }
}

impl RTController {
    pub const fn new(callbacks: TaskCallbacks) -> Self {
        Self {
            callbacks,
            task_remainders_ns: [0; MAX_PERIODIC_TASKS],
        }
    }

    pub const fn new_embedded_module() -> Self {
        Self::new(EMBEDDED_MODULE_CALLBACKS)
    }

    pub fn reset_runtime(&mut self) {
        super::reset::reset();
        super::can::reset();
        self.task_remainders_ns = [0; MAX_PERIODIC_TASKS];
    }

    pub fn advance_time_ns(&self, elapsed_ns: u64) {
        unsafe { rig_runtime_advance_time_ns(elapsed_ns) };
    }

    pub fn set_analog_input(&self, channel: i32, voltage: f32) {
        super::io::set_analog_input(channel, voltage);
    }

    pub fn get_analog_input(&self, channel: i32) -> f32 {
        super::io::get_analog_input(channel)
    }

    pub fn set_digital(&self, channel: i32, state: bool) {
        super::io::set_digital(channel, state);
    }

    pub fn get_digital(&self, channel: i32) -> bool {
        super::io::get_digital(channel)
    }

    pub fn get_fault(&self, fault: i32) -> bool {
        super::faults::get(fault)
    }

    pub fn can_bus_count(&self) -> u8 {
        super::can::bus_count()
    }

    pub fn can_send(&self, bus: u8, packet: &super::can::CanPacket) -> bool {
        super::can::send(bus, packet)
    }

    pub fn can_recv(&self, bus: u8) -> Option<super::can::CanPacket> {
        super::can::recv(bus)
    }

    pub fn can_rx_count(&self, bus: u8) -> u32 {
        super::can::rx_count(bus)
    }

    pub fn can_tx_count(&self, bus: u8) -> u32 {
        super::can::tx_count(bus)
    }
}

impl Scheduler for RTController {
    unsafe fn reset(&mut self) {
        self.task_remainders_ns = [0; MAX_PERIODIC_TASKS];
        unsafe { (self.callbacks.init)() };
    }

    unsafe fn run_for_ns(&mut self, elapsed_ns: u64) {
        if elapsed_ns == 0 {
            return;
        }

        unsafe { rig_runtime_advance_time_ns(elapsed_ns) };

        for index in 0..self.active_periodic_task_count() {
            let task = self.callbacks.periodic_tasks[index];
            if task.period_ns == 0 {
                continue;
            }
            self.task_remainders_ns[index] =
                self.task_remainders_ns[index].saturating_add(elapsed_ns);
            if self.task_remainders_ns[index] >= task.period_ns {
                unsafe { (task.task)() };
                self.task_remainders_ns[index] %= task.period_ns;
            }
        }
    }
}

impl RTController {
    fn active_periodic_task_count(&self) -> usize {
        self.callbacks.periodic_tasks.len().min(MAX_PERIODIC_TASKS)
    }
}

#[macro_export]
macro_rules! rig_rt_controller_nvm_storage {
    ($controller_nvm:ty) => {
        core::arch::global_asm!(
            ".pushsection .bss.rig_nvm,\"aw\",@nobits",
            ".balign 2",
            ".global __FLASH_NVM_ORIGIN",
            "__FLASH_NVM_ORIGIN:",
            ".zero {nvm_end_offset_bytes}",
            ".global __FLASH_NVM_END",
            "__FLASH_NVM_END:",
            ".zero {storage_bytes}",
            ".popsection",
            nvm_end_offset_bytes = const <$controller_nvm>::END_OFFSET_BYTES,
            storage_bytes = const <$controller_nvm>::STORAGE_WORD_BYTES,
        );
    };
}
