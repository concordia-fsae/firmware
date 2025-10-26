load("//drive-stack/defs.bzl", "deployable_target", "DeployableFirmwareAsset")

def _ota_agent(ctx: AnalysisContext):
    tool = ctx.attrs.tool[RunInfo]
    asset = ctx.attrs.src[DeployableFirmwareAsset]

    argv = cmd_args(tool)
    argv.add("client")
    argv.add("ota")
    argv.add(["-n", "{}".format(asset.node)])
    argv.add("-b")
    argv.add(cmd_args(asset.binary, format = "{}"))

    return [RunInfo(args = argv), DefaultInfo()]

ota_agent = rule(
    impl = _ota_agent,
    attrs = {
        "src": attrs.dep(providers = [DeployableFirmwareAsset]),
        "tool": attrs.exec_dep(default = "//drive-stack/ota-agent:ota-agent"),
    },
)

def _ota_agent_batch(ctx: AnalysisContext):
    tool = ctx.attrs.tool[RunInfo]

    argv = cmd_args(tool)
    argv.add("client")
    argv.add("batch")

    for dep in ctx.attrs.srcs:
        asset = dep[DeployableFirmwareAsset]
        node_colon_path = cmd_args(asset.binary, format = asset.node + ":{}")
        argv.add(["-u", node_colon_path])

    return [RunInfo(args = argv), DefaultInfo()]

ota_agent_batch = rule(
    impl = _ota_agent_batch,
    attrs = {
        "srcs": attrs.list(attrs.dep(providers = [DeployableFirmwareAsset])),
        "tool": attrs.exec_dep(default = "//drive-stack/ota-agent:ota-agent"),
    },
)
