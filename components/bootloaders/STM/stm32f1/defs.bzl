load("@prelude//:rules.bzl", __rules__ = "rules")
load("@toolchains//gcc/defs.bzl", "generate_asms", "generate_stripped_asms")
load("//drive-stack/conUDS/defs.bzl", "conUDS_bootloader_download", "conUDS_download")
load("//embedded/defs.bzl", "preprocess_linkscript", "produce_bin")
load("//embedded/openocd/defs.bzl", "openocd_run")
load("//tools/feature-tree/defs.bzl", "generate_feature_tree")
load("//tools/hextools/defs.bzl", "inject_crc")

SupportedMicrocontrollers = enum(
    "f103",
    "f105",
)

OutputType = enum(
    "bootloader",
    "updater",
)

BootloaderVariant = record(
    name = str,
    config_id = int,
    has_updater = bool,
    start_address = field(int, 0x08000000),
    updater_offset = field(int, 0x2000),
    toolchain = field(str, "@toolchains//:gcc-14.2.rel1-arm-none-eabi"),
    target = SupportedMicrocontrollers,
)

def bootloader(
        variant: BootloaderVariant,
        toolchain: str,
        compiler_flags: list[str],
        linker_flags: list[str]):
    _bootloader_impl(variant, compiler_flags, linker_flags, False)
    conUDS_bootloader_download(
        name = "{}-download-bootloader".format(variant.config_id),
        binary = ":{}-bin-crc".format(variant.config_id),
        manifest = "//network:manifest-uds",
        node = variant.name,
    )

    if variant.has_updater:
        _bootloader_impl(variant, compiler_flags, linker_flags, True)
        conUDS_download(
            name = "{}-download-updater".format(variant.config_id),
            binary = ":{}-updater-bin-crc".format(variant.config_id),
            manifest = "//network:manifest-uds",
            node = variant.name,
        )

