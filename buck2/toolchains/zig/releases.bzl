ZigReleaseInfo = provider(
    fields = {
        "url": provider_field(str),
        "strip_prefix": provider_field(str),
    },
)

supported_host_triples = {
    "aarch64-macos": True,
    "x86_64-macos": True,
    "aarch64-linux": True,
    "x86_64-linux": True,
}


def zig_release(version, host_triple):
    if host_triple not in supported_host_triples:
        fail("Unsupported Zig host triple '{}'.".format(host_triple))

    filename = "zig-{}-{}".format(host_triple, version)
    return ZigReleaseInfo(
        url = "https://ziglang.org/download/{}/{}.tar.xz".format(version, filename),
        strip_prefix = filename,
    )
