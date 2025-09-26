load(
    "@prelude//cxx:cxx_toolchain_types.bzl",
    "BinaryUtilitiesInfo",
    "CCompilerInfo",
    "CxxCompilerInfo",
    "CxxInternalTools",
    "CxxPlatformInfo",
    "CxxToolchainInfo",
    "LinkerInfo",
    "LinkerType",
    "PicBehavior",
    "ShlibInterfacesMode",
    "StripFlagsInfo",
    "cxx_toolchain_infos",
)
load(
    "@prelude//cxx:debug.bzl",
    "SplitDebugMode",
)
load(
    "@prelude//cxx:headers.bzl",
    "HeaderMode",
)
load(
    "@prelude//cxx:linker.bzl",
    "is_pdb_generated",
)
load(
    "@prelude//linking:link_info.bzl",
    "LinkStyle",
)
load(
    "@prelude//utils:utils.bzl",
    "flatten",
)
load(
    ":releases.bzl",
    "releases",
)

GccReleaseInfo = provider(
    # @unsorted-dict-items
    fields = {
        "version": provider_field(typing.Any, default = None),
        "url": provider_field(typing.Any, default = None),
        "sha256": provider_field(typing.Any, default = None),
        "strip_prefix": provider_field(str, default = ""),
    },
)

GccDistributionInfo = provider(
    # @unsorted-dict-items
    fields = {
        "version": provider_field(typing.Any, default = None),
        "arch": provider_field(typing.Any, default = None),
        "os": provider_field(typing.Any, default = None),
        "target_arch": provider_field(typing.Any, default = None),
        "target_os": provider_field(typing.Any, default = None),
        "target_abi": provider_field(typing.Any, default = None),
    },
)

def get_gcc_release(version: str, target_arch: str, target_os: str, target_abi: str) -> GccReleaseInfo | None:
    if not version in releases:
        fail("Unknown gcc release version '{}'. Available versions: {}".format(
            version,
            ", ".join(releases.keys()),
        ))
    gcc_version = releases[version]

    host_platform = "{}-{}".format(host_arch(), host_os())

    if not host_platform in gcc_version:
        fail("Unsupported host platform '{}'. Supported host platforms: {}".format(
            host_platform,
            ", ".join(gcc_version.keys()),
        ))

    gcc_targets = gcc_version[host_platform]

    target_triple = "{}-{}-{}".format(target_arch, target_os, target_abi)

    if not target_triple in gcc_targets:
        # FIXME: I don't think there's much we can do here, but maybe there's a better solution than this?
        return None
        fail("Unsupported target platform '{}' for this host platform '{}'. Supported platforms: {}".format(
            target_triple,
            host_platform,
            ", ".join(gcc_targets.keys()),
        ))

    release_info = gcc_targets[target_triple]

    return GccReleaseInfo(
        version = version,
        url = release_info["archive"],
        sha256 = release_info["sha256sum"],
        strip_prefix = release_info["strip_prefix"],
    )

def download_gcc_distribution(
        name: str,
        version: str,
        arch: str | None = None,
        os: str | None = None,
        target_arch: str | None = None,
        target_os: str | None = None,
        target_abi: str | None = None):
    arch = arch or host_arch()
    os = os or host_os()
    target_arch = target_arch or host_arch()
    target_os = target_os or host_os()
    target_abi = target_abi or ("gnu" if host_os() == "linux" else "elf" if host_os() == "macos" else fail("Target ABI was not specified and could not be determined automatically"))

    archive_name = name + "-archive"
    release = get_gcc_release(version, target_arch, target_os, target_abi)
    if release:
        native.http_archive(
            name = archive_name,
            urls = [release.url],
            sha256 = release.sha256,
            strip_prefix = release.strip_prefix,
        )
        gcc_distribution(
            name = name,
            dist = ":" + archive_name,
            version = release.version,
            arch = arch,
            os = os,
            target_arch = target_arch,
            target_os = target_os,
            target_abi = target_abi,
        )

def _gcc_distribution_impl(ctx: AnalysisContext) -> list[Provider]:
    dest = ctx.actions.declare_output("gcc-{}-{}-{}".format(
        ctx.attrs.target_arch,
        ctx.attrs.target_os,
        ctx.attrs.target_abi,
    ))
    src = cmd_args(ctx.attrs.dist[DefaultInfo].default_outputs[0], format = "{}/")
    ctx.actions.run(
        ["ln", "-sf", cmd_args(src, relative_to = (dest, 1)), dest.as_output()],
        category = "cp_compiler",
    )
    compiler = cmd_args(
        [dest],
        hidden = [
            ctx.attrs.dist[DefaultInfo].default_outputs,
            ctx.attrs.dist[DefaultInfo].other_outputs,
        ],
    )

    return [
        ctx.attrs.dist[DefaultInfo],
        RunInfo(args = compiler),
        GccDistributionInfo(
            version = ctx.attrs.version,
            arch = ctx.attrs.arch or host_arch(),
            os = ctx.attrs.os or host_os(),
            target_arch = ctx.attrs.target_arch,
            target_os = ctx.attrs.target_os,
            target_abi = ctx.attrs.target_abi,
        ),
    ]