def _bootloader_impl(
        variant: BootloaderVariant,
        compiler_flags: list[str],
        linker_flags: list[str],
        is_updater: bool):
    name = variant.name
    config_id = variant.config_id
    toolchain = variant.toolchain
    app_name = is_updater and "blu" or "bl"
    name_prefix = "{}".format(config_id) + ("-updater-{}" if is_updater else "-{}")

    generate_feature_tree(
        name = name_prefix.format("feature-tree"),
        config_id = config_id,
        build_renderer = "renderers/BuildDefines_generated.h.mako.buck2",
        srcs = {
            "variants.yaml": "variants.yaml",
            "STM32F103xB_FeatureSels.yaml": "//components/shared:FeatureSels/STM32F103xB_FeatureSels.yaml",
            "Controller_FeatureDefs.yaml": "//components/shared:FeatureDefs/Controller_FeatureDefs.yaml",
            "Application_FeatureDefs.yaml": "//components/shared:FeatureDefs/Application_FeatureDefs.yaml",
            "NVM_FeatureDefs.yaml": "//components/shared:FeatureDefs/NVM_FeatureDefs.yaml",
            "FeatureDefs.yaml": "FeatureDefs.yaml",
            "FeatureSels.yaml": "FeatureSels.yaml",
        } | (
            {
                "BOOTUPDATER_V1_FeatureSels.yaml": "//components/shared:FeatureSels/BOOTUPDATER_V1_FeatureSels.yaml",
            } if is_updater else {
                "BOOT_V1_FeatureSels.yaml": "//components/shared:FeatureSels/BOOT_V1_FeatureSels.yaml",
            }
        ),
        **{"{}_config_id".format(app_name): "{}U".format(config_id)}
    )

    __rules__["cxx_library"](
        name = name_prefix.format("udsServer"),
        _cxx_toolchain = toolchain,
        compiler_flags = compiler_flags + [
            "-Wno-missing-prototypes",
            "-Wno-unused-parameter",
            "-DBYTE_ORDER=_BYTE_ORDER",
            "-DLITTLE_ENDIAN=_LITTLE_ENDIAN",
            "-Wno-inline",
            "-Wno-conversion",
        ],
        srcs = [
            "//embedded/libs:isotp[isotp.c]",
            "//embedded/libs/uds:lib_udsServer.c",
        ],
        header_namespace = "",
        exported_headers = {
            "lib_uds.h": "//embedded/libs/uds:lib_uds.h",
        },
        deps = [
            ":" + name_prefix.format("feature-tree"),
        ],
        headers = {
            "uds_componentSpecific.h": "include/uds_componentSpecific.h",
            "isotp.h": "//embedded/libs:isotp[include/isotp.h]",
            "isotp_config.h": "//embedded/libs:isotp[include/isotp_config.h]",
            "isotp_defines.h": "//embedded/libs:isotp[include/isotp_defines.h]",
            "isotp_user.h": "//embedded/libs:isotp[include/isotp_user.h]",
        },
    )

    preprocess_linkscript(
        name = name_prefix.format("linkscript"),
        toolchain = toolchain,
        linkscript = "STM32F1.ld",
        srcs = {
            "BuildDefines.h": "include/HW/BuildDefines.h",
            "FeatureDefines.h": "//tools/feature-tree:FeatureDefines.h",
            "BuildDefines_generated.h": ":" + name_prefix.format("feature-tree-codegen") + "[BuildDefines_generated.h]",
            "FeatureDefines_generated.h": ":" + name_prefix.format("feature-tree-codegen") + "[FeatureDefines_generated.h]",
        },
        compiler_flags = compiler_flags,
        force_includes = ["BuildDefines.h"],
        out = "linkscript",
    )

    __rules__["cxx_binary"](
        name = name_prefix.format("elf"),
        _cxx_toolchain = toolchain,
        compiler_flags = compiler_flags,
        linker_flags = compiler_flags + linker_flags + ["-T", "$(location :{})".format(name_prefix.format("linkscript"))],
        executable_name = "{}-{}-{}.elf".format(app_name, name, config_id),
        srcs =
            glob(["src/**/*.c", "src/**/*.S"]) + [
                "//embedded/libs:libcrc.c",
                "//components/shared/code:libs/LIB_app.c",
            ],
        header_namespace = "",
        headers = {
            "lib_atomic.h": "//embedded/libs:lib_atomic.h",
            "libcrc.h": "//embedded/libs:libcrc.h",
        },
        include_directories = [
            "include/",
            "include/HW/",
        ],
        deps = [
            ":" + name_prefix.format("udsServer"),
            ":" + name_prefix.format("feature-tree"),
            "//components/shared/code:headers",
            "//embedded/libs/cells:cells",
        ],
    )

    generate_asms(
        name = name_prefix.format("asm"),
        binary = ":" + name_prefix.format("elf"),
    )

    generate_stripped_asms(
        name = name_prefix.format("asms"),
        targets = [
            ":" + name_prefix.format("elf"),
            ":" + name_prefix.format("udsServer"),
        ],
        toolchain = toolchain,
    )

    produce_bin(
        name = name_prefix.format("bin"),
        toolchain = toolchain,
        src = ":" + name_prefix.format("elf"),
        out = "{}-{}-{}".format(app_name, name, config_id),
    )

    inject_crc(
        name = name_prefix.format("bin-crc"),
        src = ":" + name_prefix.format("bin"),
        out = "{}-{}-{}_crc.bin".format(app_name, name, config_id),
        start_address = variant.start_address + (variant.updater_offset if is_updater else 0),
        visibility = ["PUBLIC"],
    )

    openocd_run(
        name = "gdb-{}-{}".format(config_id, "blu" if is_updater else "bl"),
        toolchain = toolchain,
        src = ":" + name_prefix.format("elf"),
        interface = "stlink",
        mcu = "stm32f103c8",
    )
