/// Argument processor
///
/// Handles processing command-line arguments
use std::path::PathBuf;

use argh::FromArgs;

use crate::SupportedResetTypes;

/// canUDS - a UDS client deigned for use by Concordia FSAE to interact with ECUs on their vehicles
/// using the UDS protocol
#[derive(Debug, FromArgs)]
pub struct Arguments {
    /// the node to interact with
    #[argh(option, short = 'n')]
    pub node: Option<String>,

    /// the CAN device to use. `can0` is used if this option is not provided
    #[argh(option, short = 't', default = "String::from(\"can0\")")]
    pub device: String,

    /// the manifest of UDS can nodes on the bus
    #[argh(option, short = 'm', default = "String::from(\"drive-stack/conUDS/nodes.yml\")")]
    pub node_manifest: String,

    #[argh(subcommand)]
    pub subcmd: ArgSubCommands,
}

#[derive(Debug, FromArgs)]
#[argh(subcommand)]
pub enum ArgSubCommands {
    Download(SubArgDownload),
    BootloaderDownload(SubArgBootloaderDownload),
    Batch(SubArgBatch),
    Reset(SubArgReset),
    NVMHardReset(SubArgNVMHardReset),
    ReadDID(SubArgReadDID),
}

/// Download an application to an ECU
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "download")]
pub struct SubArgDownload {
    /// dont skip downloading if there is a CRC mismatch
    #[argh(switch, short = 's')]
    pub no_skip: bool,
    /// path to the binary file to flash
    #[argh(positional)]
    pub binary: PathBuf,
}

/// Download a set of applications to their ECUs
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "batch")]
pub struct SubArgBatch {
    /// repeatable node-to-binary pairs in the form `-u node:/path/to/bin`
    /// example: -u mcu:build/app_mcu.bin -u imu:build/app_imu.bin
    #[argh(option, short = 'u')]
    pub targets: Vec<String>,
}

/// Download a bootloader to an ECU
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "bootloader-download")]
pub struct SubArgBootloaderDownload {
    /// path to the binary file to flash
    #[argh(positional)]
    pub binary: PathBuf,
}

/// Reset an ECU
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "reset")]
pub struct SubArgReset {
    /// reset type. `soft` or `hard`
    #[argh(option, short = 't', default = "SupportedResetTypes::Hard")]
    pub reset_type: SupportedResetTypes,
}

/// Read a DID from an ECU
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "readDID")]
pub struct SubArgReadDID {
    #[argh(positional)]
    pub id: String,
}

/// Hard reset entire NVM including internal values
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "nvmHardReset")]
pub struct SubArgNVMHardReset {
}
