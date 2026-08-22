load("@prelude//:rules.bzl", __rules__ = "rules")


# Generic Rig Rust source modules. The standalone core owns the independent
# primitives; a consuming backend supplies runtime/module contracts when
# assembling the dataflow scheduler and node integration modules.
RIG_RUST_RUNTIME_ENV = {
    "RIG_RUNTIME_RUST_ALGORITHMS_RS": "//tools/rig:rust/algorithms.rs",
    "RIG_RUNTIME_RUST_DATAPATH_RS": "//tools/rig:rust/datapath.rs",
    "RIG_RUNTIME_RUST_DATAFLOW_RS": "//tools/rig:rust/dataflow.rs",
    "RIG_RUNTIME_RUST_MODEL_RS": "//tools/rig:rust/model.rs",
    "RIG_RUNTIME_RUST_MODEL_ABI_RS": "//tools/rig:rust/model_abi.rs",
    "RIG_RUNTIME_RUST_NODE_RS": "//tools/rig:rust/node.rs",
    "RIG_RUNTIME_RUST_NODE_ABI_RS": "//tools/rig:rust/node_abi.rs",
    "RIG_RUNTIME_RUST_RIG_RS": "//tools/rig:rust/rig.rs",
    "RIG_RUNTIME_RUST_RUNTIME_RS": "//tools/rig:rust/runtime.rs",
    "RIG_RUNTIME_RUST_SCHEDULER_RS": "//tools/rig:rust/scheduler.rs",
    "RIG_RUNTIME_RUST_SCALAR_RS": "//tools/rig:rust/scalar.rs",
    "RIG_RUNTIME_RUST_SCALAR_SOURCE_RS": "//tools/rig:rust/scalar_source.rs",
}


def rig_model_python(
        name: str = "python",
        srcs: list[str] = [],
        visibility: list[str] | None = None):
    """Declare Python sources for a model without importing firmware bindings."""
    __rules__["filegroup"](
        name = name,
        srcs = srcs,
        visibility = visibility,
    )


def rig_pytest(
        name: str,
        test_file: str,
        visibility: list[str] | None = None):
    """Run a standalone Rig Python test without importing firmware bindings."""
    uv_runner_name = name + "-uv-runner"

    __rules__["genrule"](
        name = uv_runner_name,
        out = "uv-runner",
        cmd = "printf '%s\\n' '#!/usr/bin/env bash' 'project=$1' 'shift' 'exec uv run --frozen --project \"$project\" \"$@\"' > $OUT && chmod +x $OUT",
        executable = True,
    )

    __rules__["sh_test"](
        name = name,
        args = [
            "$(location :uv-project)",
            "python",
            "-m",
            "pytest",
            "-p",
            "no:cacheprovider",
            "--color=yes",
            "-vv",
            test_file,
        ],
        env = {
            "PYTHONDONTWRITEBYTECODE": "1",
            "PYTHONPATH": "tools/rig/python:.",
            "PIP_INDEX_URL": "https://pypi.org/simple",
            "UV_DEFAULT_INDEX": "https://pypi.org/simple",
            "UV_INDEX_URL": "https://pypi.org/simple",
            "UV_PROJECT_ENVIRONMENT": "/tmp/rig-{}-venv".format(name),
        },
        resources = ["//tools/rig:uv-project"],
        test = ":" + uv_runner_name,
        visibility = visibility,
    )
