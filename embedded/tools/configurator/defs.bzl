# embedded/tools/configurator/defs.bzl

load("//tools/uv/defs.bzl", "uv_genrule")

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
