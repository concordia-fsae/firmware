load("//tools/uv/defs.bzl", "UvTool", "uv_genrule")

def generate_adc_config(
        name: str,
        adc_config: str,
        component: str | None = None,
        **kwargs):
    """
    Generates ADC channel code from YAML config using configurator.py
    """

    uv_name = name + "-codegen"

    # genrule, uv_rule, uv_genrule I dont know what the difference is or which to use
    # I've tried all of them but keep getting build fails RIP
    # Keep it up tiger
    uv_genrule(
        name = uv_name,
        tool = "//embedded/tools/configurator:configurator_tool",
        srcs = [
            "//embedded/tools/configurator:templates/drv_inputAD_config.h.mako",
            "//embedded/tools/configurator:templates/drv_inputAD_config.c.mako",
            adc_config,
        ],
        cmd = "$(python) ${TOOLDIR}/configurator.py --input " + adc_config + " --output-dir ${OUT}",
        outs = {
            "drv_inputAD_config.h": ["drv_inputAD_config.h"],
            "drv_inputAD_config.c": ["drv_inputAD_config.c"],
        },
    )
    native.cxx_library(
        name = name,
        srcs = [
            ":{}[drv_inputAD_config.c]".format(uv_name),
        ],
        exported_headers = {
            "drv_inputAD_config.h": ":{}[drv_inputAD_config.h]".format(uv_name),
        },
        header_namespace = "",
        visibility = ["PUBLIC"],
    )

def _run_configurator_impl(ctx: AnalysisContext) -> list[Provider]:
    tool = ctx.attrs.tool[UvTool]

    out = ctx.actions.declare_output(ctx.attrs.out, dir = True)

    src = ctx.attrs.src
    src = ctx.actions.symlink_file("srcs/" + src.short_path, src)

    pprint((ctx.attrs.templates[0].basename))
    templates = {template.short_path: template for template in ctx.attrs.templates}
    templates_dir = ctx.actions.symlinked_dir("templates", templates)

    ctx.actions.run(
        cmd_args([
            tool.python,
            cmd_args(tool.tooldir, format = "{}/configurator.py"),
            "--input",
            cmd_args(src),
            "--output-dir",
            cmd_args(out.as_output()),
            "--template-dir",
            cmd_args(templates_dir),
        ]),
        category = "configurator",
    )

    return [DefaultInfo(default_outputs = [out])]

run_configurator = rule(
    impl = _run_configurator_impl,
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
