load("@prelude//rust:rust_toolchain.bzl", "PanicRuntime", "RustToolchainInfo")
load(":releases.bzl", "rust_package_release", "supported_host_triples")

RustDistributionInfo = provider(
    fields = {
        "prefix": provider_field(typing.Any),
        "version": provider_field(str),
        "host_triple": provider_field(str),
    },
)


def _rust_distribution_impl(ctx):
    prefix = ctx.actions.declare_output("toolchain")
    package_specs = [
        ("rustc", ctx.attrs.host_triple),
        ("cargo", ctx.attrs.host_triple),
        ("rust-std", ctx.attrs.host_triple),
        ("clippy", ctx.attrs.host_triple),
        ("rustfmt", ctx.attrs.host_triple),
    ] + [
        ("rust-std", target)
        for target in ctx.attrs.extra_targets
        if target != ctx.attrs.host_triple
    ]
    releases = [
        rust_package_release(
            version = ctx.attrs.version,
            package_name = package_name,
            target = target,
        )
        for package_name, target in package_specs
    ]
    install_script = """
set -euo pipefail
prefix="$1"
shift
workdir="$(mktemp -d)"
trap 'rm -rf "$workdir"' EXIT
mkdir -p "$prefix"
while [ "$#" -gt 0 ]; do
  url="$1"
  strip_prefix="$2"
  shift 2
  archive="$workdir/$strip_prefix.tar.xz"
  extract_dir="$workdir/extract"
  mkdir -p "$extract_dir"
  curl -fsSL "$url" -o "$archive"
  tar -xJf "$archive" -C "$extract_dir"
  "$extract_dir/$strip_prefix/install.sh" --prefix="$prefix" --disable-ldconfig
done
"""

    install_args = ["bash", "-eu", "-c", install_script, "--", prefix.as_output()]
    for release in releases:
        install_args.extend([release.url, release.strip_prefix])

    ctx.actions.run(
        cmd_args(install_args),
        category = "install_rust_toolchain",
    )

    return [
        DefaultInfo(default_output = prefix),
        RustDistributionInfo(
            prefix = prefix,
            version = ctx.attrs.version,
            host_triple = ctx.attrs.host_triple,
        ),
    ]


rust_distribution = rule(
    impl = _rust_distribution_impl,
    attrs = {
        "version": attrs.string(),
        "host_triple": attrs.string(),
        "extra_targets": attrs.list(attrs.string(), default = []),
    },
)


def _tool(prefix, binary):
    return RunInfo(args = cmd_args(prefix, format = "{}/bin/{}".format("{}", binary)))


def _rust_toolchain_impl(ctx):
    prefix = ctx.attrs.distribution[RustDistributionInfo].prefix
    rustc_target_triple = ctx.attrs.rustc_target_triple or ctx.attrs.distribution[RustDistributionInfo].host_triple

    return [
        DefaultInfo(),
        RustToolchainInfo(
            compiler = _tool(prefix, "rustc"),
            rustdoc = _tool(prefix, "rustdoc"),
            clippy_driver = _tool(prefix, "clippy-driver"),
            default_edition = ctx.attrs.default_edition,
            rustc_target_triple = rustc_target_triple,
            rustc_flags = ctx.attrs.rustc_flags,
            rustdoc_flags = ctx.attrs.rustdoc_flags,
            panic_runtime = PanicRuntime("unwind"),
            deny_lints = ctx.attrs.deny_lints,
            warn_lints = ctx.attrs.warn_lints,
            allow_lints = ctx.attrs.allow_lints,
            nightly_features = ctx.attrs.nightly_features,
            doctests = ctx.attrs.doctests,
            report_unused_deps = ctx.attrs.report_unused_deps,
        ),
    ]


rust_toolchain = rule(
    impl = _rust_toolchain_impl,
    attrs = {
        "distribution": attrs.dep(providers = [RustDistributionInfo]),
        "default_edition": attrs.string(default = "2024"),
        "rustc_target_triple": attrs.option(attrs.string(), default = None),
        "rustc_flags": attrs.list(attrs.arg(), default = []),
        "rustdoc_flags": attrs.list(attrs.arg(), default = []),
        "deny_lints": attrs.list(attrs.string(), default = []),
        "warn_lints": attrs.list(attrs.string(), default = []),
        "allow_lints": attrs.list(attrs.string(), default = []),
        "nightly_features": attrs.bool(default = False),
        "doctests": attrs.bool(default = True),
        "report_unused_deps": attrs.bool(default = False),
    },
    is_toolchain_rule = True,
    doc = "Pinned Rust toolchain installed from official prebuilt distribution archives.",
)

def download_rust_distribution(name, version, host_triple, extra_targets = []):
    if host_triple not in supported_host_triples:
        fail("Unsupported Rust host triple '{}'. Supported values: {}".format(
            host_triple,
            ", ".join(sorted(supported_host_triples)),
        ))

    rust_distribution(
        name = name,
        version = version,
        host_triple = host_triple,
        extra_targets = extra_targets,
    )
