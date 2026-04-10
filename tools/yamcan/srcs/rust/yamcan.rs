use std::collections::HashMap;
use std::error::Error;
use std::hash::Hash;
use std::marker::PhantomData;

#[repr(C)]
pub union CanData {
    pub u64: u64,
    pub u32: [u32; 2],
    pub u16: [u16; 4],
    pub u8: [u8; 8],
}

#[derive(Clone, Copy, Debug)]
pub struct CanFrame {
    pub can_id: u32,
    pub can_dlc: u8,
    pub data: [u8; 8],
}

#[derive(Clone, Copy, Debug)]
pub struct ReceivedCanMessage<B: NetworkBus> {
    pub bus: B,
    pub id: u32,
    pub len: u8,
    pub data: [u8; 8],
}

#[derive(Clone, Copy, Debug)]
pub struct MessageMetadata<B: NetworkBus> {
    pub bus: B,
    pub name: Option<&'static str>,
    pub id: u32,
    pub len: u8,
}

#[derive(Clone, Copy, Debug)]
pub struct UnhandledMessage<B: NetworkBus> {
    pub metadata: MessageMetadata<B>,
    pub data: [u8; 8],
}

#[derive(Clone, Debug)]
pub struct SignalMeasurement {
    pub name: String,
    pub value: f64,
    pub unit: Option<String>,
    pub label: Option<String>,
}

#[derive(Clone, Debug)]
pub struct DecodedMessage {
    pub bus_name: String,
    pub message_name: String,
    pub message_id: u32,
    pub members: Vec<SignalMeasurement>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SignalKind {
    Numeric,
    Boolean,
    Enum,
}

#[derive(Clone, Copy, Debug)]
pub struct MessageDescriptor<B: NetworkBus> {
    pub bus: B,
    pub name: &'static str,
    pub id: u32,
    pub len: u8,
}

#[derive(Clone, Copy, Debug)]
pub struct SignalDescriptor<B: NetworkBus> {
    pub bus: B,
    pub message_name: &'static str,
    pub message_id: u32,
    pub signal_name: &'static str,
    pub fqid: &'static str,
    pub unit: Option<&'static str>,
    pub kind: SignalKind,
}

#[derive(Clone, Copy)]
pub struct BusDescriptor<B: NetworkBus> {
    pub name: B,
    pub interface_type: BusInterfaceType,
    pub unpack: unsafe extern "C" fn(u32, *const CanData),
}

#[derive(Clone, Debug)]
pub struct BusBinding<B: NetworkBus> {
    pub iface: String,
    pub bus: B,
}

#[derive(Clone, Copy, Debug)]
pub struct ForwardMessage {
    pub id: u32,
    pub name: &'static str,
}

#[derive(Clone, Copy, Debug)]
pub enum ForwardPolicy {
    All,
    Bridged,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum BusInterfaceType {
    Physical,
    Virtual,
}

pub type YamcanGetFn = fn(&mut f64) -> u8;
pub type YamcanEnumNameFn = fn(i32) -> Option<&'static str>;

#[derive(Clone, Copy)]
pub struct SignalAccessor {
    pub name: &'static str,
    pub unit: Option<&'static str>,
    pub get: YamcanGetFn,
    pub enum_name: Option<YamcanEnumNameFn>,
}

pub trait NetworkBus: Copy + Eq + Hash {
    fn as_str(self) -> &'static str;
}

#[derive(Clone, Copy, Debug)]
pub struct ForwardRoute<B: NetworkBus> {
    pub source_bus: B,
    pub dest_bus: B,
    pub policy: ForwardPolicy,
    pub forwarded_messages: &'static [ForwardMessage],
}

impl SignalAccessor {
    pub fn read(&self) -> SignalMeasurement {
        let mut value = 0f64;
        (self.get)(&mut value);
        SignalMeasurement {
            name: self.name.to_string(),
            value,
            unit: self.unit.map(str::to_string),
            label: self
                .enum_name
                .and_then(|lookup| lookup(value as i32))
                .map(str::to_string),
        }
    }
}

impl<B: NetworkBus> ForwardRoute<B> {
    pub fn forwards_id(&self, id: u32) -> bool {
        match self.policy {
            ForwardPolicy::All => true,
            ForwardPolicy::Bridged => self
                .forwarded_messages
                .iter()
                .any(|message| message.id == id),
        }
    }

    pub fn forwarded_message_for_id(&self, id: u32) -> Option<&'static ForwardMessage> {
        self.forwarded_messages
            .iter()
            .find(|message| message.id == id)
    }
}

impl<B: NetworkBus> BusDescriptor<B> {
    pub fn is_virtual(&self) -> bool {
        self.interface_type == BusInterfaceType::Virtual
    }
}

pub trait DecodedCanMessage {
    type Bus: NetworkBus;

    fn metadata(&self) -> &MessageMetadata<Self::Bus>;
    fn measurements(
        &self,
        sig_filters: &[String],
        allow_empty: bool,
    ) -> Option<Vec<SignalMeasurement>>;
}

#[derive(Clone, Debug)]
pub enum MessageDecodeResult<M: DecodedCanMessage> {
    Decoded(M),
    Unhandled(UnhandledMessage<M::Bus>),
}

pub trait NetworkDecoder {
    type Bus: NetworkBus;
    type Message: DecodedCanMessage<Bus = Self::Bus>;

