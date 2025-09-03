/// App Configuration handler
///
/// Handles reading configuration files and providing the options that been configured
use std::collections::HashMap;
use std::fs::File;
use std::io::BufReader;

use anyhow::{anyhow, Result};
use log::{debug, error};
use serde::Deserialize;

#[derive(Debug, Deserialize)]
pub struct CfgYaml {
    pub nodes: HashMap<String, Node>,
}

#[derive(Debug, Deserialize)]
pub struct Node {
    pub request_id: u32,
    pub response_id: u32,
}

#[derive(Debug)]
pub struct Config {
    pub nodes: HashMap<String, Node>,
}

impl Config {
    pub fn new(filename: &str) -> Result<Config> {
        debug!("Attempting to load `{}`", filename);
        if let Ok(file) = File::open(filename) {
            let reader = BufReader::new(file);
            let cfg_yaml: CfgYaml = serde_yaml::from_reader(reader)?;
            debug!("Successfully loaded `{}`\n{:#?}", filename, cfg_yaml);

            return Ok(Self {
                nodes: cfg_yaml.nodes,
            });
        } else {
            error!("Couldn't open `{}`", filename);
            Err(anyhow!("Couldn't open `{}`", filename))
        }
    }
}
