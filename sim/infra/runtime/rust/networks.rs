use std::collections::{HashMap, VecDeque};
use std::hash::Hash;

use super::cluster::{
    CanEvent, ClusterCanRecvEventsFn, ClusterCanSendManyFn, ClusterCanTxCountFn, ClusterSpiCountFn,
    ClusterSpiRecvManyFn, ClusterSpiSendManyFn, DataflowChannel, DataflowEdge, DataflowEdgeKey,
    SpiTransaction,
};

#[derive(Clone, Copy)]
pub(super) struct ClusterCanRoute {
    pub(super) source_node: u32,
    pub(super) source_bus: u8,
    pub(super) source_tx_count: ClusterCanTxCountFn,
    pub(super) source_recv_events: ClusterCanRecvEventsFn,
    pub(super) sink_node: Option<u32>,
    pub(super) sink_bus: u8,
    pub(super) sink_send_many: Option<ClusterCanSendManyFn>,
}

#[derive(Clone, Copy)]
pub(super) struct ClusterCanSink {
    pub(super) sink_node: u32,
    pub(super) sink_bus: u8,
    pub(super) sink_send_many: ClusterCanSendManyFn,
}

#[derive(Clone, Copy)]
pub(super) struct ClusterCanRecord {
    pub(super) source_node: u32,
    pub(super) bus: u8,
    pub(super) event: CanEvent,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub(super) struct CanEndpoint {
    bus: u8,
}

impl CanEndpoint {
    pub(super) fn new(bus: u8) -> Self {
        Self { bus }
    }

    pub(super) fn bus(self) -> u8 {
        self.bus
    }
}

pub(super) struct CanNetworkFanout {
    pub(super) source_node: u32,
    pub(super) endpoint: CanEndpoint,
    pub(super) record_index: usize,
    pub(super) source_tx_count: ClusterCanTxCountFn,
    pub(super) source_recv_events: ClusterCanRecvEventsFn,
    pub(super) sinks: Vec<ClusterCanSink>,
}

struct CanRecordStream {
    records: VecDeque<CanEvent>,
}

#[derive(Clone, Copy)]
pub(super) struct ClusterSpiRoute {
    pub(super) source_node: u32,
    pub(super) device: i32,
    pub(super) source_count: ClusterSpiCountFn,
    pub(super) source_recv_many: ClusterSpiRecvManyFn,
    pub(super) sink_node: u32,
    pub(super) sink_send_many: ClusterSpiSendManyFn,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub(super) struct SpiEndpoint {
    bus: i32,
    chip_select: i32,
}

impl SpiEndpoint {
    pub(super) fn from_device(device: i32) -> Self {
        Self {
            bus: 0,
            chip_select: device,
        }
    }

    pub(super) fn device(self) -> i32 {
        self.chip_select
    }
}

#[derive(Clone, Copy)]
pub(super) struct ClusterSpiSink {
    pub(super) sink_node: u32,
    pub(super) sink_send_many: ClusterSpiSendManyFn,
}

pub(super) struct SpiNetworkFanout {
    pub(super) source_node: u32,
    pub(super) endpoint: SpiEndpoint,
    pub(super) source_count: ClusterSpiCountFn,
    pub(super) source_recv_many: ClusterSpiRecvManyFn,
    pub(super) sinks: Vec<ClusterSpiSink>,
}

pub(super) trait NetworkEndpoint: Copy + Eq + Hash {
    fn dataflow_channel(self) -> DataflowChannel;
}

impl NetworkEndpoint for CanEndpoint {
    fn dataflow_channel(self) -> DataflowChannel {
        DataflowChannel {
            interface: self.bus as i32,
            ..Default::default()
        }
    }
}

impl NetworkEndpoint for SpiEndpoint {
    fn dataflow_channel(self) -> DataflowChannel {
        DataflowChannel {
            interface: self.bus,
            channel: self.chip_select,
            ..Default::default()
        }
    }
}

pub(super) trait RuntimeNetwork {
    fn reset_network(&mut self);
}

pub(super) trait NetworkDataflow<T: 'static>: RuntimeNetwork {
    type Endpoint: NetworkEndpoint;

    fn edge(node: u32, endpoint: Self::Endpoint) -> DataflowEdgeKey {
        DataflowEdge::<T>::new(node, endpoint.dataflow_channel()).key()
    }
}

#[derive(Default)]
pub(super) struct CanNetwork {
    pub(super) fanout_indexes: HashMap<(u32, CanEndpoint), usize>,
    pub(super) fanouts: Vec<CanNetworkFanout>,
    pub(super) native_source_events: VecDeque<ClusterCanRecord>,
    record_indexes: HashMap<(u32, CanEndpoint), usize>,
    records: Vec<CanRecordStream>,
}

impl RuntimeNetwork for CanNetwork {
    fn reset_network(&mut self) {
        self.fanout_indexes.clear();
        self.fanouts.clear();
        self.native_source_events.clear();
        self.record_indexes.clear();
        self.records.clear();
    }
}

impl NetworkDataflow<CanEvent> for CanNetwork {
    type Endpoint = CanEndpoint;
}

impl CanNetwork {
    pub(super) fn reset(&mut self) {
        self.reset_network();
    }

