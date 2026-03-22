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
    "RuntimeDependencyHandling",
    "ShlibInterfacesMode",
    "StripFlagsInfo",
)
load("@prelude//cxx:debug.bzl", "SplitDebugMode")
load("@prelude//cxx:headers.bzl", "HeaderMode")
load("@prelude//cxx:linker.bzl", "is_pdb_generated")
load("@prelude//linking:link_info.bzl", "LinkStyle")
load(":releases.bzl", "supported_host_triples", "zig_release")

ZigDistributionInfo = provider(
    fields = {
        "prefix": provider_field(typing.Any),
        "version": provider_field(str),
        "host_triple": provider_field(str),
        "target_triple": provider_field(str),
    },
)


def _zig_distribution_impl(ctx):
    prefix = ctx.actions.declare_output("zig")
    release = zig_release(ctx.attrs.version, ctx.attrs.host_triple)
    install_script = """
set -euo pipefail
prefix="$1"
url="$2"
strip_prefix="$3"
workdir="$(mktemp -d)"
trap 'rm -rf "$workdir"' EXIT
archive="$workdir/$strip_prefix.tar.xz"
curl -fsSL "$url" -o "$archive"
mkdir -p "$workdir/extract"
tar -xJf "$archive" -C "$workdir/extract"
mkdir -p "$prefix"
cp -R "$workdir/extract/$strip_prefix/." "$prefix/"
"""

    ctx.actions.run(
        cmd_args([
            "bash",
            "-eu",
            "-c",
            install_script,
            "--",
            prefix.as_output(),
            release.url,
            release.strip_prefix,
        ]),
        category = "install_zig_toolchain",
    )

    return [
        DefaultInfo(default_output = prefix),
        ZigDistributionInfo(
            prefix = prefix,
            version = ctx.attrs.version,
            host_triple = ctx.attrs.host_triple,
            target_triple = ctx.attrs.target_triple,
        ),
    ]


zig_distribution = rule(
    impl = _zig_distribution_impl,
    attrs = {
        "version": attrs.string(),
        "host_triple": attrs.string(),
        "target_triple": attrs.string(),
    },
)


def _zig_tool(prefix, subcommand, target_triple, extra_args = []):
    args = cmd_args(prefix, format = "{}/zig")
    if subcommand:
        args.add(subcommand)
    if target_triple:
        args.add("-target")
        args.add(target_triple)
    for arg in extra_args:
        args.add(arg)
    return RunInfo(args = args)


