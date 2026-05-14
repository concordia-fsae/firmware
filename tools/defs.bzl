load("@prelude//:rules.bzl", __rules__ = "rules")

def strip_prefix(s, p):
    return s[len(p):] if s.startswith(p) else s

def remap_files(base: str, files: list[str]):
    remap = { strip_prefix(h, base): h for h in files }
    return remap

def host_arch() -> str:
    arch = host_info().arch
    if arch.is_x86_64:
        return "x86_64"
    elif host_info().arch.is_aarch64:
        return "aarch64"
    else:
        fail("Unsupported host architecture '{}'.".format(arch))

def host_os() -> str:
    os = host_info().os
    if os.is_linux:
        return "linux"
    elif os.is_macos:
        return "macos"
    elif os.is_windows:
        return "windows"
    else:
        fail("Unsupported host os '{}'.".format(os))

def _sh_run_impl(ctx: AnalysisContext) -> list[Provider]:
    script = ctx.attrs.script
    args = cmd_args(script)
    if ctx.attrs.args:
        args.add(ctx.attrs.args)
    return [
        DefaultInfo(default_outputs = [script]),
        RunInfo(args = args),
    ]

sh_run = rule(
    impl = _sh_run_impl,
    attrs = {
        "script": attrs.source(),
        "args": attrs.list(attrs.string(), default = []),
    },
)
