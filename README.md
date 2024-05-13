conUDS
============================
A UDS client written for use by the Concordia FSAE team.

# About
This UDS client (written in Rust) uses the attached CAN dongle (only socketcan supported for now)
to interact with a given ECU using the UDS protocl on the attached CAN bus.

## Supported Functionality
So far, this UDS client supports:
  - ECU Resets
  - Persistent Tester Present at 10ms
  - App Downloading

# Building the Application
Releases are not currently set up yet, though eventually you will be able to simply download
a zip file containing the binary.

For now, you must set up Rust on your computer and use Cargo to build this application. Instructions for
setting up Rust are ubiquitous, so details are not provided here. See [here](https://www.rust-lang.org/tools/install)
for instructions.

Once Rust is installed, simply clone this repo, cd into the `conUDS` directory contained within it, and build the
application with Cargo:
`cargo build --release`

The resulting binary lives in the `target` directory which is created when building.

# Usage

## Running

Once built, you can run the application in a few ways.

1. using `cargo`. You can run the application from the `conUDS` directory using:  
   `cargo run -- <args>`  
   note: the seemingly useless `--` in the above command is required. Only arguments that come after it will be passed
   to the actual application
1. by running the application directly. From the `conUDS` directory, you can call:  
   `./target/release/conuds <args>`  
   to run the application. In this case, you can start passing args in right away, i.e.  
   `./target/release/conuds --help`  
   You are free to move this binary wherever you like, but **you must move the `assets` directory with it**

The `assets/nodes.yml` file contains details about the UDS nodes that are available for the application to communicate
with. You may edit this file as you please and then run the application, which reads the file at runtime. Feel free to
update this file as new nodes are added and to make a PR to merge your changes.

## Functionality

Read the information provided by the `--help` flag for the most up-to-date information about the features
that this application supports.

