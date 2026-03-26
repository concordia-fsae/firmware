use can_bridge::Bus;
use can_bridge::app;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    app::run(Bus::Body)
}