gcc_distribution = rule(
    impl = _gcc_distribution_impl,
    attrs = {
        "dist": attrs.dep(providers = [DefaultInfo]),
        "version": attrs.string(),
        "arch": attrs.option(attrs.string(), default = None),
        "os": attrs.option(attrs.string(), default = None),
        "target_arch": attrs.option(attrs.string()),
        "target_os": attrs.option(attrs.string()),
        "target_abi": attrs.option(attrs.string()),
    },
)

def _incompatible_impl(ctx: AnalysisContext) -> list[Provider]:
    label = ctx.label.raw_target()
    setting = ConstraintSettingInfo(label = label)
    value = ConstraintValueInfo(setting = setting, label = label)

    return [
        DefaultInfo(),
        RunInfo(),
        GccDistributionInfo(),
        ConfigurationInfo(
            constraints = {label: value},
            values = {},
        ),
    ]

incompatible_rule = rule(
    attrs = {},
    impl = _incompatible_impl,
    doc = "A rule that produces nothing. Used for no-op dep in a select.",
    is_configuration_rule = True,
)

def host_arch() -> str:
    arch = host_info().arch
    if arch.is_x86_64:
        return "x86_64"
    elif host_info().arch.is_aarch64:
        return "aarch64"
    else:
        fail("Unsupported host architecture '{}'.".format(arch))

def host_os() -> str:
    os = host_info().os
    if os.is_linux:
        return "linux"
    elif os.is_macos:
        return "macos"
    elif os.is_windows:
        return "windows"
    else:
        fail("Unsupported host os '{}'.".format(os))

def _shell_quote(xs):
    return cmd_args(xs, quote = "shell")

