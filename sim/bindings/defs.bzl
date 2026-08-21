load("@prelude//:rules.bzl", __rules__ = "rules")
load("//components/vehicle_platform:platforms.bzl", "platform_output_name", "platform_target_label")
load("//tools/rig:defs.bzl", "RIG_RUST_RUNTIME_ENV", "rig_model_python")

RIG_RUNTIME_ENV = "RIG_RUNTIME_RS"
RIG_RUNTIME_SRC = "//sim/bindings/firmware/runtime:runtime-src"
RIG_RUNTIME_RUST_ENV = RIG_RUST_RUNTIME_ENV | {
    "RIG_RUNTIME_RUST_CAN_RS": "//sim/bindings/firmware/can:can.rs",
    "RIG_RUNTIME_RUST_CLUSTER_RS": "//sim/bindings/firmware/runtime:cluster.rs",
    "RIG_RUNTIME_RUST_FAULTS_RS": "//sim/bindings/firmware/faults:faults.rs",
    "RIG_RUNTIME_RUST_INTERFACES_RS": "//tools/rig:rust/interfaces.rs",
    "RIG_RUNTIME_RUST_MODULES_RS": "//sim/bindings/firmware/runtime:runtime_common.rs",
    "RIG_RUNTIME_RUST_IO_RS": "//sim/bindings/firmware/io:io.rs",
    "RIG_RUNTIME_RUST_IO_HOST_RS": "//sim/bindings/firmware/io:host.rs",
    "RIG_RUNTIME_RUST_NVM_RS": "//sim/bindings/firmware/nvm:nvm.rs",
    "RIG_RUNTIME_RUST_REGISTRY_RS": "//sim/bindings/firmware/runtime:registry.rs",
    "RIG_RUNTIME_RUST_RT_CONTROLLER_RS": "//sim/bindings/firmware/runtime:rt_controller.rs",
    "RIG_RUNTIME_RUST_SPI_RS": "//sim/bindings/firmware/spi:spi.rs",
    "RIG_RUNTIME_RUST_TIMER_RS": "//sim/bindings/firmware/timer:timer.rs",
}

_DEFAULT_MODEL_C_FLAGS = [
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-include",
    "BuildDefines.h",
    "-include",
    "Utility.h",
    "-include",
    "string.h",
    "-Wno-unused-parameter",
]

_SHORT_ENUMS_FLAG = "-fshort-enums"

_DEFAULT_FEATURE_CONST_ALLOWLIST = [
    "APP_COMPONENT_ID",
    "APP_VARIANT_ID",
    "NVM_BLOCK_SIZE",
    "NVM_FLASH_BACKED",
    "NVM_LIB_ENABLED",
]

_DEFAULT_BINDGEN_FLAGS = [
    "--no-layout-tests",
    "--default-enum-style rust",
]

_DEFAULT_BINDGEN_FUNCTION_ALLOWLIST = [
    "Module_Init",
    "Module_1kHz_TSK",
    "Module_100Hz_TSK",
    "Module_10Hz_TSK",
    "Module_1Hz_TSK",
    "drv_outputAD_toggleDigitalState",
    "YAMCAN_shared_init_static",
]

_DEFAULT_BINDGEN_VAR_ALLOWLIST = [
    "g_yamcan",
]

_DEFAULT_CONTROLLER_DEPS = [
    "//components/shared/code:headers",
    "//components/shared/code/RTOS:headers",
    "//sim/bindings/firmware/runtime:headers",
    "//sim/bindings/firmware/can:headers",
    "//sim/bindings/firmware/faults:headers",
    "//sim/bindings/firmware/flash:headers",
    "//sim/bindings/firmware/i2c:headers",
    "//sim/bindings/firmware/io:headers",
    "//sim/bindings/firmware/spi:headers",
    "//sim/bindings/firmware/timer:headers",
    "//sim/bindings/firmware/uart:headers",
]

