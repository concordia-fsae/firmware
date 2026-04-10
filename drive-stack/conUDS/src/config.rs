/// App Configuration handler
///
/// Handles reading configuration files and providing the options that been configured
use std::collections::HashMap;
use std::fs::File;
use std::io::BufReader;

use anyhow::{Result, anyhow};
use log::{debug, error, warn};
use serde::Deserialize;

#[derive(Debug, Deserialize)]
pub struct CfgYaml {
    pub nodes: HashMap<String, Node>,
}

#[derive(Debug, Deserialize)]
pub struct RoutineCfgYaml {
    pub nodes: HashMap<String, RoutineNode>,
}

#[derive(Debug, Deserialize)]
pub struct RoutineNode {
    pub routines: HashMap<String, Routine>,
}

#[derive(Debug, Deserialize)]
pub struct Node {
    pub request_id: u32,
    pub response_id: u32,
    #[serde(default)]
    pub routines: HashMap<String, Routine>,
}

#[derive(Debug, Deserialize)]
pub struct Routine {
    pub id: u16,
}

#[derive(Debug)]
pub struct Config {
    pub nodes: HashMap<String, Node>,
}

impl Config {
    pub fn new(node_manifest: &str, routine_manifest: &str) -> Result<Config> {
        debug!("Attempting to load `{}`", node_manifest);
        if let Ok(file) = File::open(node_manifest) {
            let reader = BufReader::new(file);
            let cfg_yaml: CfgYaml = serde_yaml::from_reader(reader)?;
            debug!("Successfully loaded `{}`\n{:#?}", node_manifest, cfg_yaml);

            let mut nodes = cfg_yaml.nodes;
            merge_routines(&mut nodes, routine_manifest)?;

            return Ok(Self { nodes });
        } else {
            error!("Couldn't open `{}`", node_manifest);
            Err(anyhow!("Couldn't open `{}`", node_manifest))
        }
    }
}

fn merge_routines(nodes: &mut HashMap<String, Node>, routine_manifest: &str) -> Result<()> {
    debug!("Attempting to load `{}`", routine_manifest);
    let file = match File::open(routine_manifest) {
        Ok(file) => file,
        Err(_) => {
            warn!(
                "Routine manifest `{}` not found; no routines loaded",
                routine_manifest
            );
            return Ok(());
        }
    };

    let reader = BufReader::new(file);
    let cfg_yaml: RoutineCfgYaml = serde_yaml::from_reader(reader)?;
    debug!(
        "Successfully loaded `{}`\n{:#?}",
        routine_manifest, cfg_yaml
    );

    for (node_name, routine_node) in cfg_yaml.nodes {
        if let Some(node) = nodes.get_mut(&node_name) {
            node.routines = routine_node.routines;
        } else {
            warn!("Routine manifest references unknown node '{}'", node_name);
        }
    }

    Ok(())
}
