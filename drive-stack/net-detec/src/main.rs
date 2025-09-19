use std::collections::HashMap;
use std::time::Duration;

use anyhow;
use clap::{ArgAction, Parser, Subcommand};
use env_logger::Env;
use if_addrs;
use net_detec::{Client, DiscoveredService, DiscoveryFilter, Server};

#[derive(Parser, Debug)]
#[command(name = "net-detec", about = "mDNS-SD client/server utility and library")]
struct Cli {
    /// Network interface to use (e.g., eth0, en0)
    #[arg(short, long)]
    interface: Option<String>,

    /// Host name to advertise or query (must end with .local.)
    #[arg(short = 'H', long, default_value = "net-detec.local.")]
    host_name: String,

    /// Service type like _net-detec._tcp.local.
    #[arg(short = 's', long, default_value = "_net-detec._tcp.local.")]
    service_name: String,

    /// Optional IPv4 to advertise. If omitted, the server will detect the IPv4 of the chosen interface.
    #[arg(long)]
    ip: Option<String>,

    /// Run mode
    #[command(subcommand)]
    cmd: Mode,
}

#[derive(Subcommand, Debug)]
enum Mode {
    /// Run as a server/responder and advertise a service on the chosen interface
    Server {
        /// Instance name to show (defaults to host_name without domain)
        #[arg(short, long)]
        instance: Option<String>,
        /// TCP/UDP port to advertise
        #[arg(short, long, default_value_t = 8080)]
        port: u16,
        /// TXT records as key=value. Repeat for multiple.
        #[arg(short = 't', long = "txt", action = ArgAction::Append)]
        txt: Vec<String>,
    },

    /// Run as a client/querier and discover services
    Client {
        /// Timeout in seconds
        #[arg(short, long, default_value_t = 5)]
        timeout: u64,
    },
}

fn parse_txt(kvs: &[String]) -> HashMap<String, String> {
    kvs.iter()
        .filter_map(|kv| {
            let mut it = kv.splitn(2, '=');
            match (it.next(), it.next()) {
                (Some(k), Some(v)) => Some((k.to_string(), v.to_string())),
                _ => None,
            }
        })
        .collect()
}

fn main() -> anyhow::Result<()> {
    env_logger::Builder::from_env(Env::default().default_filter_or("info")).init();
    let cli = Cli::parse();

    match cli.cmd {
        Mode::Server { instance, port, txt } => {
            let host = cli.host_name;
            let service = cli.service_name;
            let instance_name =
                instance.unwrap_or_else(|| host.trim_end_matches(".local.").to_string());
            let interface = cli.interface.unwrap_or_else(default_iface_name);
            let txt_map = parse_txt(&txt);

            let server = Server {
                interface: interface.clone(),
                service_type: service.clone(),
                instance_name,
                host_name: host.clone(),
                ip: cli.ip.clone(), // <-- user-specified or None
                port,
                txt: txt_map,
            };

            println!("Starting service advertisement for {} => {} {:?} port:{} txt:{:?} on interface '{}'",
                     service, host, cli.ip, port, txt, interface);
            let dns = server.start();

            match dns {
                Ok(dns_server) => {
                    while dns_server.is_alive() {
                        std::thread::sleep(Duration::from_secs(60))
                    }
                    println!("mDNS server exited!: {:?}", dns_server.try_exit());
                }
                Err(e) => {
                    println!("mDNS failed with: {:?}", e);
                }
            }
        }
        Mode::Client { timeout } => {
            let client = Client::new(cli.interface.clone(), Some(Duration::from_secs(timeout)))?;
            let mut result: Option<DiscoveredService> = None;

            let filter = DiscoveryFilter {
                service_type: Some(cli.service_name.clone()),
                host_name: Some(cli.host_name.clone()),
            };
            let servers = client.discover(filter);
            match servers {
                Ok(results) => result = Some(results),
                Err(e) => println!("Error identifying server: {:?}", e),
            }

            match result {
                None => println!("No services found."),
                Some(result) => {
                    println!(
                        "{} => {} {:?} port:{} txt:{:?}",
                        result.fullname, result.host_name, result.addresses, result.port, result.txt
                    );
                }
            }
        }
    }
    Ok(())
}

fn default_iface_name() -> String {
    if let Ok(addrs) = if_addrs::get_if_addrs() {
        for a in addrs {
            if !a.is_loopback() {
                return a.name;
            }
        }
    }
    "lo".to_string()
}
