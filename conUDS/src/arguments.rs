/// Argument processor
///
/// Handles processing command-line arguments
use std::path::PathBuf;

use argh::FromArgs;

use crate::SupportedResetTypes;

/// conUDS - a UDS client deigned for use by Concordia FSAE to interact with ECUs on their vehicles
/// using the UDS protocol
#[derive(Debug, FromArgs)]
pub struct Arguments {
    /// the node to interact with
    #[argh(option, short = 'n')]
    pub node: String,

    /// the CAN device to use. `can0` is used if this option is not provided
    #[argh(option, short = 't', default = "String::from(\"can0\")")]
    pub device: String,

    #[argh(subcommand)]
    pub subcmd: ArgSubCommands,
}

#[derive(Debug, FromArgs)]
#[argh(subcommand)]
pub enum ArgSubCommands {
    Download(SubArgDownload),
    Reset(SubArgReset),
    BootloaderDownload(SubArgBootloaderDownload),
    ReadDID(SubArgReadDID),
}

/// Download an application to an ECU
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "download")]
pub struct SubArgDownload {
    /// path to the binary file to flash
    #[argh(positional)]
    pub binary: PathBuf,
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
