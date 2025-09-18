use std::collections::HashMap;
use std::io::Write;
use std::path::PathBuf;
use std::process::{Command, Stdio};
use std::time::Duration;

use anyhow::{anyhow, Context, Result};
use clap::Parser;
use bin_logger::{iterate_dir, BinRecordCan};
use net_detec::{Client, DiscoveredService, DiscoveryFilter};

const BASEPUTER_TIMEOUT: Duration = Duration::from_secs(2);

#[derive(Parser, Debug)]
#[command(name = "log-offloader", about = "Iterate bin-logger files and offload records")]
struct Args {
    /// Directory containing can-bridge-*.bin files
    #[arg(long, value_name = "DIR")]
    dir: PathBuf,

    /// Output mode: stdout | stats | server
    #[arg(long, default_value = "stdout")]
    mode: String,

    /// External command to exec; we stream JSON lines to its stdin
    /// (only used with --mode=stdout).
    #[arg(long, value_name = "CMD")]
    exec: Option<String>,

    /// mDNS interface
    #[arg(short = 'i', long)]
    interface: Option<String>,

    /// Log destination service name for mDNS
    #[arg(short = 'b', long, default_value = "_baseputer._tcp.local.")]
    baseputer_service: String,
}

fn main() -> Result<()> {
    let args = Args::parse();

    match args.mode.as_str() {
        "stdout" => run_stdout_mode(&args),
        "stats"  => run_stats_mode(&args),
        "server" => run_server_mode(&args),
        other    => Err(anyhow!("unknown --mode: {}", other)),
    }
}

fn run_stdout_mode(args: &Args) -> Result<()> {
    if let Some(cmdline) = &args.exec {
        // Spawn child command; write JSONL to its stdin
        let (prog, rest) = split_cmd(cmdline);
        let mut child = Command::new(prog)
            .args(rest)
            .stdin(Stdio::piped())
            .stdout(Stdio::inherit())
            .stderr(Stdio::inherit())
            .spawn()
            .with_context(|| format!("failed to spawn {}", cmdline))?;

        let mut stdin = child.stdin.take().context("no stdin on child")?;
        iterate_dir(&args.dir, |path, rec| {
            let line = serde_json::to_string(rec)? + "\n";
            stdin
                .write_all(line.as_bytes())
                .with_context(|| format!("write to child for file {}", path.display()))?;
            Ok(())
        })?;
        drop(stdin);
        let status = child.wait()?;
        if !status.success() {
            return Err(anyhow!("child exited with {}", status));
        }
    } else {
        // Print to our own stdout
        iterate_dir(&args.dir, |_path, rec| {
            println!("{}", serde_json::to_string(rec)?);
            Ok(())
        })?;
    }
    Ok(())
}

fn run_stats_mode(args: &Args) -> Result<()> {
    let mut per_id: HashMap<u32, usize> = HashMap::new();
    let mut total = 0usize;

    iterate_dir(&args.dir, |_path, rec| {
        *per_id.entry(rec.id).or_insert(0) += 1;
        total += 1;
        Ok(())
    })?;

    eprintln!("total records: {}", total);
    let mut pairs: Vec<_> = per_id.into_iter().collect();
    pairs.sort_by_key(|&(id, _)| id);
    for (id, n) in pairs {
        println!("ID=0x{id:X} ({id}) count={n}");
    }
    Ok(())
}

fn run_server_mode(args: &Args) -> Result<()> {
    let mut server_up = false;
    let client = Client::new(args.interface.clone(), Some(BASEPUTER_TIMEOUT)).expect("mDNS client failed");
    println!("Starting mDNS listener...");

    let filter = DiscoveryFilter {
        service_type: Some(args.baseputer_service.clone()),
        host_name: Some("".to_string()),
    };

    loop {
        let servers = client.discover(filter.clone());

        match servers {
            Ok(results) => {
                if !server_up {
                    println!("Identified server: {:?}", result);
                    server_up = true;
                }

                if let Some(result) = result {
                    iterate_dir(&args.dir, |path, _rec| {
                        println!(
                            "Uploading {} to {} => {} {:?} port:{}",
                            path.display(), result.fullname, result.host_name, result.addresses, result.port
                        );
                        Ok(())
                    });
                }
            }
            Err(e) => {
                if server_up {
                    println!("Error identifying server: {:?}", e);
                    server_up = false;
                }
            }
        }
    }
}

fn split_cmd(s: &str) -> (&str, Vec<&str>) {
    // Very simple splitter; for complex shells pass a wrapper script.
    let mut iter = s.split_whitespace();
    let prog = iter.next().unwrap_or(s);
    let rest = iter.collect::<Vec<_>>();
    (prog, rest)
}