_RIG_RUNTIME_MODULE_SRCS = {
    "can": ["//sim/bindings/firmware/can:can.c"],
    "runtime": ["//sim/bindings/firmware/runtime:runtime.c"],
    "flash": ["//sim/bindings/firmware/flash:flash.c"],
    "faults": ["//sim/bindings/firmware/faults:faults.c"],
    "i2c": ["//sim/bindings/firmware/i2c:i2c.c"],
    "io": ["//sim/bindings/firmware/io:io.c"],
    "spi": ["//sim/bindings/firmware/spi:spi.c"],
    "swi": ["//sim/bindings/firmware/runtime:swi.c"],
    "system": ["//sim/bindings/firmware/runtime:system.c"],
    "time": ["//sim/bindings/firmware/runtime:time.c"],
    "timer": ["//sim/bindings/firmware/timer:timer.c"],
    "timer_capture": ["//sim/bindings/firmware/timer:timer_capture.c"],
    "uart": ["//sim/bindings/firmware/uart:uart.c"],
}

def rig_runtime_srcs(modules: list[str]):
    srcs = []
    for module in modules:
        if module not in _RIG_RUNTIME_MODULE_SRCS:
            fail("unknown rig runtime module '{}'; expected one of {}".format(module, _RIG_RUNTIME_MODULE_SRCS.keys()))
        srcs += _RIG_RUNTIME_MODULE_SRCS[module]
    return srcs

def rig_platforms_from_variants(platform_variants):
    return [
        platform
        for platform, _variant in platform_variants
    ]

def rig_model_c_support(
        name: str = "model-c-support",
        srcs: list[str] = [],
        deps: list[str] = [],
        compiler_flags: list[str] | None = None,
        visibility: list[str] | None = None,
        **kwargs):
    __rules__["cxx_library"](
        name = name,
        srcs = srcs,
        compiler_flags = compiler_flags if compiler_flags else _DEFAULT_MODEL_C_FLAGS,
        header_namespace = "",
        deps = deps,
        preferred_linkage = "static",
        visibility = visibility,
        **kwargs
    )

def _rust_link_args(link_deps: list[str], extra_link_args: list[str]):
    link_args = ["-C link-arg=-Wl,--start-group"]
    link_args += [
        "-C link-arg=$(location {})".format(dep)
        for dep in link_deps
    ]
    link_args += ["-C link-arg=-Wl,--end-group"]
    link_args += [
        "-C link-arg={}".format(arg)
        for arg in extra_link_args
    ]
    return " ".join(link_args)

def _rust_genrule_env(rust_env: dict[str, str]):
    return " ".join([
        "{}=$PWD/$(location {})".format(name, target)
        for name, target in rust_env.items()
    ])

def _rust_test_env(rust_env: dict[str, str]):
    return {
        name: "$(location {})".format(target)
        for name, target in rust_env.items()
    }

def _bindgen_allowlist_args(
        allowlist_types: list[str],
        allowlist_functions: list[str],
        allowlist_vars: list[str]):
    return " ".join(
        ["--allowlist-type {}".format(item) for item in allowlist_types] +
        ["--allowlist-function {}".format(item) for item in allowlist_functions] +
        ["--allowlist-var {}".format(item) for item in allowlist_vars]
    )

def _bindgen_header_lines(include_headers: list[str]):
    return " ".join([
        "printf '#include \"%s\"\\n' \"{}\" >> $WRAPPER;".format(header)
        for header in include_headers
    ])

