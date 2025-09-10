load("@prelude//http_archive:http_archive.bzl", "http_archive_impl")
load(":releases.bzl", "releases")
load("//tools/defs.bzl", "host_arch", "host_os")

OpenOCDReleaseInfo = provider(
    # @unsorted-dict-items
    fields = {
        "version": provider_field(typing.Any, default = None),
        "url": provider_field(typing.Any, default = None),
        "sha256": provider_field(typing.Any, default = None),
        "strip_prefix": provider_field(str, default = ""),
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

OpenOCDToolInfo = provider(
    # @unsorted-dict-items
    fields = {
        "version": provider_field(typing.Any, default = None),
        "arch": provider_field(typing.Any, default = None),
        "os": provider_field(typing.Any, default = None),
    },
)

def download_openocd_distribution(
        name: str,
        version: str,
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
    )

def _openocd_distribution_impl(ctx: AnalysisContext) -> list[Provider]:
    download_openocd_distribution(
        name = "dist-openocd-" + ctx.attrs.version,
        version = ctx.attrs.version,
        arch = ctx.attrs.arch or host_arch(),
        os = ctx.attrs.arch or host_os(),
    )
    dest = ctx.actions.declare_output("openocd")
    src = cmd_args(ctx.attrs.version[DefaultInfo].default_outputs[0], format = "{}/")
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
        ctx.attrs.version[DefaultInfo],
        RunInfo(args = compiler),
        OpenOCDToolInfo(
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
    is_toolchain_rule = True,
)

def openocd_tool(
        name: str,
        version: str,
        arch: [None, str] = None,
        os: [None, str] = None,
        **kwargs):
    openocd_distribution(
        name = version,
        version = version,
        arch = arch or host_arch(),
        os = arch or host_os(),
        **kwargs
    )
    bin = cmd_args(":openocd-" + version, format = "{}/bin/openocd")
    return [

    ]
