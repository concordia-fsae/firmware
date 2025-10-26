load("//drive-stack/defs.bzl", "deployable_target", "DeployableFirmwareAsset")

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
        asset = dep[DeployableFirmwareAsset]
        node_colon_path = cmd_args(asset.binary, format = asset.node + ":{}")
        argv.add(["-u", node_colon_path])

    return [RunInfo(args = argv), DefaultInfo()]

conUDS_batch = rule(
    impl = _conUDS_batch,
    attrs = {
        "srcs": attrs.list(attrs.dep(providers = [DeployableFirmwareAsset])),
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