    pub(super) fn upsert_fanout(&mut self, route: ClusterCanRoute) {
        let endpoint = CanEndpoint::new(route.source_bus);
        let key = (route.source_node, endpoint);
        let record_index = self.ensure_record_stream(route.source_node, endpoint);
        let group_index = *self.fanout_indexes.entry(key).or_insert_with(|| {
            self.fanouts.push(CanNetworkFanout {
                source_node: route.source_node,
                endpoint,
                record_index,
                source_tx_count: route.source_tx_count,
                source_recv_events: route.source_recv_events,
                sinks: Vec::new(),
            });
            self.fanouts.len() - 1
        });
        if let (Some(sink_node), Some(sink_send_many)) = (route.sink_node, route.sink_send_many) {
            if self.fanouts[group_index]
                .sinks
                .iter()
                .any(|sink| sink.sink_node == sink_node && sink.sink_bus == route.sink_bus)
            {
                return;
            }
            self.fanouts[group_index].sinks.push(ClusterCanSink {
                sink_node,
                sink_bus: route.sink_bus,
                sink_send_many,
            });
        }
    }

    fn ensure_record_stream(&mut self, source_node: u32, endpoint: CanEndpoint) -> usize {
        let key = (source_node, endpoint);
        *self.record_indexes.entry(key).or_insert_with(|| {
            self.records.push(CanRecordStream {
                records: VecDeque::new(),
            });
            self.records.len() - 1
        })
    }

    pub(super) fn record(&mut self, source_node: u32, bus: u8, event: CanEvent) {
        let endpoint = CanEndpoint::new(bus);
        let stream_index = self.ensure_record_stream(source_node, endpoint);
        self.record_at(stream_index, event);
    }

    pub(super) fn record_at(&mut self, stream_index: usize, event: CanEvent) {
        self.records[stream_index].records.push_back(event);
    }

    pub(super) fn latest_message(
        &self,
        source_node: u32,
        bus: u8,
        message_id: u32,
    ) -> Option<CanEvent> {
        let stream_index = self
            .record_indexes
            .get(&(source_node, CanEndpoint::new(bus)))
            .copied()?;
        self.records[stream_index]
            .records
            .iter()
            .rev()
            .find(|event| event.packet.id == message_id)
            .copied()
    }

    pub(super) fn latest_bus_event(&self, source_node: u32, bus: u8) -> Option<CanEvent> {
        let stream_index = self
            .record_indexes
            .get(&(source_node, CanEndpoint::new(bus)))
            .copied()?;
        self.records[stream_index].records.back().copied()
    }
}

#[derive(Default)]
pub(super) struct SpiNetwork {
    pub(super) fanout_indexes: HashMap<(u32, SpiEndpoint), usize>,
    pub(super) fanouts: Vec<SpiNetworkFanout>,
}

impl RuntimeNetwork for SpiNetwork {
    fn reset_network(&mut self) {
        self.fanout_indexes.clear();
        self.fanouts.clear();
    }
}

impl NetworkDataflow<SpiTransaction> for SpiNetwork {
    type Endpoint = SpiEndpoint;
}

impl SpiNetwork {
    pub(super) fn reset(&mut self) {
        self.reset_network();
    }

    pub(super) fn upsert_fanout(&mut self, route: ClusterSpiRoute) {
        let endpoint = SpiEndpoint::from_device(route.device);
        let key = (route.source_node, endpoint);
        let group_index = *self.fanout_indexes.entry(key).or_insert_with(|| {
            self.fanouts.push(SpiNetworkFanout {
                source_node: route.source_node,
                endpoint,
                source_count: route.source_count,
                source_recv_many: route.source_recv_many,
                sinks: Vec::new(),
            });
            self.fanouts.len() - 1
        });
        if self.fanouts[group_index]
            .sinks
            .iter()
            .any(|sink| sink.sink_node == route.sink_node)
        {
            return;
        }
        self.fanouts[group_index].sinks.push(ClusterSpiSink {
            sink_node: route.sink_node,
            sink_send_many: route.sink_send_many,
        });
    }
}

#[derive(Default)]
pub(super) struct RuntimeNetworks {
    pub(super) can: CanNetwork,
    pub(super) spi: SpiNetwork,
}

impl RuntimeNetworks {
    pub(super) fn reset(&mut self) {
        self.can.reset();
        self.spi.reset();
    }
}
