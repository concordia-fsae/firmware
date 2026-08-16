//! Standalone Rig core. Firmware bindings and model implementations are
//! composed by the consuming simulation backend, never by this crate.

pub mod algorithms;
pub mod dataflow;
pub mod datapath;
pub mod interfaces;
pub mod node;
pub mod runtime;
pub mod scalar;
pub mod scheduler;

pub mod model;

pub mod model_abi;
pub mod node_abi;

pub mod rig;

pub use dataflow::{
    DataflowAlgorithm, DataflowAlgorithmExecutor, DataflowChannel, DataflowEdge, DataflowEdgeKey,
    DataflowEvent, DataflowRuntime, DataflowSchedule, DataflowWait, ScalarEvent,
};
pub use datapath::{DataPath, DataPathEvent};
pub use interfaces::{
    InterfaceCaller, InterfaceDataflow, InterfaceEndpoint, InterfaceImplementation,
};
pub use model::{ModelRuntime, NodeModel, NodeTarget};
pub use node::{RigNode, RigNodeResetFn, RigNodeRunForFn, RigNodeScheduler, RigPythonScheduledFn};
pub use rig::{Rig, RigElement};
pub use runtime::{NoBackend, RigBackend, RigRuntime};
pub use scalar::{
    ScalarCountFn, ScalarEndpoint, ScalarInterface, ScalarRecvManyFn, ScalarRoute,
    ScalarSendManyFn, ScalarSink, ScalarSinkSetFn,
};
pub use scheduler::{RigScheduler, SchedulerCallbackContext};

#[cfg(test)]
mod tests {
    use super::datapath::{DataPath, DataPathEvent};
    use super::model::{ModelRuntime, NodeModel, NodeTarget};

    #[derive(Clone, Copy, Debug, PartialEq)]
    struct Event(u8);

    impl DataPathEvent for Event {
        type Channel = u8;

        fn channel(&self) -> Self::Channel {
            self.0
        }
    }

    #[test]
    fn datapath_is_an_independent_core_primitive() {
        let mut path = DataPath::new(1);
        assert!(path.push(Event(1)));
        assert!(!path.push(Event(2)));
        assert_eq!(path.channel(), 1);
        assert_eq!(path.count(), 1);
        assert_eq!(path.latest(), Some(Event(1)));
        assert_eq!(path.pop(), Some(Event(1)));
        assert_eq!(path.count(), 0);
        path.clear();
        assert_eq!(path.latest(), None);
    }

    struct Runtime;

    impl ModelRuntime for Runtime {
        unsafe fn reset_backend(&mut self) {}

        fn reset_runtime(&mut self) {}

        unsafe fn reset_scheduler(&mut self) {}

        unsafe fn run_for_ns(&mut self, _elapsed_ns: u64) {}
    }

    struct Target;

    impl NodeTarget<Runtime> for Target {
        unsafe fn reset_node(&mut self, _controller: &mut Runtime) {}
    }

    #[test]
    fn model_lifecycle_is_backend_agnostic() {
        let mut model = NodeModel::new(Runtime, Target);
        unsafe { model.reset() };
        unsafe { model.run_for_ns(1) };
    }
}