def rig_bindgen(
        name: str,
        out: str,
        include_headers: list[str],
        deps: list[str] = [],
        headers: dict[str, str] = {},
        allowlist_types: list[str] = [],
        allowlist_functions: list[str] = [],
        allowlist_vars: list[str] = [],
        bindgen_flags: list[str] = [],
        clang_flags: list[str] = [],
        visibility: list[str] | None = None):
    context_name = name + "-c-context"

    __rules__["cxx_library"](
        name = context_name,
        header_namespace = "",
        headers = headers,
        exported_deps = deps,
    )

    clang_arg_string = " ".join(clang_flags)
    __rules__["genrule"](
        name = name,
        out = out,
        cmd = "WRAPPER=$TMP/bindgen-wrapper.h; " +
              ": > $WRAPPER; " +
              _bindgen_header_lines(include_headers) +
              "bindgen $WRAPPER " +
              _bindgen_allowlist_args(allowlist_types, allowlist_functions, allowlist_vars) +
              " " +
              " ".join(bindgen_flags) +
              " -- " +
              "$(cppflags :{}) ".format(context_name) +
              clang_arg_string +
              " > $OUT; " +
              "sed -i 's/^extern \"C\"/unsafe extern \"C\"/' $OUT",
        visibility = visibility,
    )

def rig_feature_consts(
        name: str = "features-rs",
        out: str = "features.rs",
        deps: list[str] = [],
        allowlist_vars: list[str] = [],
        bindgen_flags: list[str] = [],
        clang_flags: list[str] = [],
        visibility: list[str] | None = None):
    rig_bindgen(
        name = name,
        out = out,
        include_headers = [
            "FeatureDefines_generated.h",
        ],
        deps = deps,
        allowlist_vars = allowlist_vars,
        bindgen_flags = [
            "--no-layout-tests",
        ] + bindgen_flags,
        clang_flags = clang_flags,
        visibility = visibility,
    )

def rig_python_enums(
        name: str,
        out: str,
        include_headers: list[str],
        deps: list[str] = [],
        headers: dict[str, str] = {},
        c_enums: list[str] = [],
        c_enums_auto: bool = False,
        rust_sources: list[str] = [],
        rust_enums: list[str] = [],
        clang_flags: list[str] = [],
        visibility: list[str] | None = None):
    context_name = name + "-c-context"

    __rules__["cxx_library"](
        name = context_name,
        header_namespace = "",
        headers = headers,
        exported_deps = deps,
    )

    __rules__["genrule"](
        name = name,
        out = out,
        cmd = "PROJECT=$PWD/$(location //tools/rig:uv-project); " +
              "WRAPPER=$TMP/python-enums-wrapper.h; " +
              ": > $WRAPPER; " +
              _bindgen_header_lines(include_headers) +
              "UV_INDEX_URL=https://pypi.org/simple " +
              "UV_DEFAULT_INDEX=https://pypi.org/simple " +
              "PIP_INDEX_URL=https://pypi.org/simple " +
              "UV_PROJECT_ENVIRONMENT=$PWD/.uv-env " +
              # Buck supplies an immutable generated project.  `--frozen`
              # consumes its checked-in lock without trying to rewrite it
              # while several code-generation actions run in parallel.
              "uv run --frozen --project $PROJECT python $(location //tools/rig:gen-python-enums) " +
              "--c-wrapper $WRAPPER " +
              " ".join(["--c-enum '{}'".format(item) for item in c_enums]) +
              (" --c-enums-auto-prefix CAN_ --c-enums-auto-suffix _E" if c_enums_auto else "") +
              " " +
              " ".join(["--rust-source $(location {})".format(item) for item in rust_sources]) +
              " " +
              " ".join(["--rust-enum '{}'".format(item) for item in rust_enums]) +
              " " +
              "--out $OUT -- " +
              "$(cppflags :{}) ".format(context_name) +
              " ".join(clang_flags),
        visibility = visibility,
    )

def rig_python_model(
        name: str,
        out: str,
        rust_source: str,
        class_name: str,
        symbol_prefix: str,
        buck_target: str,
        env_var: str,
        enum_env_var: str,
        enum_target: str,
        visibility: list[str] | None = None):
    __rules__["genrule"](
        name = name,
        out = out,
        cmd = "python3 $(location //sim/bindings/firmware/runtime:gen-python-model) " +
              "--rust-source $(location {}) ".format(rust_source) +
              "--out $OUT " +
              "--class-name '{}' ".format(class_name) +
              "--symbol-prefix '{}' ".format(symbol_prefix) +
              "--buck-target '{}' ".format(buck_target) +
              "--env-var '{}' ".format(env_var) +
              "--enum-env-var '{}' ".format(enum_env_var) +
              "--enum-target '{}'".format(enum_target),
        visibility = visibility,
    )

