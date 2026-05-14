DeployableFirmwareAsset = provider(
    # @unsorted-dict-items
    fields = {
        "node": provider_field(str),
        "binary": provider_field(typing.Any),
        "filename": provider_field(str),
        "sha256_file": provider_field(typing.Any),
    },
)

DeployableNodeInfo = provider(
    fields = {
        "node": provider_field(str),
    },
)

def _deployable_target(ctx: AnalysisContext) -> list[Provider]:
    di = ctx.attrs.src[DefaultInfo]
    if not di.default_outputs:
        fail("`src` dep must produce at least one output file")
    binary_art = di.default_outputs[0]

    filename = binary_art.short_path.split("/")[-1]
    sha256 = ctx.actions.declare_output("{}.sha256".format(ctx.attrs.target_node))
    script = ctx.actions.write(
        "hash-artifact.sh",
        [
            "set -eu",
            'sha256sum "$1" | awk \'{print $1}\' > "$2"',
        ],
        is_executable = True,
    )
    ctx.actions.run(
        cmd_args(
            ["/usr/bin/env", "bash", "-e", script, binary_art, sha256.as_output()],
            hidden = [binary_art],
        ),
        category = "deployable_target_hash",
    )

    return [
        DefaultInfo(default_output = binary_art),
        DeployableNodeInfo(node = ctx.attrs.target_node),
        DeployableFirmwareAsset(
            node = ctx.attrs.target_node,
            binary = binary_art,
            filename = filename,
            sha256_file = sha256,
        ),
    ]

deployable_target = rule(
    impl = _deployable_target,
    attrs = {
        "target_node": attrs.string(),
        "src": attrs.dep(),
    },
)
