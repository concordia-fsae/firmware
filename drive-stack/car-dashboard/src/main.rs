use anyhow::Result;
use clap::Parser;

use dashboard::{Opts, run};

#[tokio::main]
async fn main() -> Result<()> {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();
    run(Opts::parse()).await
}