def rig_embedded_rust_model(
        name: str = "sil-so",
        crate: str = "",
        crate_root: str = "src/lib.rs",
        out: str = "",
        link_deps: list[str] = [],
        test_name: str = "test",
        test_deps: list[str] | None = None,
        src_target_name: str = "rust-wrapper-src",
        extra_link_args: list[str] | None = None,
        rust_env: dict[str, str] = {},
        visibility: list[str] | None = None,
        test_visibility: list[str] | None = None):
    __rules__["export_file"](
        name = src_target_name,
        src = crate_root,
    )

    __rules__["genrule"](
        name = name,
        out = out if out else "lib{}.so".format(crate),
        cmd = _rust_genrule_env({RIG_RUNTIME_ENV: RIG_RUNTIME_SRC} | RIG_RUNTIME_RUST_ENV | rust_env) +
              " " +
              "rustc --edition=2024 --crate-name {} --crate-type cdylib ".format(crate) +
              "$(location :{}) ".format(src_target_name) +
              "-C opt-level=3 " +
              "-o $OUT " +
              _rust_link_args(link_deps, extra_link_args if extra_link_args else ["-lm"]),
        visibility = visibility,
    )

    __rules__["rust_test"](
        name = test_name,
        crate = crate,
        crate_root = crate_root,
        edition = "2024",
        srcs = [crate_root],
        env = _rust_test_env({RIG_RUNTIME_ENV: RIG_RUNTIME_SRC} | RIG_RUNTIME_RUST_ENV | rust_env),
        deps = test_deps if test_deps != None else link_deps,
        visibility = test_visibility if test_visibility != None else visibility,
    )

def rig_embedded_rust_model_platform_aliases(
        name: str,
        actual: str,
        platforms,
        visibility: list[str] | None = None):
    [
        native.configured_alias(
            name = "{}-{}".format(name, platform_output_name(platform)),
            actual = actual,
            platform = platform_target_label(platform),
            visibility = visibility,
        )
        for platform in platforms
    ]

def _with_short_enums(items: list[str], short_enums: bool):
    return items + ([_SHORT_ENUMS_FLAG] if short_enums else [])

def _controller_component_target(component: str, target: str):
    return "{}:{}".format(component, target)

def _controller_codegen_artifact(component: str, artifact: str):
    return "{}:sil-yamcan[{}]".format(component, artifact)

def _controller_deps(component: str, extra_deps: list[str]):
    return _DEFAULT_CONTROLLER_DEPS + [
        _controller_component_target(component, "sil-features"),
        _controller_component_target(component, "sil-host-headers"),
        _controller_component_target(component, "sil-yamcan-c"),
    ] + extra_deps

