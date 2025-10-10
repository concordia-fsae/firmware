use std::collections::HashMap;
use std::net::IpAddr;
use std::time::{Duration, Instant};

use mdns_sd::{ResolvedService, ScopedIp, ServiceDaemon, ServiceEvent};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum ClientError {
    #[error("mdns error: {0}")]
    Mdns(#[from] mdns_sd::Error),
    #[error("mdns error: Service not found")]
    NotFound,
}

#[derive(Debug, Clone, Default)]
pub struct DiscoveryFilter {
    pub service_type: Option<String>,
    pub host_name: Option<String>,
}

#[derive(Debug, Clone)]
pub struct DiscoveredService {
    pub fullname: String,
    pub host_name: String,
    pub addresses: Vec<IpAddr>,
    pub port: u16,
    pub txt: HashMap<String, String>,
}

pub struct Client {
    pub interface: Option<String>,
    discovery_duration: Duration,
}

const DEFAULT_RX_TIMEOUT: Duration = Duration::from_millis(1000);

impl Client {
    pub fn new(interface: Option<String>, max_duration: Option<Duration>) -> Result<Self, ClientError> {
        let mut discovery_duration = DEFAULT_RX_TIMEOUT;

        if let Some(duration) = max_duration {
            discovery_duration = duration;
        }

        Ok(Self { interface, discovery_duration})
    }

    pub fn discover(
        &self,
        filter: DiscoveryFilter,
    ) -> Result<DiscoveredService, ClientError> {
        let daemon = ServiceDaemon::new()?;
        if let Some(iface) = &self.interface {
            daemon.enable_interface(iface)?;
        }

        let deadline = Instant::now() + self.discovery_duration;
        let mut out: Option<DiscoveredService> = None;

        if let Some(service_type) = filter.service_type {
            let rx = daemon.browse(&service_type)?;
            while Instant::now() < deadline {
                if let Ok(evt) = rx.recv_timeout(self.discovery_duration) {
                    if let ServiceEvent::ServiceResolved(res) = evt {
                        out = Some(as_discovered(&res));
                    }
                }
            }
        }

        if let Some(output) = out {
            Ok(output)
        } else {
            Err(ClientError::NotFound)
        }
    }
}

fn as_discovered(r: &ResolvedService) -> DiscoveredService {
    let txt_map: HashMap<String, String> = r
        .txt_properties
        .iter()
        .map(|p| (p.key().to_string(), p.val_str().to_string()))
        .collect();

    DiscoveredService {
        fullname: r.fullname.clone(),
        host_name: r.host.clone(),
        addresses: r
            .addresses
            .iter()
            .map(|s: &ScopedIp| s.to_ip_addr())
            .collect(),
        port: r.port,
        txt: txt_map,
    }
}
