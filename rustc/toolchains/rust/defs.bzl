def _toolchain_sysroot_impl(ctx: AnalysisContext) -> list[Provider]:
    return [ctx.attrs.rust_toolchain[DefaultInfo]]

toolchain_sysroot = rule(
    impl = _toolchain_sysroot_impl,
    attrs = {
        "rust_toolchain": attrs.default_only(attrs.toolchain_dep(default = "toolchains//:rust")),
    },
)
