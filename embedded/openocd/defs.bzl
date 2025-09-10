load("@prelude//http_archive:http_archive.bzl", "http_archive_impl")
load("@prelude//cxx:cxx_toolchain_types.bzl", "CxxToolchainInfo")
load("//tools/defs.bzl", "host_arch", "host_os")
load(":releases.bzl", "releases")

OpenOCDReleaseInfo = provider(
    # @unsorted-dict-items
    fields = {
        "version": provider_field(typing.Any, default = None),
        "url": provider_field(typing.Any, default = None),
        "sha256": provider_field(typing.Any, default = None),
        "strip_prefix": provider_field(str, default = ""),
    },
)

OpenOCDDistributionInfo = provider(
    # @unsorted-dict-items
    fields = {
        "version": provider_field(typing.Any, default = None),
        "arch": provider_field(typing.Any, default = None),
        "os": provider_field(typing.Any, default = None),
    },
)

def get_openocd_release(version: str, target_arch: str, target_os: str) -> OpenOCDReleaseInfo:
    if not version in releases:
        fail("Unknown OpenOCD release version '{}'. Available versions: {}".format(
            version,
            ", ".join(releases.keys()),
        ))
    openocd_version = releases[version]

    platform = "{}-{}".format(host_arch(), host_os())
    if not platform in openocd_version:
        fail("Unsupported platform '{}'. Supported platforms: {}".format(
            platform,
            ", ".join(openocd_version.keys()),
        ))
    openocd_platform = openocd_version[platform]

    return OpenOCDReleaseInfo(
        version = openocd_version.get("version", version),
        url = openocd_platform["archive"],
        sha256 = openocd_platform["sha256sum"],
        strip_prefix = openocd_platform["strip_prefix"],
    )

def download_openocd_distribution(
        name: str,
        version: str,
        visibility: list[str],
        arch: [None, str] = None,
        os: [None, str] = None):
    arch = arch or host_arch()
    os = os or host_os()

    archive_name = name + "-archive"
    release = get_openocd_release(version, arch, os)
    native.http_archive(
        name = archive_name,
        urls = [release.url],
        sha256 = release.sha256,
        strip_prefix = release.strip_prefix,
    )
    openocd_distribution(
        name = name,
        dist = ":" + archive_name,
        version = release.version,
        arch = arch,
        os = os,
        visibility = visibility,
    )

def _openocd_distribution_impl(ctx: AnalysisContext) -> list[Provider]:
    dest = ctx.actions.declare_output("openocd")
    src = cmd_args(ctx.attrs.dist[DefaultInfo].default_outputs[0], format = "{}/")
    ctx.actions.run(
        ["ln", "-sf", cmd_args(src, relative_to = (dest, 1)), dest.as_output()],
        category = "debug",
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
        OpenOCDDistributionInfo(
            version = ctx.attrs.version,
            arch = ctx.attrs.arch or host_arch(),
            os = ctx.attrs.os or host_os(),
        ),
    ]

openocd_distribution = rule(
    impl = _openocd_distribution_impl,
    attrs = {
        "version": attrs.string(),
        "dist": attrs.dep(providers = [DefaultInfo]),
        "arch": attrs.option(attrs.string(), default = None),
        "os": attrs.option(attrs.string(), default = None),
    },
)

def _openocd_run_impl(ctx: AnalysisContext) -> list[Provider]:
    # example command:
    # gdb {src} -ex 'target extended-remote | 'openocd -s embedded/openocd -f stlink.cfg -f stm32f103c8.cfg -c "gdb_port pipe"' -ex "monitor reset"
    gdb = ctx.attrs.toolchain[TemplatePlaceholderInfo].unkeyed_variables["gdb"]
    src = ctx.attrs.src
    interface = ctx.attrs.interface
    mcu = ctx.attrs.mcu
    repo = ctx.attrs.repository[DefaultInfo].default_outputs[0]

    openocd = cmd_args(ctx.attrs.distribution[RunInfo], format = "{}/bin/openocd")

    return [
        DefaultInfo(),
        RunInfo(args = cmd_args(
            gdb,
            src[DefaultInfo].default_outputs[0],
            "-ex",
            cmd_args(
                "target",
                "extended-remote",
                "|",
                cmd_args(
                    openocd,
                    "-s",
                    repo,
                    "-f",
                    "interface/{}.cfg".format(interface),
                    "-f",
                    "mcu/{}.cfg".format(mcu),
                    "-c",
                    "'gdb_port pipe'",
                ),
                delimiter = " ",
            ),
            "-ex",
            "monitor reset",
        )),
    ]

openocd_run = rule(
    impl = _openocd_run_impl,
    attrs = {
        "distribution": attrs.exec_dep(providers = [RunInfo, OpenOCDDistributionInfo], default = "//embedded/openocd:dist-openocd"),
        "src": attrs.exec_dep(providers = [DefaultInfo]),
        "interface": attrs.string(),
        "mcu": attrs.string(),
        "toolchain": attrs.toolchain_dep(providers = [CxxToolchainInfo], default = "@toolchains//:cxx"),
        "repository": attrs.exec_dep(default = "//embedded/openocd:repository"),
    },
)
