load("@prelude//:genrule.bzl", "genrule_attributes", "process_genrule")
load("@prelude//decls/genrule_common.bzl", "genrule_common")

UvTool = provider(
    # @unsorted-dict-items
    fields = {
        "srcs": provider_field(typing.Any),
        "venv": provider_field(typing.Any),
        "python": provider_field(typing.Any),
        "tooldir": provider_field(typing.Any),
    },
)

def _uv_tool_impl(ctx: AnalysisContext) -> list[Provider]:
    venv_artifact = ctx.actions.declare_output("out/.venv", dir = True)

    srcs = []
    for src in [ctx.attrs.pyproject_file, ctx.attrs.lock_file]:
        srcs.append(ctx.actions.symlink_file("out/{}".format(src.short_path), src))

    for src in ctx.attrs.srcs + ctx.attrs.deps:
        srcs.append(ctx.actions.symlink_file("srcs/{}".format(src.short_path), src))

    # write a script which will initialize the uv venv
    script = [
        cmd_args("set -x"),
        cmd_args(venv_artifact, format = "mkdir -p {}", parent = 1),
        cmd_args(venv_artifact, format = "cd {} || exit 99", parent = 1),
        cmd_args("uv venv -v --no-project .venv"),
        cmd_args("source .venv/bin/activate"),
        cmd_args("uv sync -v --active --locked"),
    ]
    sh_script = ctx.actions.write(
        "sh/create_venv.sh",
        script,
        is_executable = True,
    )
    script_args = ["/usr/bin/env", "bash", "-e", sh_script]

    # run the script we wrote above
    ctx.actions.run(
        cmd_args(
            script_args,
            hidden = [venv_artifact.as_output(), srcs],
        ),
        category = "uv",
    )

    # write a wrapper script which will activate the venv and then
    # run whatever args are provided under uv
    script = [
        cmd_args(venv_artifact, format = "source {}/bin/activate"),
        cmd_args(venv_artifact, format = "uv run --no-sync --active $@", parent = 1),
    ]
    sh_script, _ = ctx.actions.write("sh/wrapper.sh", script, is_executable = True, allow_args = True)
    script_args = ["/usr/bin/env", "bash", "-e", sh_script]

    # write _another_ wrapper script, this time for genrules. Otherwise, same concept as above
    genrule_script = [
        cmd_args("source $(dirname \"$0\")/../out/.venv/bin/activate"),
        cmd_args("uv run --no-sync --active $@"),
    ]
    genrule_sh_script, _ = ctx.actions.write("sh/genrule_wrapper.sh", genrule_script, is_executable = True, allow_args = True)
    genrule_script_args = ["/usr/bin/env", "bash", "-e", genrule_sh_script]

    return [
        DefaultInfo(default_outputs = [venv_artifact]),
        UvTool(
            srcs = ctx.attrs.srcs,
            venv = venv_artifact,
            python = RunInfo(cmd_args(script_args, hidden = [venv_artifact, srcs])),
            tooldir = cmd_args(venv_artifact, format = "{}/srcs", parent = 2),
        ),
        TemplatePlaceholderInfo(
            unkeyed_variables = {
                "python": cmd_args(genrule_script_args, hidden = [venv_artifact, srcs]),
            },
        ),
    ]

uv_tool = rule(
    impl = _uv_tool_impl,
    attrs = {
        "pyproject_file": attrs.source(),
        "lock_file": attrs.source(),
        "srcs": attrs.list(attrs.source()),
        "deps": attrs.list(attrs.dep(), default = []),
    },
    is_toolchain_rule = True,
)

def _uv_genrule_impl(ctx: AnalysisContext) -> list[Provider]:
    tool = ctx.attrs.tool[UvTool]

    env = {
        "TOOLDIR": cmd_args(
            tool.tooldir,
            hidden = [tool.venv, tool.srcs],
        ),
    }

    return process_genrule(ctx, ctx.attrs.out, ctx.attrs.outs, extra_env_vars = env)

uv_genrule = rule(
    impl = _uv_genrule_impl,
    attrs = (
        genrule_common.cmd_arg() |
        genrule_common.cmd_exe_arg() |
        genrule_common.env_arg() |
        genrule_common.environment_expansion_separator() |
        genrule_common.out_arg() |
        genrule_common.srcs_arg() |
        genrule_common.type_arg() |
        genrule_common.weight_arg() |
        genrule_attributes() |
        {
            "tool": attrs.toolchain_dep(providers = [UvTool]),
            "outs": attrs.option(
                attrs.dict(
                    key = attrs.string(),
                    value = attrs.set(attrs.string(), sorted = False),
                    sorted = False,
                ),
                default = None,
            ),
            "default_outs": attrs.option(attrs.list(attrs.string()), default = None),
            "bash": attrs.option(attrs.arg(), default = None),
            "cacheable": attrs.option(attrs.bool(), default = None),
            "executable_outs": attrs.option(
                attrs.set(attrs.string(), sorted = False),
                default = None,
                doc = """
                Only valid if the `outs` arg is present. Dictates which of those named outputs are marked as
                executable.
                """,
            ),
            "labels": attrs.list(attrs.string(), default = []),
        }
    ),
)

UvBinary = provider(fields = {
    "run": provider_field(typing.Any),
})

def _uv_binary_impl(ctx: AnalysisContext) -> list[Provider]:
    tool = ctx.attrs.tool[UvTool]

    # Program args: run python inside uv venv, then script
    # Use the genrule wrapper (it activates venv then `uv run ...`)
    argv = cmd_args(
        tool.python,  # this is already a RunInfo(cmd_args(...)) in your UvTool
        cmd_args(tool.tooldir, format = "{}/{}".format("{}", ctx.attrs.entrypoint)),
        ctx.attrs.args,
        hidden = [tool.venv, tool.srcs],
    )

    return [
        DefaultInfo(),
        RunInfo(argv),
        UvBinary(run = argv),
    ]

uv_binary = rule(
    impl = _uv_binary_impl,
    attrs = {
        "tool": attrs.toolchain_dep(providers = [UvTool]),
        # path relative to tool.tooldir (which points at .../srcs)
        "entrypoint": attrs.string(),
        "args": attrs.list(attrs.string(), default = []),
    },
)
