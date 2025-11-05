DeployableFirmwareAsset = provider(
    # @unsorted-dict-items
    fields = {
        "node": provider_field(str),
        "binary": provider_field(typing.Any),
    },
)

def _deployable_target(ctx: AnalysisContext) -> list[Provider]:
    di = ctx.attrs.src[DefaultInfo]
    if not di.default_outputs:
        fail("`src` dep must produce at least one output file")
    binary_art = di.default_outputs[0]

    return [
        DefaultInfo(default_output = binary_art),
        DeployableFirmwareAsset(
            node = ctx.attrs.target_node,
            binary = binary_art,
        ),
    ]

deployable_target = rule(
    impl = _deployable_target,
    attrs = {
        "target_node": attrs.string(),
        "src": attrs.dep(),
    },
)