def _gcc_toolchain_impl(ctx: AnalysisContext) -> list[Provider]:
    target_arch = ctx.attrs.distribution[GccDistributionInfo].target_arch
    target_os = ctx.attrs.distribution[GccDistributionInfo].target_os
    target_abi = ctx.attrs.distribution[GccDistributionInfo].target_abi
    bin = cmd_args(
        ctx.attrs.distribution[RunInfo],
        format = "{}/bin/" + "{}-none-{}-{}-".format(target_arch, target_os, target_abi) if target_os != "none" else "{}/bin/" + "{}-none-{}-".format(target_arch, target_abi),
    )
    platform_name = target_arch
    platform_info = CxxPlatformInfo(name = platform_name)

    toolchain_info = CxxToolchainInfo(
        internal_tools = ctx.attrs._cxx_internal_tools[CxxInternalTools],
        c_compiler_info = CCompilerInfo(
            compiler = RunInfo(args = cmd_args(bin, format = "{}gcc")),
            compiler_type = "gnu",
            compiler_flags = cmd_args(ctx.attrs.c_compiler_flags),
            preprocessor = RunInfo(args = cmd_args(bin, format = "{}cpp")),
            preprocessor_type = "gcc",
            preprocessor_flags = cmd_args(ctx.attrs.c_preprocessor_flags),
        ),
        cxx_compiler_info = CxxCompilerInfo(
            compiler = RunInfo(args = cmd_args(bin, format = "{}g++")),
            compiler_type = "gnu",
            compiler_flags = cmd_args(ctx.attrs.cxx_compiler_flags),
            preprocessor = RunInfo(args = cmd_args(bin, format = "{}cpp")),
            preprocessor_type = "gnu",
            preprocessor_flags = cmd_args(ctx.attrs.cxx_preprocessor_flags),
        ),
        linker_info = LinkerInfo(
            archiver = RunInfo(args = cmd_args(bin, format = "{}gcc-ar")),
            archiver_type = "gnu",
            archiver_supports_argfiles = True,
            archive_objects_locally = False,
            binary_extension = "",
            generate_linker_maps = ctx.attrs.generate_linker_maps,
            link_binaries_locally = False,
            link_libraries_locally = False,
            link_style = LinkStyle(ctx.attrs.link_style),
            link_weight = 1,
            linker = RunInfo(args = cmd_args(bin, format = "{}gcc")),
            linker_flags = cmd_args(ctx.attrs.linker_flags),
            lto_mode = "thin",
            object_file_extension = "o",
            shlib_interfaces = ShlibInterfacesMode("disabled"),
            shared_dep_runtime_ld_flags = ctx.attrs.shared_dep_runtime_ld_flags,
            shared_library_name_default_prefix = "lib",
            shared_library_name_format = "{}.so",
            shared_library_versioned_name_format = "{}.so.{}",
            static_dep_runtime_ld_flags = ctx.attrs.static_dep_runtime_ld_flags,
            static_library_extension = "a",
            static_pic_dep_runtime_ld_flags = ctx.attrs.static_pic_dep_runtime_ld_flags,
            independent_shlib_interface_linker_flags = ctx.attrs.shared_library_interface_flags,
            type = LinkerType("gnu"),
            use_archiver_flags = True,
            is_pdb_generated = is_pdb_generated(LinkerType("gnu"), ctx.attrs.linker_flags),
        ),
        binary_utilities_info = BinaryUtilitiesInfo(
            nm = RunInfo(args = cmd_args(bin, format = "{}gcc-nm")),
            objcopy = RunInfo(args = cmd_args(bin, format = "{}objcopy")),
            objdump = RunInfo(args = cmd_args(bin, format = "{}objdump")),
            ranlib = RunInfo(args = cmd_args(bin, format = "{}gcc-ranlib")),
            strip = RunInfo(args = cmd_args(bin, format = "{}strip")),
        ),
        header_mode = HeaderMode("symlink_tree_only"),
        as_compiler_info = CCompilerInfo(
            compiler = RunInfo(args = cmd_args(bin, format = "{}gcc")),
            compiler_type = "gnu",
            compiler_flags = cmd_args(ctx.attrs.as_compiler_flags),
            preprocessor = RunInfo(args = cmd_args(bin, format = "{}cpp")),
            preprocessor_type = "gnu",
            preprocessor_flags = cmd_args(ctx.attrs.as_preprocessor_flags),
        ),
        strip_flags_info = StripFlagsInfo(
            strip_debug_flags = ctx.attrs.strip_debug_flags,
            strip_non_global_flags = ctx.attrs.strip_non_global_flags,
            strip_all_flags = ctx.attrs.strip_all_flags,
        ),
        split_debug_mode = SplitDebugMode("none"),
        pic_behavior = PicBehavior("supported"),
        bolt_enabled = False,
    )

    # most of these are taken straight from the prelude's `cxx_toolchain_infos()` function,
    # with the exception of `cpp`, `cxxpp`, and `strip` which I have added
    unkeyed_variables = {
        "ar": toolchain_info.linker_info.archiver,
        "cc": toolchain_info.c_compiler_info.compiler,
        "cflags": _shell_quote(toolchain_info.c_compiler_info.compiler_flags),
        "cpp": _shell_quote(toolchain_info.c_compiler_info.preprocessor),
        "cppflags": _shell_quote(toolchain_info.c_compiler_info.preprocessor_flags),
        "cxx": toolchain_info.cxx_compiler_info.compiler,
        "cxxpp": _shell_quote(toolchain_info.cxx_compiler_info.preprocessor),
        "cxxflags": _shell_quote(toolchain_info.cxx_compiler_info.compiler_flags),
        "cxxppflags": _shell_quote(toolchain_info.cxx_compiler_info.preprocessor_flags),
        "ld": toolchain_info.linker_info.linker,
        "ldflags-shared": _shell_quote(toolchain_info.linker_info.linker_flags or []),
        "ldflags-static": _shell_quote(toolchain_info.linker_info.linker_flags or []),
        "ldflags-static-pic": _shell_quote(toolchain_info.linker_info.linker_flags or []),
        "objcopy": toolchain_info.binary_utilities_info.objcopy,
        "objdump": toolchain_info.binary_utilities_info.objdump,
        "strip": _shell_quote(toolchain_info.binary_utilities_info.strip),
        "platform-name": platform_name,
        "as": toolchain_info.as_compiler_info.compiler,
        "asflags": _shell_quote(toolchain_info.as_compiler_info.compiler_flags),
        "asppflags": _shell_quote(toolchain_info.as_compiler_info.preprocessor_flags),
    }

    placeholders_info = TemplatePlaceholderInfo(unkeyed_variables = unkeyed_variables)

    return [ctx.attrs.distribution[DefaultInfo], toolchain_info, placeholders_info, platform_info]