def _zig_toolchain_impl(ctx):
    prefix = ctx.attrs.distribution[ZigDistributionInfo].prefix
    target_triple = ctx.attrs.distribution[ZigDistributionInfo].target_triple
    platform_info = CxxPlatformInfo(name = target_triple)

    toolchain_info = CxxToolchainInfo(
        internal_tools = ctx.attrs._cxx_internal_tools[CxxInternalTools],
        c_compiler_info = CCompilerInfo(
            compiler = _zig_tool(prefix, "cc", target_triple),
            compiler_type = "clang",
            compiler_flags = cmd_args(ctx.attrs.c_compiler_flags),
            preprocessor = _zig_tool(prefix, "cc", target_triple, ["-E"]),
            preprocessor_type = "clang",
            preprocessor_flags = cmd_args(ctx.attrs.c_preprocessor_flags),
        ),
        cxx_compiler_info = CxxCompilerInfo(
            compiler = _zig_tool(prefix, "c++", target_triple),
            compiler_type = "clang",
            compiler_flags = cmd_args(ctx.attrs.cxx_compiler_flags),
            preprocessor = _zig_tool(prefix, "c++", target_triple, ["-E"]),
            preprocessor_type = "clang",
            preprocessor_flags = cmd_args(ctx.attrs.cxx_preprocessor_flags),
        ),
        linker_info = LinkerInfo(
            archiver = _zig_tool(prefix, "ar", None),
            archiver_type = "gnu",
            archiver_supports_argfiles = True,
            archive_objects_locally = False,
            binary_extension = "",
            generate_linker_maps = ctx.attrs.generate_linker_maps,
            link_binaries_locally = False,
            link_libraries_locally = False,
            link_style = LinkStyle(ctx.attrs.link_style),
            link_weight = 1,
            linker = _zig_tool(prefix, "cc", target_triple),
            linker_flags = cmd_args(ctx.attrs.linker_flags),
            lto_mode = "none",
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
            nm = _zig_tool(prefix, "nm", None),
            objcopy = _zig_tool(prefix, "objcopy", None),
            objdump = _zig_tool(prefix, "objdump", None),
            ranlib = _zig_tool(prefix, "ranlib", None),
            strip = _zig_tool(prefix, "strip", None),
        ),
        header_mode = HeaderMode("symlink_tree_only"),
        cpp_dep_tracking_mode = "makefile",
        as_compiler_info = CCompilerInfo(
            compiler = _zig_tool(prefix, "cc", target_triple),
            compiler_type = "clang",
            compiler_flags = cmd_args(ctx.attrs.as_compiler_flags),
            preprocessor = _zig_tool(prefix, "cc", target_triple, ["-E"]),
            preprocessor_type = "clang",
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
        use_dep_files = True,
        runtime_dependency_handling = RuntimeDependencyHandling("none"),
    )

    return [
        DefaultInfo(),
        toolchain_info,
        TemplatePlaceholderInfo(
            unkeyed_variables = {
                "ar": toolchain_info.linker_info.archiver,
                "cc": toolchain_info.c_compiler_info.compiler,
                "cflags": cmd_args(toolchain_info.c_compiler_info.compiler_flags, quote = "shell"),
                "cpp": cmd_args(toolchain_info.c_compiler_info.preprocessor, quote = "shell"),
                "cppflags": cmd_args(toolchain_info.c_compiler_info.preprocessor_flags, quote = "shell"),
                "cxx": toolchain_info.cxx_compiler_info.compiler,
                "cxxpp": cmd_args(toolchain_info.cxx_compiler_info.preprocessor, quote = "shell"),
                "cxxflags": cmd_args(toolchain_info.cxx_compiler_info.compiler_flags, quote = "shell"),
                "cxxppflags": cmd_args(toolchain_info.cxx_compiler_info.preprocessor_flags, quote = "shell"),
                "gdb": _zig_tool(prefix, None, None),
                "ld": toolchain_info.linker_info.linker,
                "ldflags-shared": cmd_args(toolchain_info.linker_info.linker_flags or [], quote = "shell"),
                "ldflags-static": cmd_args(toolchain_info.linker_info.linker_flags or [], quote = "shell"),
                "ldflags-static-pic": cmd_args(toolchain_info.linker_info.linker_flags or [], quote = "shell"),
                "objcopy": toolchain_info.binary_utilities_info.objcopy,
                "objdump": toolchain_info.binary_utilities_info.objdump,
                "strip": cmd_args(toolchain_info.binary_utilities_info.strip, quote = "shell"),
                "platform-name": target_triple,
                "as": toolchain_info.as_compiler_info.compiler,
                "asflags": cmd_args(toolchain_info.as_compiler_info.compiler_flags, quote = "shell"),
                "asppflags": cmd_args(toolchain_info.as_compiler_info.preprocessor_flags, quote = "shell"),
            },
        ),
        platform_info,
    ]


zig_toolchain = rule(
    impl = _zig_toolchain_impl,
    attrs = {
        "distribution": attrs.exec_dep(providers = [ZigDistributionInfo]),
        "c_compiler_flags": attrs.list(attrs.arg(), default = []),
        "c_preprocessor_flags": attrs.list(attrs.arg(), default = []),
        "cxx_compiler_flags": attrs.list(attrs.arg(), default = []),
        "cxx_preprocessor_flags": attrs.list(attrs.arg(), default = []),
        "as_compiler_flags": attrs.list(attrs.arg(), default = []),
        "as_preprocessor_flags": attrs.list(attrs.arg(), default = []),
        "link_style": attrs.enum(LinkStyle.values(), default = "static"),
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


def host_triple():
    arch = host_info().arch
    os = host_info().os
    host_arch = "aarch64" if arch.is_aarch64 else "x86_64" if arch.is_x86_64 else fail("Unsupported host arch '{}'.".format(arch))
    host_os = "macos" if os.is_macos else "linux" if os.is_linux else fail("Unsupported host os '{}'.".format(os))
    return "{}-{}".format(host_arch, host_os)


def download_zig_distribution(name, version, target_triple):
    host = host_triple()
    if host not in supported_host_triples:
        fail("Unsupported Zig host triple '{}'.".format(host))
    zig_distribution(
        name = name,
        version = version,
        host_triple = host,
        target_triple = target_triple,
    )
