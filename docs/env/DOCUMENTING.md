# Documenting the Code

### Doxygen

- Doxygen is a tool that generates documentation for code. It is used in this repository for documenting various sections of the codebase
    - [Doxygen Manual](https://doxygen.nl/manual/docblocks.html)
    - [Doxygen Configuration Examples](https://doxygen.nl/manual/config.html#config_examples)
- Each Doxygen configuration file is located in the root folder of each component
    - ex: Steering Wheel Doxygen configuration file `./components/steering_wheel/doxygen_stw.conf`
- To document one of the components, execute:
    - `scons --target=$COMPONENT_DESIGNATOR --doc` from inside the container
- To document the SCons Python scripts located in `./site_scons/site_tools/`, execute:
    - Configuration file is located at `./site_scons/site_tools/doxygen.conf`
    - Execute `scons --doc` from inside the container to build the documentation
    - The documentation, once created, is located in `./docs/env/tools/`
