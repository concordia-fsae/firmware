load("@prelude//rust:rust_toolchain.bzl", "PanicRuntime", "RustToolchainInfo")

def _rust_toolchain_impl(ctx):
    return [
        DefaultInfo(),
        RustToolchainInfo(
            compiler = ctx.attrs.rustc[RunInfo],
            rustdoc = ctx.attrs.rustdoc[RunInfo],
            clippy_driver = ctx.attrs.clippy_driver[RunInfo] if ctx.attrs.clippy_driver else None,
            rustfmt = ctx.attrs.rustfmt[RunInfo] if ctx.attrs.rustfmt else None,
            cargo = ctx.attrs.cargo[RunInfo] if ctx.attrs.cargo else None,

            # Optional metadata
            default_edition = ctx.attrs.default_edition,
            rustc_target_triple = ctx.attrs.rustc_target_triple,
            rustc_flags = ctx.attrs.rustc_flags,
            rustdoc_flags = ctx.attrs.rustdoc_flags,
            panic_runtime = PanicRuntime("unwind"),

            # Useful for debugging
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
        # Paths to the actual tool binaries.
        "rustc": attrs.dep(providers = [RunInfo]),
        "rustdoc": attrs.dep(providers = [RunInfo]),
        "clippy_driver": attrs.option(attrs.dep(providers = [RunInfo]), default = None),
        "rustfmt": attrs.option(attrs.dep(providers = [RunInfo]), default = None),
        "cargo": attrs.option(attrs.dep(providers = [RunInfo]), default = None),

        # Config / metadata
        "default_edition": attrs.string(default = "2024"),
        "rustc_target_triple": attrs.string(default = "x86_64-unknown-linux-gnu"),
        "rustc_flags": attrs.list(attrs.arg(), default = []),
        "rustdoc_flags": attrs.list(attrs.arg(), default = []),
        "deny_lints": attrs.list(attrs.string(), default = []),
        "warn_lints": attrs.list(attrs.string(), default = []),
        "allow_lints": attrs.list(attrs.string(), default = []),

        # Optional toggles
        "nightly_features": attrs.bool(default = False),
        "doctests": attrs.bool(default = True),
        "report_unused_deps": attrs.bool(default = False),
    },
    is_toolchain_rule = True,
    doc = "Hermetic Rust toolchain wrapping stage2 rustc/rustdoc/etc.",
)
