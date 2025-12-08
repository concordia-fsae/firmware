load("//tools/uv/defs.bzl", "UvTool")

def _configurator_impl(ctx: AnalysisContext) -> list[Provider]:
    tool = ctx.attrs.tool[UvTool]
    out = ctx.attrs.out

    src = ctx.attrs.src
    src = ctx.actions.symlink_file("srcs/" + src.short_path, src)

    templates = {template.short_path: template for template in ctx.attrs.templates}
    outs = []

    for template in templates:
        outs.append(ctx.actions.declare_output(out + "/{}".format(template.removesuffix(".mako"))))

    templates_dir = ctx.actions.symlinked_dir("templates", templates)
    headers = ctx.actions.symlinked_dir("headers", {o.short_path.lstrip("out/"): o for o in outs if o.extension == ".h"})

    ctx.actions.run(
        cmd_args(
            [
                tool.python,
                cmd_args(tool.tooldir, format = "{}/configurator.py"),
                "--input",
                cmd_args(src),
                "--output-dir",
                cmd_args(src, format = "{{}}/{}".format(out), parent = 2),
                "--template-dir",
                cmd_args(templates_dir),
            ],
            hidden = [out.as_output() for out in outs],
        ),
        category = "configurator",
    )

    return [
        DefaultInfo(
            default_outputs = outs,
            sub_targets = {
                "srcs": [
                    DefaultInfo(default_outputs = [
                        file
                        for file in outs
                        if file.short_path.endswith(".c")
                    ]),
                ],
                "headers": [
                    DefaultInfo(default_outputs = [headers]),
                ],
            },
        ),
    ]

configurator_rule = rule(
    impl = _configurator_impl,
    attrs = {
        "src": attrs.source(allow_directory = True, doc = """
            Directory containing yaml files
            (or other directories that, in turn, contain yaml files) that should be processed by
            the configurator.
        """),
        "templates": attrs.list(attrs.source()),
        "out": attrs.string(default = "out", doc = "Directory in which configurator outputs will be placed. Default: `out`"),
        "tool": attrs.toolchain_dep(providers = [UvTool], default = "//embedded/tools/configurator:configurator_tool"),
    },
)

def run_configurator(name: str, src: str, templates: list[str], out: str = "out"):
    configurator_rule(name = name, src = src, templates = templates, out = out)

    native.prebuilt_cxx_library(
        name = name + "-headers",
        header_dirs = [":{}[headers]".format(name)],
        header_namespace = "",
        header_only = True,
    )
