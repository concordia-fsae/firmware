load("//drive-stack/defs.bzl", "DeployableFirmwareAsset", "DeployableNodeInfo", "deployable_target")

def _target_nodes(ctx):
    nodes = []
    seen = {}
    for node in ctx.attrs.nodes:
        if node not in seen:
            nodes.append(node)
            seen[node] = True
    for dep in ctx.attrs.targets:
        node = dep[DeployableNodeInfo].node
        if node not in seen:
            nodes.append(node)
            seen[node] = True
    return nodes

def _ota_agent(ctx: AnalysisContext):
    tool = ctx.attrs.tool[RunInfo]
    asset = ctx.attrs.src[DeployableFirmwareAsset]

    argv = cmd_args(tool)
    argv.add("client")
    argv.add("ota")
    argv.add(["-n", "{}".format(asset.node)])
    if ctx.attrs.platform != None:
        argv.add(["-p", ctx.attrs.platform])
    argv.add("-b")
    argv.add(cmd_args(asset.binary, format = "{}"))

    return [RunInfo(args = argv), DefaultInfo()]

ota_agent = rule(
    impl = _ota_agent,
    attrs = {
        "platform": attrs.option(attrs.string(), default = None),
        "src": attrs.dep(providers = [DeployableFirmwareAsset]),
        "tool": attrs.exec_dep(default = "//drive-stack/ota-agent:ota-agent"),
    },
)

def _ota_agent_bootstrap(ctx: AnalysisContext):
    tool = ctx.attrs.tool[RunInfo]
    asset = ctx.attrs.src[DeployableFirmwareAsset]

    argv = cmd_args(tool)
    argv.add("client")
    argv.add("bootstrap")
    argv.add(["-n", "{}".format(asset.node)])
    if ctx.attrs.platform != None:
        argv.add(["-p", ctx.attrs.platform])
    argv.add("-b")
    argv.add(cmd_args(asset.binary, format = "{}"))

    return [RunInfo(args = argv), DefaultInfo()]

ota_agent_bootstrap = rule(
    impl = _ota_agent_bootstrap,
    attrs = {
        "platform": attrs.option(attrs.string(), default = None),
        "src": attrs.dep(providers = [DeployableFirmwareAsset]),
        "tool": attrs.exec_dep(default = "//drive-stack/ota-agent:ota-agent"),
    },
)

def _ota_agent_batch(ctx: AnalysisContext):
    tool = ctx.attrs.tool[RunInfo]

    argv = cmd_args(tool)
    argv.add("client")
    argv.add("batch")
    if ctx.attrs.platform != None:
        argv.add(["-p", ctx.attrs.platform])

    for dep in ctx.attrs.srcs:
        asset = dep[DeployableFirmwareAsset]
        node_colon_path = cmd_args(asset.binary, format = asset.node + ":{}")
        argv.add(["-u", node_colon_path])

    return [RunInfo(args = argv), DefaultInfo()]

ota_agent_batch = rule(
    impl = _ota_agent_batch,
    attrs = {
        "platform": attrs.option(attrs.string(), default = None),
        "srcs": attrs.list(attrs.dep(providers = [DeployableFirmwareAsset])),
        "tool": attrs.exec_dep(default = "//drive-stack/ota-agent:ota-agent"),
    },
)

def _ota_agent_status(ctx: AnalysisContext):
    tool = ctx.attrs.tool[RunInfo]
    argv = cmd_args(tool)
    argv.add("client")
    argv.add("status")
    for node in _target_nodes(ctx):
        argv.add(["-n", node])
    return [RunInfo(args = argv), DefaultInfo()]

ota_agent_status = rule(
    impl = _ota_agent_status,
    attrs = {
        "nodes": attrs.list(attrs.string(), default = []),
        "targets": attrs.list(attrs.dep(providers = [DeployableNodeInfo]), default = []),
        "tool": attrs.exec_dep(default = "//drive-stack/ota-agent:ota-agent"),
    },
)

def _ota_agent_revert(ctx: AnalysisContext):
    tool = ctx.attrs.tool[RunInfo]
    argv = cmd_args(tool)
    argv.add("client")
    argv.add("revert")
    if ctx.attrs.all:
        argv.add("--all")
    for node in _target_nodes(ctx):
        argv.add(["-n", node])
    return [RunInfo(args = argv), DefaultInfo()]

ota_agent_revert = rule(
    impl = _ota_agent_revert,
    attrs = {
        "all": attrs.bool(default = False),
        "nodes": attrs.list(attrs.string(), default = []),
        "targets": attrs.list(attrs.dep(providers = [DeployableNodeInfo]), default = []),
        "tool": attrs.exec_dep(default = "//drive-stack/ota-agent:ota-agent"),
    },
)
