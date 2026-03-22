use std::error::Error;

mod rust_decode_generated;
mod rust_model_generated;
pub mod yamcan;

pub use rust_decode_generated::GeneratedNetwork;
pub use rust_decode_generated::{forward_route_for_bus, forward_route_for_pair, forward_routes_from_bus};
pub use rust_model_generated::{AnyMessage, Bus};
pub use yamcan::{
    bus_descriptor_for, BusBinding, BusDescriptor, BusInterfaceType, BusRouter, CanData,
    CanFrame, DecodedCanMessage, DecodedMessage, ForwardMessage, ForwardPolicy, ForwardRoute,
    MessageDecodeResult, MessageMetadata, NetworkBus, ReceivedCanMessage, SignalAccessor,
    SignalMeasurement, UnhandledMessage,
};

pub type YamcanRouter = BusRouter<rust_decode_generated::GeneratedNetwork>;

pub fn init() {
    yamcan::init::<rust_decode_generated::GeneratedNetwork>();
}

pub fn configure(
    specs: &[String],
    iface_bus_map: &[(&str, Bus)],
) -> Result<(Vec<String>, YamcanRouter), Box<dyn Error>> {
    YamcanRouter::from_specs(specs, iface_bus_map)
}

pub fn configure_iface(
    iface: &str,
    iface_bus_map: &[(&str, Bus)],
) -> Result<BusBinding<Bus>, Box<dyn Error>> {
    yamcan::binding_for_iface::<rust_decode_generated::GeneratedNetwork>(iface, iface_bus_map)
}

pub fn maybe_decode(
    binding_opt: Option<&BusBinding<Bus>>,
    frame: &CanFrame,
    id_masked: u32,
    allow_empty_signals: bool,
    ignore_msg_filter: bool,
    msg_filters: &[String],
    sig_filters: &[String],
) -> Option<DecodedMessage> {
    yamcan::maybe_decode::<rust_decode_generated::GeneratedNetwork>(
        binding_opt,
        frame,
        id_masked,
        allow_empty_signals,
        ignore_msg_filter,
        msg_filters,
        sig_filters,
    )
}

pub fn bus_descriptor(bus: Bus) -> Option<&'static BusDescriptor<Bus>> {
    yamcan::bus_descriptor_for::<rust_decode_generated::GeneratedNetwork>(bus)
}
