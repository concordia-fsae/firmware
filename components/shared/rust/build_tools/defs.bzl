"""Shared, declared Cargo builds and Yamcan/bootloader firmware target wiring."""
load("@prelude//:rules.bzl", "alias", "cxx_genrule", "genrule")
load("//tools/yamcan/defs.bzl", "generate_embedded_rust")
load("//tools/hextools/defs.bzl", "inject_crc")
load("//components/vehicle_platform:defs.bzl", "add_platform_deploy_targets")
load("//components/vehicle_platform:platforms.bzl", "platform_output_name")
load("//embedded/openocd/defs.bzl", "openocd_run")

CargoSources = provider(fields = {"srcs": provider_field(dict), "path": provider_field(str)})

def _sources_impl(ctx):
    return [DefaultInfo(default_outputs = ctx.attrs.srcs.values()), CargoSources(srcs = ctx.attrs.srcs, path = ctx.attrs.path)]

cargo_sources = rule(
    impl = _sources_impl,
    attrs = {"srcs": attrs.dict(attrs.string(), attrs.source()), "path": attrs.string()},
)

def _cargo_impl(ctx):
    sources = ctx.actions.symlinked_dir("package", ctx.attrs.srcs)
    output = ctx.actions.declare_output("out", dir = True)
    command = cmd_args("python3", ctx.attrs._runner, "--source", sources,
                       "--package-path", ctx.attrs.package_path, "--output", output.as_output(),
                       "--binary", ctx.attrs.binary, "--rust-toolchain", ctx.attrs.rust_toolchain,
                       "--target", ctx.attrs.target, "--feature", ctx.attrs.feature)
    for index, dependency in enumerate(ctx.attrs.dependencies):
        inputs = dependency[CargoSources]
        folder = ctx.actions.symlinked_dir("dependency-{}".format(index), inputs.srcs)
        command.add("--dependency", inputs.path, folder)
    if ctx.attrs.protocol:
        command.add("--protocol", ctx.attrs.protocol[DefaultInfo].default_outputs[0])
    for package in ctx.attrs.test_packages:
        command.add("--test-package", package)
    if ctx.attrs.test:
        command.add("--test")
    ctx.actions.run(command, category = "firmware_cargo_test" if ctx.attrs.test else "firmware_cargo_build", local_only = True)
    if ctx.attrs.test:
        return [DefaultInfo(default_output = output)]
    elf = output.project(ctx.attrs.binary + ".elf")
    return [DefaultInfo(default_output = elf, sub_targets = {
        "elf": [DefaultInfo(default_output = elf)],
        "map": [DefaultInfo(default_output = output.project(ctx.attrs.binary + ".map"))],
    })]

firmware_cargo = rule(
    impl = _cargo_impl,
    attrs = {
        "srcs": attrs.dict(attrs.string(), attrs.source()),
        "package_path": attrs.string(),
        "binary": attrs.string(),
        "protocol": attrs.option(attrs.dep(), default = None),
        "dependencies": attrs.list(attrs.dep(providers = [CargoSources]), default = []),
        "rust_toolchain": attrs.string(default = "1.86.0"),
        "target": attrs.string(default = "thumbv7m-none-eabi"),
        "feature": attrs.string(default = "firmware"),
        "test_packages": attrs.list(attrs.string(), default = []),
        "test": attrs.bool(default = False),
        "_runner": attrs.source(default = "//components/shared/rust/build_tools:build_firmware.py"),
    },
)

def rust_firmware_targets(app_name, srcs, package_path, app_start_address,
                          gcc_toolchain, network_dep = "//network:network", node = None,
                          dependencies = ["//components/shared/rust:sources", "//components/shared/rust/build_tools:sources"],
                          test_packages = ["firmware-shared"], rust_toolchain = "1.86.0",
                          target = "thumbv7m-none-eabi", feature = "firmware",
                          platform_variants = [], debug_mcu = None, debug_interface = "stlink"):
    """Create the standard firmware/test/ELF/bin/CRC targets in this package."""
    generate_embedded_rust(name = "rust-protocol", network_dep = network_dep, node = node if node else app_name)
    for name, test in [("rust-firmware", False), ("rust-tests", True)]:
        firmware_cargo(name = name, srcs = srcs, package_path = package_path, binary = app_name,
                       dependencies = dependencies, protocol = ":rust-protocol", test = test,
                       test_packages = test_packages, rust_toolchain = rust_toolchain, target = target, feature = feature)
    alias(name = "elf", actual = ":rust-firmware[elf]")
    cxx_genrule(name = "bin", _cxx_toolchain = gcc_toolchain, srcs = [":elf"],
                out = app_name + ".bin", cmd = "$(objcopy) -O binary ${SRCS} ${OUT}")
    inject_crc(name = "bin_crc", src = ":bin", out = app_name + "_crc.bin",
               start_address = app_start_address, visibility = ["PUBLIC"])
    genrule(
        name = "rust-build-tests",
        srcs = {tool: "//components/shared/rust/build_tools:" + tool for tool in ["build_firmware.py", "test_tools.py"]},
        out = "passed",
        cmd = "python3 ${SRCDIR}/test_tools.py --output ${OUT}",
    )
    alias(name = app_name, actual = ":bin")
    if platform_variants:
        add_platform_deploy_targets(platform_variants = platform_variants, app_name = app_name)
    if debug_mcu:
        for platform, _variant in platform_variants:
            openocd_run(name = "gdb-{}".format(platform_output_name(platform)), toolchain = gcc_toolchain,
                        src = ":elf", interface = debug_interface, mcu = debug_mcu)
