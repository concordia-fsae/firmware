def identify_carputer(name: str):
    return native.command_alias(
        name = name,
        exe = "//drive-stack/net-detec:net-detec",
        args = ["-s", "_carputer._tcp.local.", "client"],
    )
