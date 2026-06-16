RustPackageReleaseInfo = provider(
    fields = {
        "url": provider_field(str),
        "strip_prefix": provider_field(str),
    },
)

supported_host_triples = {
    "aarch64-apple-darwin": True,
    "x86_64-apple-darwin": True,
    "aarch64-unknown-linux-gnu": True,
    "x86_64-unknown-linux-gnu": True,
}


def rust_package_release(version, package_name, target):
    if target not in supported_host_triples and target != "aarch64-unknown-linux-gnu":
        fail("Unsupported Rust package target '{}'.".format(target))

    filename = "{}-{}-{}".format(package_name, version, target)
    return RustPackageReleaseInfo(
        url = "https://static.rust-lang.org/dist/{}.tar.xz".format(filename),
        strip_prefix = filename,
    )
