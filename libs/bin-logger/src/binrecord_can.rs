use serde::{Serialize, Deserialize};
use std::collections::HashMap;

/// Value enum for decoded signals or metadata.
#[derive(Clone, Serialize, Deserialize, Debug)]
pub enum Val {
    F64(f64),
    Str(String),
}

/// Record type written by bin-logger and used by producers/consumers.
#[derive(Clone, Serialize, Deserialize, Debug)]
pub struct BinRecordCan {
    /// (sec, nsec) if present
    pub ts: Option<(u64, u32)>,
    /// Interface/bus label (e.g., "can0")
    pub bus: String,
    /// Masked 11/29-bit ID (no flags)
    pub id: u32,
    pub dlc: u8,
    pub extended: bool,
    pub rtr: bool,
    pub err: bool,
    /// Signals + raw "data" hex string under the "data" key
    pub fields: HashMap<String, Val>,
}
