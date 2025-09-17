use std::collections::HashMap;
use std::net::{IpAddr, Ipv4Addr};
use std::panic::{self, AssertUnwindSafe};
use std::sync::mpsc::{self, Receiver};
use std::time::Duration;
use std::thread::{self, JoinHandle};

use if_addrs::get_if_addrs;
use mdns_sd::{ServiceDaemon, ServiceInfo};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum ServerError {
    #[error("interface '{0}' not found or has no IPv4 address")]
    InterfaceNotFound(String),
    #[error("mdns error: {0}")]
    Mdns(#[from] mdns_sd::Error),
    #[error("mdns thread error: {0}")]
    ThreadSpawn(String),
}

/// Server configuration + launcher
pub struct Server {
    /// Interface name (e.g., "eth0")
    pub interface: String,
    /// Service type, e.g. "_net-detec._tcp.local."
    pub service_type: String,
    /// Instance name shown to browsers, e.g. "orin-node"
    pub instance_name: String,
    /// Hostname to advertise (must end with ".local.")
    pub host_name: String,
    /// Optional IPv4 to advertise; if None we'll detect from interface
    pub ip: Option<String>,
    /// Port to advertise
    pub port: u16,
    /// TXT records
    pub txt: HashMap<String, String>,
}

#[derive(Debug)]
pub enum ServerExit {
    Ok,
    Err(ServerError),
    Panicked(String),
}

pub struct ServerHandle {
    join: JoinHandle<()>,
    exit_rx: Receiver<ServerExit>,
}

impl ServerHandle {
    /// Non-blocking check: returns Some(exit) if the thread has exited; None otherwise.
    pub fn try_exit(&self) -> Option<ServerExit> {
        self.exit_rx.try_recv().ok()
    }

    /// If you ever want to block and join.
    pub fn join(self) -> thread::Result<()> {
        self.join.join().map(|_| ())
    }

    /// Quick liveness check.
    pub fn is_alive(&self) -> bool {
        !self.join.is_finished()
    }
}

impl Server {
/// Spawns the announcer thread and returns immediately.
    pub fn start(self) -> Result<ServerHandle, ServerError> {
        let (exit_tx, exit_rx) = mpsc::channel::<ServerExit>();

        // Spawn named thread (helps in debuggers/logs)
        let join = thread::Builder::new()
            .name("mdns-announcer".into())
            .spawn(move || {
                // Catch panics so the caller gets a signal instead of silent death.
                let result = panic::catch_unwind(AssertUnwindSafe(|| self.run()));

                match result {
                    Ok(Ok(())) => {
                        let _ = exit_tx.send(ServerExit::Ok);
                    }
                    Ok(Err(e)) => {
                        let _ = exit_tx.send(ServerExit::Err(e));
                    }
                    Err(payload) => {
                        // best-effort stringify
                        let msg = payload
                            .downcast_ref::<&'static str>()
                            .map(|s| (*s).to_string())
                            .or_else(|| payload.downcast_ref::<String>().cloned())
                            .unwrap_or_else(|| "thread panicked".to_string());
                        let _ = exit_tx.send(ServerExit::Panicked(msg));
                    }
                }
            })
            .map_err(|e| ServerError::ThreadSpawn(e.to_string()))?;

        Ok(ServerHandle { join, exit_rx })
    }

    /// Your original logic extracted into a private function.
    fn run(self) -> Result<(), ServerError> {
        // If IP wasn't provided, detect it from the interface.
        let ip_str = if let Some(ip) = self.ip {
            ip
        } else {
            let ipv4 = ipv4_for_iface(&self.interface)
                .ok_or_else(|| ServerError::InterfaceNotFound(self.interface.clone()))?;
            ipv4.to_string()
        };

        let daemon = ServiceDaemon::new()?;
        // Restrict announcements to this interface
        daemon.enable_interface(&self.interface)?;

        // txt must be (&str, &str). Build owned pairs to keep data alive for the loop.
        let txt_owned: Vec<(String, String)> =
            self.txt.into_iter().map(|(k, v)| (k, v)).collect();
        let txt_pairs: Vec<(&str, &str)> =
            txt_owned.iter().map(|(k, v)| (k.as_str(), v.as_str())).collect();

        // mdns-sd constructor variant: (service, instance, host_name, ip, port, txt)
        let info = ServiceInfo::new(
            &self.service_type,
            &self.instance_name,
            &self.host_name,
            &ip_str,
            self.port,
            &*txt_pairs,
        )?;

        daemon.register(info)?;

        // Keep thread alive to maintain the announcement
        loop {
            std::thread::sleep(Duration::from_secs(60));
        }
    }
}

fn ipv4_for_iface(iface: &str) -> Option<Ipv4Addr> {
    let addrs = get_if_addrs().ok()?;
    addrs
        .into_iter()
        .find(|a| a.name == iface && matches!(a.ip(), IpAddr::V4(_)))
        .and_then(|a| match a.ip() {
            IpAddr::V4(v4) => Some(v4),
            _ => None,
        })
}