def rig_embedded_controller_model(
        controller: str,
        class_name: str,
        component: str,
        platform_variants,
        python_srcs: list[str],
        runtime_modules: list[str],
        bindgen_headers: list[str],
        bindgen_types: list[str],
        enum_headers: list[str],
        c_enums: list[str],
        model_target: str,
        extra_deps: list[str] = [],
        bindgen_functions: list[str] | None = None,
        bindgen_vars: list[str] | None = None,
        enum_rust_enums: list[str] = [],
        model_rust_modules: dict[str, str] = {},
        short_enums: bool = False,
        rust_env_prefix: str | None = None,
        visibility: list[str] | None = ["PUBLIC"]):
    controller_upper = rust_env_prefix if rust_env_prefix else controller.upper()
    controller_lower = controller.lower()
    model_platforms = rig_platforms_from_variants(platform_variants)
    common_deps = _controller_deps(component, extra_deps)
    clang_flags = _with_short_enums([
        "-include",
        "BuildDefines.h",
    ], short_enums)

    model_rust_env = {}
    if model_rust_modules:
        model_module_lines = [": > $OUT;"]
        for module, target in model_rust_modules.items():
            env_name = "{}_MODEL_{}_RS".format(controller_upper, module.upper())
            model_rust_env[env_name] = target
            model_module_lines += [
                "printf '%s\\n' 'pub mod {} {{' >> $OUT;".format(module),
                "printf '%s\\n' '    include!(env!(\"{}\"));' >> $OUT;".format(env_name),
                "printf '%s\\n' '}' >> $OUT;",
            ]
        __rules__["genrule"](
            name = "model-modules-rs",
            out = "model_modules.rs",
            cmd = " ".join(model_module_lines),
        )
        model_rust_env["{}_MODEL_MODULES_RS".format(controller_upper)] = ":model-modules-rs"

    rig_python_model(
        name = "{}-py".format(controller_lower),
        out = "{}.py".format(controller_lower),
        rust_source = ":rust-wrapper-src",
        class_name = class_name,
        symbol_prefix = "{}_sim".format(controller_lower),
        buck_target = "{}:sil-so".format(model_target),
        env_var = "{}_SIM_LIB".format(controller_upper),
        enum_env_var = "{}_ENUMS_PY".format(controller_upper),
        enum_target = "{}:enums-py".format(model_target),
        visibility = visibility,
    )

    rig_model_python(
        srcs = python_srcs + [
            ":{}-py".format(controller_lower),
            ":enums-py",
        ],
        visibility = visibility,
    )

    rig_model_c_support(
        srcs = rig_runtime_srcs(runtime_modules),
        compiler_flags = _with_short_enums(_DEFAULT_MODEL_C_FLAGS, short_enums),
        headers = {
            "runtime_state.h": "//sim/bindings/firmware/runtime:runtime_state.h",
        },
        deps = common_deps,
    )

    rig_feature_consts(
        name = "features-rs",
        out = "features.rs",
        deps = [
            _controller_component_target(component, "sil-features"),
        ],
        allowlist_vars = _DEFAULT_FEATURE_CONST_ALLOWLIST,
    )

    rig_bindgen(
        name = "bindings-rs",
        out = "bindings.rs",
        include_headers = bindgen_headers,
        deps = [
            ":model-c-support",
        ] + common_deps,
        allowlist_types = bindgen_types,
        allowlist_functions = bindgen_functions if bindgen_functions != None else _DEFAULT_BINDGEN_FUNCTION_ALLOWLIST,
        allowlist_vars = bindgen_vars if bindgen_vars != None else _DEFAULT_BINDGEN_VAR_ALLOWLIST,
        bindgen_flags = _DEFAULT_BINDGEN_FLAGS,
        clang_flags = clang_flags,
    )

    rig_python_enums(
        name = "enums-py",
        out = "enums.py",
        include_headers = enum_headers,
        deps = [
            ":model-c-support",
        ] + common_deps,
        c_enums = c_enums,
        c_enums_auto = True,
        rust_sources = [_controller_codegen_artifact(component, "rust_faults_generated.rs")] if enum_rust_enums else [],
        rust_enums = enum_rust_enums,
        clang_flags = clang_flags,
        visibility = visibility,
    )

    rig_embedded_rust_model(
        name = "sil-so",
        crate = "{}_sim".format(controller_lower),
        out = "lib{}_sim.so".format(controller_lower),
        link_deps = [
            _controller_component_target(component, "sil-application"),
            ":model-c-support",
            _controller_component_target(component, "sil-yamcan-c"),
        ],
        rust_env = {
            "{}_BINDINGS_RS".format(controller_upper): ":bindings-rs",
            "{}_FEATURES_RS".format(controller_upper): ":features-rs",
            "{}_YAMCAN_DECODE_RS".format(controller_upper): _controller_codegen_artifact(component, "rust_decode_generated.rs"),
            "{}_YAMCAN_MODEL_RS".format(controller_upper): _controller_codegen_artifact(component, "rust_model_generated.rs"),
            "{}_YAMCAN_RS".format(controller_upper): _controller_codegen_artifact(component, "yamcan.rs"),
        } | model_rust_env,
        visibility = visibility,
    )

    rig_embedded_rust_model_platform_aliases(
        name = "sil-so",
        actual = ":sil-so",
        platforms = model_platforms,
        visibility = visibility,
    )

