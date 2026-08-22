use super::can::{CanEndpoint, CanInterface};
pub(super) use super::can::{
    CanEvent, CanPacket, CanSignalComparison, CanSignalDecoderFn,
};
pub(super) use super::can::{CanRouteResult, ClusterCanRoute};
use super::dataflow::{DataflowAlgorithm, DataflowEdgeKey, DataflowWait};
use super::interfaces::{InterfaceCaller, InterfaceDataflow};
pub(super) use super::scalar::ScalarEvent;
use super::spi::SpiEndpoint;
use super::spi::SpiInterface;
pub(super) use super::spi::SpiTransaction;
pub(super) use super::spi::ClusterSpiRoute;
use super::timer::{TimerEndpoint, TimerInterface, TimerRouteResult};
pub(super) use super::timer::TimerChannelEvent;
pub(super) use super::timer::ClusterTimerRoute;

pub(super) enum InterfaceRoute {
    Can(ClusterCanRoute),
    Timer(ClusterTimerRoute),
    Spi(ClusterSpiRoute),
}

impl InterfaceRoute {
    pub(super) fn nodes(&self) -> (u32, Option<u32>) {
        match self {
            Self::Can(route) => (route.source_node, route.sink_node),
            Self::Timer(route) => (
                route.source_node,
                (route.sink_node != u32::MAX).then_some(route.sink_node),
            ),
            Self::Spi(route) => (route.source_node, Some(route.sink_node)),
        }
    }
}

pub(super) trait RuntimeInterface {
    fn reset(&mut self);
    fn register_route(&mut self, route: InterfaceRoute);
    fn append_algorithm_specs(&self, specs: &mut Vec<DataflowAlgorithm>);
}

#[derive(Default)]
pub(super) struct RuntimeInterfaces {
    pub(super) can: CanInterface,
    pub(super) spi: SpiInterface,
    pub(super) timer: TimerInterface,
}

impl RuntimeInterface for RuntimeInterfaces {
    fn reset(&mut self) {
        self.can.reset();
        self.spi.reset();
        self.timer.reset();
    }

    fn register_route(&mut self, route: InterfaceRoute) {
        match route {
            InterfaceRoute::Can(route) => self.can.upsert_fanout(route),
            InterfaceRoute::Timer(route) => self.timer.upsert_fanout(route),
            InterfaceRoute::Spi(route) => self.spi.upsert_fanout(route),
        }
    }

    fn append_algorithm_specs(&self, specs: &mut Vec<DataflowAlgorithm>) {
        self.can.append_algorithm_specs(specs);
        self.timer.append_algorithm_specs(specs);
        self.spi.append_algorithm_specs(specs);
    }
}

impl RuntimeInterfaces {
    pub(super) fn can_native_source_pending(
        &self, source_online: impl FnMut(u32) -> bool,
    ) -> bool {
        self.can.native_source_pending(source_online)
    }

    pub(super) fn can_pop_native_source_event(&mut self) -> Option<super::can::ClusterCanRecord> {
        self.can.pop_native_source_event()
    }

    pub(super) fn can_route_event(
        &mut self, source_node: u32, bus: u8, event: CanEvent,
    ) -> CanRouteResult {
        self.can.route_event(source_node, bus, event)
    }

    pub(super) fn begin_can_signal_wait(
        &mut self,
        source_node: u32,
        comparisons: &[CanSignalComparison],
        decoder: CanSignalDecoderFn,
        wait: DataflowWait,
    ) -> bool {
        self.can
            .begin_signal_wait(source_node, comparisons, decoder, wait)
    }

    pub(super) fn cancel_dataflow_wait(&mut self, wait: DataflowWait) {
        self.can.cancel_signal_wait(wait);
    }

    pub(super) fn timer_fanout_pending(
        &self, group_index: usize, source_online: impl FnMut(u32) -> bool,
    ) -> bool {
        self.timer.fanout_pending(group_index, source_online)
    }

    pub(super) fn timer_route_fanout(
        &mut self, group_index: usize, source_online: impl FnMut(u32) -> bool,
    ) -> Option<TimerRouteResult> {
        self.timer.route_fanout(group_index, source_online)
    }

    pub(super) fn update_scaled_scalar_source(&mut self, result: &TimerRouteResult) {
        self.timer.update_scaled_scalar_source(
            result.source_node, result.interface, result.port, result.channel, &result.events,
        );
    }

    pub(super) fn take_scaled_scalar_event(
        &mut self, source_index: usize, elapsed_ns: u64,
    ) -> Option<(u32, u32, ScalarEvent)> {
        self.timer.take_scaled_scalar_event(source_index, elapsed_ns)
    }

    pub(super) fn add_scaled_scalar_source(
        &mut self, node: u32, route_id: u32, timer_interface: u16, timer_port: i32,
        timer_channel: i32, scale_route_id: u32, scale_value: f32, scale: f32, offset: f32,
    ) {
        self.timer.add_scaled_scalar_source(
            node, route_id, timer_interface, timer_port, timer_channel, scale_route_id,
            scale_value, scale, offset,
        );
    }

    pub(super) fn update_scaled_scalar_scale(
        &mut self, node: u32, route_id: u32, value: f32,
    ) {
        self.timer.update_scaled_scalar_scale(node, route_id, value);
    }

    pub(super) fn reset_node_interfaces(&mut self, node: u32) {
        self.timer.reset_node_models(node);
    }

    pub(super) fn send_native_can_source_event(
        &mut self, source_node: u32, bus: u8, elapsed_ns: u64, packet: CanPacket,
    ) {
        self.can.push_native_source_event(super::can::ClusterCanRecord {
            source_node,
            bus,
            event: CanEvent { bus, timestamp_ns: elapsed_ns, packet },
        });
    }

    pub(super) fn latest_can_message(
        &self, source_node: u32, bus: u8, message_id: u32,
    ) -> Option<CanEvent> {
        self.can.latest_message(source_node, bus, message_id)
    }

    pub(super) fn latest_can_event(&self, source_node: u32, bus: u8) -> Option<CanEvent> {
        self.can.latest_bus_event(source_node, bus)
    }

    pub(super) fn latest_timer_event(
        &self, source_node: u32, interface: u16, port: i32, channel: i32,
    ) -> Option<TimerChannelEvent> {
        self.timer.latest(source_node, interface, port, channel)
    }

    pub(super) fn decode_can_signal(
        &self, bus: u8, packet: &CanPacket, signal_name: &str,
    ) -> Option<f64> {
        super::can::decode_signal(bus, packet, signal_name)
    }

    pub(super) fn timer_edge(
        node: u32, interface: u16, port: i32, channel: i32,
    ) -> DataflowEdgeKey {
        <TimerInterface as InterfaceDataflow<TimerChannelEvent>>::edge(
            node,
            TimerEndpoint::new(interface, port, channel),
        )
    }

    pub(super) fn spi_edge(node: u32, device: i32) -> DataflowEdgeKey {
        <SpiInterface as InterfaceDataflow<SpiTransaction>>::edge(
            node,
            SpiEndpoint::from_device(device),
        )
    }

    pub(super) fn can_edge(node: u32, bus: u8) -> DataflowEdgeKey {
        <CanInterface as InterfaceDataflow<CanEvent>>::edge(node, CanEndpoint::new(bus))
    }

}
