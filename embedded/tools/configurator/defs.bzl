# embedded/tools/configurator/defs.bzl

load("//tools/uv/defs.bzl", "uv_genrule")

def generate_adc_code(
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
    uv_genrule(
        name = uv_name,
        tool = "//embedded/tools/configurator:configurator_tool",
        srcs = [
            "//embedded/tools/configurator:configurator.py",
            "//embedded/tools/configurator:templates/drv_inputAD_componentSpecific.h.mako",
            "//embedded/tools/configurator:templates/drv_inputAD_componentSpecific.c.mako",
            adc_config,
        ],
        
        # I have no idea what's going on 
        cmd = (
            "echo \"=== Starting configurator ===\" && " +
            "mkdir -p workspace && " +
            "cp configurator.py workspace/ && " +
            "cp -r templates workspace/ && " +
            "cp $(location " + adc_config + ") workspace/ && " +
            "cd workspace && " +
            "echo \"=== Files in workspace ===\" && " +
            "find . -type f && " +
            "echo \"=== Running configurator ===\" && " +
            "$(python) configurator.py --input $(location " + adc_config + ") --output-dir ../${OUT} && " +
            "echo \"=== Checking output ===\" && " +
            "cd ../${OUT} && " +
            "echo \"Generated files:\" && " +
            "ls -la && " +
            "if [ ! -f \"drv_inputAD_componentSpecific.c\" ]; then " +
            "  echo \"ERROR: drv_inputAD_componentSpecific.c was not generated!\" && " +
            "  exit 1; " +
            "fi && " +
            "if [ ! -f \"drv_inputAD_componentSpecific.h\" ]; then " +
            "  echo \"ERROR: drv_inputAD_componentSpecific.h was not generated!\" && " +
            "  exit 1; " +
            "fi && " +
            "echo \"=== SUCCESS: Files generated ===\""
        ),
        outs = {
            "drv_inputAD_componentSpecific.h": ["drv_inputAD_componentSpecific.h"],
            "drv_inputAD_componentSpecific.c": ["drv_inputAD_componentSpecific.c"],
        },
    )