gcc_toolchain = rule(
    impl = _gcc_toolchain_impl,
    attrs = {
        "c_compiler_flags": attrs.list(attrs.arg(), default = []),
        "c_preprocessor_flags": attrs.list(attrs.arg(), default = []),
        "cxx_compiler_flags": attrs.list(attrs.arg(), default = []),
        "cxx_preprocessor_flags": attrs.list(attrs.arg(), default = []),
        "as_compiler_flags": attrs.list(attrs.arg(), default = []),
        "as_preprocessor_flags": attrs.list(attrs.arg(), default = []),
        "distribution": attrs.exec_dep(providers = [RunInfo, GccDistributionInfo]),
        "link_style": attrs.enum(
            LinkStyle.values(),
            default = "static",
            doc = """
            The default value of the `link_style` attribute for rules that use this toolchain.
            """,
        ),
        "linker_flags": attrs.list(attrs.arg(), default = []),
        "generate_linker_maps": attrs.bool(default = True),
        "shared_dep_runtime_ld_flags": attrs.list(attrs.arg(), default = []),
        "shared_library_interface_flags": attrs.list(attrs.string(), default = []),
        "static_dep_runtime_ld_flags": attrs.list(attrs.arg(), default = []),
        "static_pic_dep_runtime_ld_flags": attrs.list(attrs.arg(), default = []),
        "strip_all_flags": attrs.option(attrs.list(attrs.arg()), default = None),
        "strip_debug_flags": attrs.option(attrs.list(attrs.arg()), default = None),
        "strip_non_global_flags": attrs.option(attrs.list(attrs.arg()), default = None),
        "_cxx_internal_tools": attrs.default_only(attrs.dep(providers = [CxxInternalTools], default = "prelude//cxx/tools:internal_tools")),
    },
    is_toolchain_rule = True,
)

def _generate_asms_impl(ctx: AnalysisContext) -> list[Provider]:
    outputs = []

    for file, provider in ctx.attrs.binary[DefaultInfo].sub_targets["objects"][DefaultInfo].sub_targets.items():
        if file.endswith(".s.o") or file.endswith(".S.o"):
            continue

        sub_targets = provider[DefaultInfo].sub_targets
        maybe_assembly = sub_targets.get("assembly")
        if maybe_assembly:
            outputs.extend(maybe_assembly[DefaultInfo].default_outputs)

    return [DefaultInfo(default_outputs = outputs)]

generate_asms = rule(
    impl = _generate_asms_impl,
    attrs = {
        "binary": attrs.dep(),
    },
)

def _generate_stripped_asms_impl(ctx: AnalysisContext) -> list[Provider]:
    outputs = []
    for target in ctx.attrs.targets:
        default_output = target[DefaultInfo].default_outputs

        # find all object files in this target
        objects = default_output + flatten(
            [
                providers[DefaultInfo].default_outputs
                for providers in target[DefaultInfo].sub_targets["objects"][DefaultInfo].sub_targets.values()
            ],
        )

        # produce stripped object and corresponding asm for each object file
        for object in [
            obj
            for obj in objects
            if not obj.basename.endswith(".S.o") and
               not obj.basename.endswith(".s.o") and
               not obj.basename.endswith(".a") and
               not obj.basename.endswith(".pic.o")
        ]:
            stripped_object = ctx.actions.declare_output(object.short_path.replace(".o", "_stripped.o").replace(".elf", "_stripped.elf"))

            ctx.actions.run(
                [
                    ctx.attrs.toolchain[TemplatePlaceholderInfo].unkeyed_variables["strip"],
                    cmd_args(object),
                    "-o",
                    stripped_object.as_output(),
                ],
                category = "stripped_object",
                identifier = object.short_path,
            )

            stripped_asm = ctx.actions.declare_output(object.short_path.replace(".o", ".asms").replace(".elf", ".asms"))
            outputs.append(stripped_asm)

            # there must be a better way to do this...
            # Couldn't figure out how to get `>` into the command without it getting quoted
            cmd = cmd_args(
                "/usr/bin/env",
                "bash",
                "-c",
                cmd_args(
                    ctx.attrs.toolchain[TemplatePlaceholderInfo].unkeyed_variables["objdump"],
                    "-D",
                    stripped_object,
                    cmd_args(stripped_asm.as_output(), format = ">{}"),
                    delimiter = " ",
                ),
            )

            t = ctx.actions.run(
                cmd,
                category = "stripped_asm",
                identifier = object.short_path,
            )

    return [DefaultInfo(default_outputs = outputs)]

generate_stripped_asms = rule(
    impl = _generate_stripped_asms_impl,
    attrs = {
        "targets": attrs.list(attrs.dep()),
        "toolchain": attrs.toolchain_dep(providers = [CxxToolchainInfo, TemplatePlaceholderInfo]),
    },
)
