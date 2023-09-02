# Firmware Repo

### Documentation

_All documentation is accessible from `./docs/`_

_To access the Doxygen built documentation, load the `html/index.html` file located in any of the <br> `./docs/[ env | components ]/docs-$DESIGNATOR/` folders_
- Documentation relating to the build system is found in `./docs/env/`
    - Setting Up and Using the Development Environment `./docs/env/ENV.md`
    - Documenting the code `./docs/env/DOCUMENTING.md`
    - Doxygen documentation relating to the SCons Python tools `./docs/env/docs-tools/`
- Documentation relating to each component is found in `./docs/components/`
    - Steering Wheel Details `./docs/components/docs-tools/STW.md`
    - Component Folder and File Hierarchy `./docs/components/COMPONENTS.md`
    - Doxygen documentation relating to individual components `./docs/components/docs-$DESIGNATOR/`
- Programming, Documentation, and all other guidelines `./docs/GUIDELINES.md`

### Components

| Name | Designator | Path | Comments |
| --- | --- | --- | --- |
| Steering Wheel | stw | components/steering_wheel/ | Steering Wheel source code and headers |