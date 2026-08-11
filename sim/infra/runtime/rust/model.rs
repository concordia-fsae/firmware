use super::rt_controller::RTController;
use super::rt_controller::Scheduler;

pub const RIG_MODEL_DATAPATH_TIMER_DUTY: u16 = 1;
pub const RIG_MODEL_DATAPATH_TIMER_FREQUENCY: u16 = 2;
pub const RIG_MODEL_DATAPATH_SPI_TRANSACTION: u16 = 3;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct ModelDataPathDescriptor {
    pub interface: u16,
    pub port: i32,
    pub channel: i32,
    pub device: i32,
}

impl ModelDataPathDescriptor {
    pub const fn timer_duty(port: i32, channel: i32) -> Self {
        Self {
            interface: RIG_MODEL_DATAPATH_TIMER_DUTY,
            port,
            channel,
            device: 0,
        }
    }

    pub const fn timer_frequency(port: i32, channel: i32) -> Self {
        Self {
            interface: RIG_MODEL_DATAPATH_TIMER_FREQUENCY,
            port,
            channel,
            device: 0,
        }
    }

    pub const fn spi_transaction(device: i32) -> Self {
        Self {
            interface: RIG_MODEL_DATAPATH_SPI_TRANSACTION,
            port: 0,
            channel: 0,
            device,
        }
    }
}

pub trait NodeTarget {
    unsafe fn reset_node(&mut self, controller: &mut RTController);

    fn datapath_count(&self) -> u32 {
        0
    }

    fn datapath_descriptor(&self, _index: u32) -> Option<ModelDataPathDescriptor> {
        None
    }
}

pub struct NodeModel<Target> {
    controller: RTController,
    target: Target,
}

impl<Target: NodeTarget> NodeModel<Target> {
    pub const fn new(controller: RTController, target: Target) -> Self {
        Self { controller, target }
    }

    pub unsafe fn reset(&mut self) {
        super::spi::reset();
        super::timer::reset();
        self.configure_runtime_datapaths();
        self.controller.reset_runtime();
        unsafe { self.target.reset_node(&mut self.controller) };
        unsafe { self.controller.reset() };
    }

    pub unsafe fn run_for_ns(&mut self, elapsed_ns: u64) {
        unsafe { self.controller.run_for_ns(elapsed_ns) };
    }

    pub unsafe fn fast_forward_for_ns(&mut self, elapsed_ns: u64) {
        unsafe { self.controller.fast_forward_for_ns(elapsed_ns) };
    }

    pub fn next_scheduler_step_ns(&self, max_step_ns: u64) -> u64 {
        self.controller.next_scheduler_step_ns(max_step_ns)
    }

    pub fn controller(&mut self) -> &mut RTController {
        &mut self.controller
    }

    pub fn target(&mut self) -> &mut Target {
        &mut self.target
    }

    pub fn datapath_count(&self) -> u32 {
        self.target.datapath_count()
    }

    pub fn datapath_descriptor(&self, index: u32) -> Option<ModelDataPathDescriptor> {
        self.target.datapath_descriptor(index)
    }

    fn configure_runtime_datapaths(&self) {
        for index in 0..self.target.datapath_count() {
            let Some(descriptor) = self.target.datapath_descriptor(index) else {
                continue;
            };
            match descriptor.interface {
                RIG_MODEL_DATAPATH_TIMER_DUTY => {
                    super::timer::configure_channel(descriptor.port, descriptor.channel);
                }
                RIG_MODEL_DATAPATH_TIMER_FREQUENCY => {
                    super::timer::configure_channel(descriptor.port, descriptor.channel);
                }
                RIG_MODEL_DATAPATH_SPI_TRANSACTION => {
                    super::spi::configure_device(descriptor.device);
                }
                _ => {}
            }
        }
    }
}

#[macro_export]
macro_rules! rig_model_abi {
    ($model:expr) => {
        #[unsafe(no_mangle)]
        pub extern "C" fn rig_model_new() {
            unsafe {
                $model.lock().unwrap().reset();
            }
        }

        #[unsafe(no_mangle)]
        pub extern "C" fn rig_model_run_for(elapsed_ns: u64) {
            unsafe {
                $model.lock().unwrap().run_for_ns(elapsed_ns);
            }
        }

        #[unsafe(no_mangle)]
        pub extern "C" fn rig_model_fast_forward_for(elapsed_ns: u64) {
            unsafe {
                $model.lock().unwrap().fast_forward_for_ns(elapsed_ns);
            }
        }

        #[unsafe(no_mangle)]
        pub extern "C" fn rig_model_next_scheduler_step(max_step_ns: u64) -> u64 {
            $model.lock().unwrap().next_scheduler_step_ns(max_step_ns)
        }

        #[unsafe(no_mangle)]
        pub extern "C" fn rig_model_datapath_count() -> u32 {
            $model.lock().unwrap().datapath_count()
        }

        #[unsafe(no_mangle)]
        pub extern "C" fn rig_model_datapath_descriptor(
            index: u32,
            out: *mut $crate::rig_runtime::model::ModelDataPathDescriptor,
        ) -> bool {
            if out.is_null() {
                return false;
            }
            match $model.lock().unwrap().datapath_descriptor(index) {
                Some(descriptor) => {
                    unsafe { *out = descriptor };
                    true
                }
                None => false,
            }
        }
    };
}