def rig_platform_sim_lib_env(
        env_prefix: str,
        model_target: str,
        platforms) -> dict[str, str]:
    return {
        "{}_{}_SIM_LIB".format(env_prefix, platform_output_name(platform).upper()): "$(location {}:sil-so-{})".format(model_target, platform_output_name(platform))
        for platform in platforms
    }

def rig_platform_sim_lib_resources(
        model_target: str,
        platforms) -> list[str]:
    return [
        "{}:sil-so-{}".format(model_target, platform_output_name(platform))
        for platform in platforms
    ]

def rig_platform_node_sim_lib_env(
        env_prefix: str,
        model_target: str,
        platform_nodes) -> dict[str, str]:
    return {
        "{}{}_{}_SIM_LIB".format(
            env_prefix,
            node,
            platform_output_name(platform).upper(),
        ): "$(location {}:sil-so-{}-node-{})".format(
            model_target,
            platform_output_name(platform),
            node,
        )
        for platform, node in platform_nodes
    }

def rig_platform_node_sim_lib_resources(
        model_target: str,
        platform_nodes) -> list[str]:
    return [
        "{}:sil-so-{}-node-{}".format(
            model_target,
            platform_output_name(platform),
            node,
        )
        for platform, node in platform_nodes
    ]

def rig_platform_variants_env(platforms) -> dict[str, str]:
    return {
        "SIM_PLATFORM_VARIANTS": ",".join([
            platform_output_name(platform)
            for platform in platforms
        ]),
    }

def rig_pytest(
        name: str,
        test_file: str,
        env: dict[str, str] = {},
        resources: list[str] = [],
        debug: bool = False,
        visibility: list[str] | None = None):
    uv_runner_name = name + "-uv-runner"

    __rules__["genrule"](
        name = uv_runner_name,
        out = "uv-runner",
        cmd = "printf '%s\n' '#!/usr/bin/env bash' 'project=$1' 'shift' 'exec uv run --frozen --project \"$project\" \"$@\"' > $OUT && chmod +x $OUT",
        executable = True,
    )

    __rules__["sh_test"](
        name = name,
        args = [
            "$(location //tools/rig:uv-project)",
            "python",
            "-m",
            "pytest",
            "-p",
            "no:cacheprovider",
            "--color=yes",
            "-vv",
        ] +
        (["--durations=10"] if debug else []) +
        [
            test_file,
        ],
        env = {
            "PYTHONDONTWRITEBYTECODE": "1",
            "PYTHONPATH": "tools/rig/python:.",
            "RIG_RUNTIME_LIB": "$(location //sim/bindings/firmware/runtime:runtime-so)",
            "PIP_INDEX_URL": "https://pypi.org/simple",
            "UV_DEFAULT_INDEX": "https://pypi.org/simple",
            "UV_INDEX_URL": "https://pypi.org/simple",
            # Each Buck test target gets its own uv environment. Sharing one
            # environment makes parallel tests race while uv installs the
            # editable Rig package and can surface spurious lockfile errors.
            "UV_PROJECT_ENVIRONMENT": "/tmp/firmware-sim-rig-{}-venv".format(name),
        } | env,
        resources = [
            "//tools/rig:uv-project",
            "//sim/bindings/firmware/runtime:runtime-so",
        ] + resources,
        test = ":" + uv_runner_name,
        visibility = visibility,
    )
