use can_bridge::app;
use can_bridge::Bus;

const IFACE_BUS_MAP: &[(&str, Bus)] = &[("can1", Bus::Body)];

fn main() -> Result<(), Box<dyn std::error::Error>> {
    app::run(IFACE_BUS_MAP)
}
