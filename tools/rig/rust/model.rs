/// Runtime operations supplied by a simulation backend to a Rig model.
///
/// Rig owns the model lifecycle, while firmware bindings provide the concrete
/// controller that implements these operations. The backend owns all concrete
/// I/O and scheduling state; Rig only coordinates the lifecycle.
pub trait ModelRuntime {
    unsafe fn reset_backend(&mut self);

    fn reset_runtime(&mut self);

    unsafe fn reset_scheduler(&mut self);

    unsafe fn run_for_ns(&mut self, elapsed_ns: u64);
}

pub trait NodeTarget<Runtime: ModelRuntime> {
    unsafe fn reset_node(&mut self, controller: &mut Runtime);
}

pub struct NodeModel<Target, Runtime> {
    controller: Runtime,
    target: Target,
}

impl<Target, Runtime> super::rig::RigElement for NodeModel<Target, Runtime>
where
    Runtime: ModelRuntime,
    Target: NodeTarget<Runtime>,
{
    unsafe fn reset(&mut self) {
        unsafe { NodeModel::reset(self) };
    }

    unsafe fn run_for_ns(&mut self, duration_ns: u64) {
        unsafe { NodeModel::run_for_ns(self, duration_ns) };
    }
}

impl<Target, Runtime> NodeModel<Target, Runtime>
where
    Runtime: ModelRuntime,
    Target: NodeTarget<Runtime>,
{
    pub const fn new(controller: Runtime, target: Target) -> Self {
        Self { controller, target }
    }

    pub unsafe fn reset(&mut self) {
        unsafe { self.controller.reset_backend() };
        self.controller.reset_runtime();
        unsafe { self.target.reset_node(&mut self.controller) };
        unsafe { self.controller.reset_scheduler() };
    }

    pub unsafe fn run_for_ns(&mut self, elapsed_ns: u64) {
        unsafe { self.controller.run_for_ns(elapsed_ns) };
    }

    pub fn controller(&mut self) -> &mut Runtime {
        &mut self.controller
    }

    pub fn target(&mut self) -> &mut Target {
        &mut self.target
    }
}
