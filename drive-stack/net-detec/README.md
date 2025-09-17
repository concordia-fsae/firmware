# net-detec

A tiny mDNS-SD **client/server** utility with a reusable library. It can **advertise** a service on a specific interface or **discover** services/hosts by service type or hostname.

## Advertise (server)
```bash
# Advertise _net-detec._tcp on eth0, port 8080 with some TXT
net-detec --interface eth0 --host-name my-compute.local. --service-name _net-detec._tcp.local. \
  server --instance compute-node --port 8080 --txt role=node --txt env=prod
```

## Discover (client)
```bash
# Find by service type
net-detec --interface eth0 --service-name _net-detec._tcp.local. client --timeout 5

# Or resolve a hostname directly
net-detec --interface eth0 --host-name my-compute.local. client --timeout 3
```

## Library usage
```rust
use net_detec::{Client, DiscoverFilter, Server};
use std::time::Duration;

// Server
let handle = Server {
    interface: "eth0".into(),
    service_type: "_net-detec._tcp.local.".into(),
    instance_name: "compute-node".into(),
    host_name: "my-compute.local.".into(),
    port: 8080,
    txt: Default::default(),
}.start()?;

// Client
let cli = Client::new(Some("eth0".into()))?;
let results = cli.discover(DiscoverFilter{ service_type: Some("_net-detec._tcp.local.".into()), host_name: None }, Duration::from_secs(3))?;
```