    fn decode_received_message(
        message: ReceivedCanMessage<Self::Bus>,
    ) -> MessageDecodeResult<Self::Message>;
    fn buses() -> &'static [BusDescriptor<Self::Bus>];
}

pub struct BusRouter<N: NetworkDecoder> {
    bindings: HashMap<String, BusBinding<N::Bus>>,
    _network: PhantomData<N>,
}

impl<N: NetworkDecoder> BusRouter<N> {
    pub fn from_specs(
        specs: &[String],
        iface_bus_map: &[(&str, N::Bus)],
    ) -> Result<(Vec<String>, Self), Box<dyn Error>>
    where
        N::Bus: 'static,
    {
        let (ifaces, bindings) = parse_bus_specs::<N>(specs, iface_bus_map)?;
        Ok((
            ifaces,
            Self {
                bindings,
                _network: PhantomData,
            },
        ))
    }

    pub fn binding_for_iface(&self, iface: &str) -> Option<&BusBinding<N::Bus>> {
        self.bindings.get(iface)
    }
}

pub fn bus_descriptor_for<N: NetworkDecoder>(
    bus: N::Bus,
) -> Option<&'static BusDescriptor<N::Bus>> {
    N::buses().iter().find(|desc| desc.name == bus)
}

pub fn parse_bus_specs<N: NetworkDecoder>(
    specs: &[String],
    iface_bus_map: &[(&str, N::Bus)],
) -> Result<(Vec<String>, HashMap<String, BusBinding<N::Bus>>), Box<dyn Error>>
where
    N::Bus: 'static,
{
    let mut buses: Vec<String> = Vec::new();
    let mut seen: HashMap<String, usize> = HashMap::new();
    let mut bindings: HashMap<String, BusBinding<N::Bus>> = HashMap::new();

    for spec in specs {
        if spec.contains('=') {
            return Err("DBC decoding is not supported; pass only IFACE names".into());
        }

        let iface = spec.trim();
        if iface.is_empty() {
            return Err("Interface name must not be empty".into());
        }

        let binding = resolve_binding::<N>(iface, iface_bus_map)?;

        if !seen.contains_key(iface) {
            seen.insert(iface.to_string(), buses.len());
            buses.push(iface.to_string());
        }

        bindings.insert(iface.to_string(), binding);
    }

    if buses.is_empty() {
        return Err("Provide at least one IFACE".into());
    }

    Ok((buses, bindings))
}

pub fn binding_for_iface<N: NetworkDecoder>(
    iface: &str,
    iface_bus_map: &[(&str, N::Bus)],
) -> Result<BusBinding<N::Bus>, Box<dyn Error>>
where
    N::Bus: 'static,
{
    if iface.contains('=') {
        return Err("DBC decoding is not supported; pass only IFACE names".into());
    }
    let iface = iface.trim();
    if iface.is_empty() {
        return Err("Interface name must not be empty".into());
    }
    resolve_binding::<N>(iface, iface_bus_map)
}

fn resolve_binding<N: NetworkDecoder>(
    iface: &str,
    iface_bus_map: &[(&str, N::Bus)],
) -> Result<BusBinding<N::Bus>, Box<dyn Error>>
where
    N::Bus: 'static,
{
    let bus_name = iface_bus_map
        .iter()
        .find(|(mapped_iface, _)| *mapped_iface == iface)
        .map(|(_, bus)| *bus)
        .ok_or_else(|| format!("No yamcan mapping for interface '{iface}'"))?;

    if !N::buses().iter().any(|desc| desc.name == bus_name) {
        return Err(format!("No yamcan bus descriptor found for interface '{iface}'").into());
    }

    Ok(BusBinding {
        iface: iface.to_string(),
        bus: bus_name,
    })
}

pub fn maybe_decode<N: NetworkDecoder>(
    binding_opt: Option<&BusBinding<N::Bus>>,
    frame: &CanFrame,
    id_masked: u32,
    allow_empty_signals: bool,
    ignore_msg_filter: bool,
    msg_filters: &[String],
    sig_filters: &[String],
) -> Option<DecodedMessage> {
    let binding = binding_opt?;
    let message = ReceivedCanMessage {
        bus: binding.bus,
        id: id_masked,
        len: frame.can_dlc,
        data: frame.data,
    };

    match N::decode_received_message(message) {
        MessageDecodeResult::Decoded(message) => {
            let metadata = message.metadata();
            let message_name = metadata.name?;

            if !ignore_msg_filter && !msg_filters.is_empty() {
                let lname = message_name.to_lowercase();
                if !msg_filters.iter().any(|pattern| lname.contains(pattern)) {
                    return None;
                }
            }

            message
                .measurements(sig_filters, allow_empty_signals)
                .map(|members| DecodedMessage {
                    bus_name: metadata.bus.as_str().to_string(),
                    message_name: message_name.to_string(),
                    message_id: metadata.id,
                    members,
                })
        }
        MessageDecodeResult::Unhandled(_) => None,
    }
}
