load("//tools/uv/defs.bzl", "uv_genrule")

def generate_adc_config(
        name,
        adc_config,
        component = None,
        **kwargs):

    uv_name = name + "-codegen"

    uv_genrule(
        name = uv_name,
        tool = "//embedded/tools/configurator:configurator_tool",
        srcs = [adc_config],
        cmd = (
            "$(python) ${TOOLDIR}/configurator.py"
            + " --input ${SRCS} "
            + " --output-dir ${OUT} "
            + " --template-dir ${TOOLDIR} "
            + " && ls -R ${OUT}"
        ),
        outs = {
            "drv_inputAD_config.h": ["drv_inputAD_config.h"],
            "drv_inputAD_config.c": ["drv_inputAD_config.c"],
        },
    )

    base_deps = ["//components/shared/code:headers"]
    
    native.cxx_library(
        name = name,
        srcs = [
            ":{}[drv_inputAD_config.c]".format(uv_name),
        ],
        exported_headers = {
            "drv_inputAD_config.h": ":{}[drv_inputAD_config.h]".format(uv_name),
        },
        header_namespace = "",
        deps = base_deps,
        visibility = ["PUBLIC"],
    )
