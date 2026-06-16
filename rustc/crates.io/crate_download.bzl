load(":rule.bzl", _crate_download = "crate_download")

def crate_download(**kwargs):
    _crate_download(
        incoming_transition = "//crates.io:download",
        **kwargs
    )
