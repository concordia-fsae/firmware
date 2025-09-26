load("//tools/uv/defs.bzl", "UvTool")

def _inject_crc_impl(ctx: AnalysisContext) -> list[Provider]:
    tool = ctx.attrs.tool[UvTool]
    out = ctx.actions.declare_output("out/" + ctx.attrs.out)

    src = ctx.attrs.src[DefaultInfo].default_outputs[0]
    src = ctx.actions.symlink_file("srcs/" + src.short_path, src)

    ctx.actions.run(
        cmd_args([
            tool.python,
            cmd_args(tool.tooldir, format = "{}/hextools.py"),
            "--input",
            cmd_args(src),
            "--output",
            cmd_args(out.as_output()),
            "--start_address",
            "{}".format(ctx.attrs.start_address),
        ]),
        category = "test",
    )

    return [DefaultInfo(default_outputs = [out])]

inject_crc = rule(
    impl = _inject_crc_impl,
    attrs = {
        "src": attrs.dep(),
        "out": attrs.string(),
        "start_address": attrs.int(),
        "tool": attrs.toolchain_dep(providers = [UvTool], default = "//tools/hextools:hextools"),
    },
)
