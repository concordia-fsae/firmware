use std::hash::Hash;

use super::dataflow::{DataflowEdge, DataflowEdgeKey, DataflowEvent};

pub trait InterfaceEndpoint: Copy + Eq + Hash {
    fn dataflow_channel(self) -> super::dataflow::DataflowChannel;
}

pub trait InterfaceImplementation {
    fn reset_interface(&mut self);
}

pub trait InterfaceCaller: InterfaceImplementation {
    fn reset(&mut self) {
        self.reset_interface();
    }

    fn append_algorithm_specs(&self, specs: &mut Vec<super::dataflow::DataflowAlgorithm>);
}

pub trait InterfaceDataflow<T: DataflowEvent>: InterfaceImplementation {
    type Endpoint: InterfaceEndpoint;

    fn edge(node: u32, endpoint: Self::Endpoint) -> DataflowEdgeKey {
        DataflowEdge::<T>::new(node, endpoint.dataflow_channel()).key()
    }
}
