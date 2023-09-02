# Setting Up and Using the Development Environment

## Workflows

### 1. Installation and Set-Up 

1. Clone repository
    - Clone with ssh link. This enables ssh when pushing and pulling
2. Pull in submodules
    - `git submodule update --init --recursive`
3. Install and start docker (OS-specific)
    - **Windows Users**: Install bash emulator: cygwin, mingw64, WSL (test optimal tool)
4. Install docker compose (OS-specific)
5. Execute docker script from root directory
    - `./buildroot.sh`
        * Change permissions if necessary: `chmod +x buildroot.sh`

### 2. Container Usage

#### Building and Running with the SCons Build System

- _All components, and their designator, are listed in `./site_scons/components.yaml`_
- _It is always good when you first open the env to execute `st-info --probe` to see if the ST-LINK is recognized_

1. Building the program
    - Execute the SCons program from inside the env
        - `scons --target=$COMPONENT_DESIGNATOR build`
        - ex: To build the stw, execute `scons --target=stw build`. Build is the default action for a target and therefore does not need to be specified
2. Uploading the program (Optional)
    - The SCons build environment has an optional upload command that flashes the device and holds
    - To flash and verify, execute `scons --target=$COMPONENT_DESIGNATOR upload` from the env
3. Debugging the program
    - _More information and better debugging workflows can be found by researching OpenOCD and GDB commands_
    - Debugging in the env is done through GDB and OpenOCD
    - To start the OpenOCD server and connect to the remote host in GDB, execute:
        - `scons --target=$COMPONENT_DESIGNATOR openocd-gdb` from the env
        - You can chain multiple commands together. For example: 'scons --target=stw upload openocd-gdb' will build, upload, and start the debugger
    - The standard GDB commands apply. To reset or halt the MCU through the SWD connection, execute:
        - `monitor reset`, `monitor reset halt`

## Sections

1. SCons Tools
    - Located in `./site_scons/site_tools/`
    - Custom python scripts added to the SCons build environment
2. Embedded Systems
    1. Toolchains
        - Downloaded from ARM Developper
        - Located in `./embedded/toolchains`
    2. USB Loader/Debugger
        - Located in `./embedded/openocd`
        - Interfaces and stores configurations of different chips/boards
    3. Platforms
        - Supported hardware platforms
        - Located in `./embedded/platforms/`
4. SCons related scripts
    - `./SConstruct`
    - `./components/$DESIGNATOR/Sconscript`

## Notes

- The script will set up the env for compiling - no need to run twice
- Due to the mounting feature of the docker container, anytime the container is open the changes applied to the files will automatically and immediately be accessible to the SCons tool
    - This also means that, once built, the build dir will be on your local machine as well. Run scons --clean in container if you want to re-build from scratch
- If doing loading/debugging of physical hardware, the ST-LINK or other interface must be plugged in by USB before starting the container. Docker containers do not support dynamic loading of USB peripherals

