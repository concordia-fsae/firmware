load("@prelude//:rules.bzl", __rules__ = "rules")

def strip_prefix(s, p):
    return s[len(p):] if s.startswith(p) else s

def remap_files(base: str, files: list[str]):
    remap = { strip_prefix(h, base): h for h in files }
    return remap
