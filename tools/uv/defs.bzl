load("@prelude//:genrule.bzl", "genrule_attributes", "process_genrule")
load("@prelude//decls/genrule_common.bzl", "genrule_common")

UvTool = provider(
    # @unsorted-dict-items
    fields = {
        "srcs": provider_field(typing.Any),
        "venv": provider_field(typing.Any),
    },
)

def _uv_tool_impl(ctx: AnalysisContext) -> list[Provider]:
    return [
        DefaultInfo(),
        UvTool(srcs = ctx.attrs.srcs, venv = ctx.attrs.venv),
        TemplatePlaceholderInfo(
            unkeyed_variables = {
                "python": "source ${TOOLDIR}/.venv/bin/activate && uv run -v --no-sync --active",
            },
        ),
    ]

_uv_tool = rule(
    impl = _uv_tool_impl,
    attrs = {
        "srcs": attrs.list(attrs.source(allow_directory = True)),
        "venv": attrs.dep(),
    },
    is_toolchain_rule = True,
)

def uv_tool(
        name: str,
        pyproject_file: str,
        lock_file: str,
        srcs: list[str],
        deps: list[str] | None = None,
        visibility: list[str] = ["PRIVATE"]):
    deps = deps or []

    # bootstrap python env with uv based on pyproject file
    venv = ":{}_venv".format(name)
    native.genrule(
        name = venv[1:],
        srcs = [pyproject_file, lock_file],
        out = ".venv",
        cmd = "uv venv -v ${OUT} && source ${OUT}/bin/activate && uv sync -v --active --locked",
    )

    # call the rule defined above now that the environment is bootstrapped
    return _uv_tool(
        name = name,
        srcs = deps + srcs,
        venv = venv,
        visibility = visibility,
    )

def _uv_genrule_impl(ctx: AnalysisContext) -> list[Provider]:
    venv = ctx.attrs.tool[UvTool].venv.providers[0].default_outputs[0]
    symlinks = {
        venv.short_path: venv,
    } | {src.short_path: src for src in ctx.attrs.tool[UvTool].srcs}

    tooldir = ctx.actions.symlinked_dir("tool", symlinks)
    env = {"TOOLDIR": cmd_args(tooldir, format = "./{}")}

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
