DeployableAsset = provider(
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
        DeployableAsset(
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

def _conUDS_batch(ctx: AnalysisContext):
    tool = ctx.attrs.tool[RunInfo]

    man_di = ctx.attrs.manifest[DefaultInfo]
    if not man_di.default_outputs:
        fail("manifest dep must produce at least one output file")
    manifest_art = man_di.default_outputs[0]

    argv = cmd_args(tool)
    argv.add(["-m", manifest_art])
    argv.add("batch")

    for dep in ctx.attrs.srcs:
        asset = dep[DeployableAsset]
        node_colon_path = cmd_args(asset.binary, format = asset.node + ":{}")
        argv.add(["-u", node_colon_path])

    return [RunInfo(args = argv), DefaultInfo()]

conUDS_batch = rule(
    impl = _conUDS_batch,
    attrs = {
        "srcs": attrs.list(attrs.dep(providers = [DeployableAsset])),
        "manifest": attrs.dep(providers = [DefaultInfo]),
        "tool": attrs.exec_dep(default = "//drive-stack/conUDS:conUDS"),
    },
)

def conUDS_download(
        name: str,
        binary: str,
        node: str,
        manifest: str):
    return native.command_alias(
        name = name,
        exe = "//drive-stack/conUDS:conUDS",
        args = ["-m", "$(location {})".format(manifest), "-n", node, "download", "$(location {})".format(binary)],
    )

def conUDS_bootloader_download(
        name: str,
        binary: str,
        node: str,
        manifest: str):
    return native.command_alias(
        name = name,
        exe = "//drive-stack/conUDS:conUDS",
        args = ["-m", "$(location {})".format(manifest), "-n", node, "bootloader-download", "$(location {})".format(binary)],
    )
