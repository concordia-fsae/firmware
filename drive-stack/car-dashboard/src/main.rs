#![recursion_limit = "512"]

use anyhow::Result;
use clap::Parser;

use dashboard::{run, Opts};

#[tokio::main]
async fn main() -> Result<()> {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();
    run(Opts::parse()).await
}
