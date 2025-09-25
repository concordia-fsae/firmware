load("@prelude//:rules.bzl", __rules__ = "rules")

DeployableAsset = provider(
    # @unsorted-dict-items
    fields = {
        "node": provider_field(str),
        "binary": provider_field(typing.Any),
    },
)

def _deployable_target(ctx: AnalysisContext) -> list[Provider]:
    return [
        DefaultInfo(),
        DeployableAsset(
            node = ctx.attrs.node,
            binary = ctx.attrs.binary,
        )
    ]

deployable_target = rule(
    impl = _deployable_target,
    attrs = {
        "node": attrs.string(),
        "binary": attrs.dep(providers = [DefaultInfo]),
    },
)

def _conUDS_batch(ctx: AnalysisContext) -> list[Provider]:
    args = []
    for binary in ctx.attrs.binaries:
        binary = binary[DeployableAsset]
        args.append("-u")
        args.append("{}:$(location {})".format(binary.node, binary.binary))

    return __rules__["command_alias"](
        name = ctx.attrs.name,
        exe = "//drive-stack/conUDS:conUDS",
        args = ["-m", "$(location {})".format(ctx.attrs.manifest), "batch", "{}".format(*args)],
    )

conUDS_batch = rule(
    impl = _conUDS_batch,
    attrs = {
        "binaries": attrs.list(attrs.dep(providers = [DeployableAsset])),
        "manifest": attrs.string(),
    },
)

def conUDS_download(
        name: str,
        binary: str,
        node: str,
        manifest: str):
    return __rules__["command_alias"](
        name = name,
        exe = "//drive-stack/conUDS:conUDS",
        args = ["-m", "$(location {})".format(manifest), "-n", node, "download", "$(location {})".format(binary)],
    )

def conUDS_bootloader_download(
        name: str,
        binary: str,
        node: str,
        manifest: str):
    return __rules__["command_alias"](
        name = name,
        exe = "//drive-stack/conUDS:conUDS",
        args = ["-m", "$(location {})".format(manifest), "-n", node, "bootloader-download", "$(location {})".format(binary)],
    )
