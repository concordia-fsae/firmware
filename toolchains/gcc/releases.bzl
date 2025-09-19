# naming convention informaton: https://web.archive.org/web/20160410104337/https://community.freescale.com/thread/313490#comment-354077
# summary:
#   Tool chains have  a loose name convention like arch [-vendor] [-os] - eabi
#     arch   -  refers to target architecture (which in this case is ARM)
#     vendor -  refers to toolchain supplier (unused here)
#     os     -  refers to the target operating system (linux or none)
#     abi    -  refers to Application Binary Interface type (eabi, gnu, gnueabihf, etc.)
#
# Only vendorless toolchains are used in this file.
#
# The following structure maps host target triples to the corresponding
# information about a particular cross target triple

releases = {
    "14.2.rel1": {
        "date": "2024-12-10",
        "aarch64-linux": {
            "arm-none-eabi": {
                "sha256sum": "87330bab085dd8749d4ed0ad633674b9dc48b237b61069e3b481abd364d0a684",
                "size": "",
                "archive": "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-aarch64-arm-none-eabi.tar.xz",
                "strip_prefix": "arm-gnu-toolchain-14.2.rel1-aarch64-arm-none-eabi",
            },
            "aarch64-linux-gnu": {
                "sha256sum": "299c56db1644c135670afabbf801b97a42e5ef6069d73157ab869458cbda2096",
                "size": "",
                "archive": "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-aarch64-aarch64-none-linux-gnu.tar.xz",
                "strip_prefix": "arm-gnu-toolchain-14.2.rel1-aarch64-aarch64-none-linux-gnu",
            },
            "arm-linux-gnueabihf": {
                "sha256sum": "299c56db1644c135670afabbf801b97a42e5ef6069d73157ab869458cbda2096",
                "size": "",
                "archive": "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-aarch64-arm-none-linux-gnueabihf.tar.xz",
                "strip_prefix": "arm-gnu-toolchain-14.2.rel1-aarch64-arm-linux-gnueabihf",
            },
        },
        "aarch64-macos": {
            "arm-none-eabi": {
                "sha256sum": "c7c78ffab9bebfce91d99d3c24da6bf4b81c01e16cf551eb2ff9f25b9e0a3818",
                "size": "",
                "archive": "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi.tar.xz",
                "strip_prefix": "arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi",
            },
            "aarch64-none-elf": {
                "sha256sum": "fc111bb4bb4871e521e3c8a89bd0af51cddfd00fe3f526f4faa09398b7c613f5",
                "size": "",
                "archive": "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-darwin-arm64-aarch64-none-elf.tar.xz",
                "strip_prefix": "arm-gnu-toolchain-14.2.rel1-darwin-arm64-aarch64-none-elf",
            },
        },
        "x86_64-linux": {
            "arm-none-eabi": {
                "sha256sum": "62a63b981fe391a9cbad7ef51b17e49aeaa3e7b0d029b36ca1e9c3b2a9b78823",
                "size": "",
                "archive": "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz",
                "strip_prefix": "arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi",
            },
            "aarch64-linux-gnu": {
                "sha256sum": "47aeefc02b0ee39f6d4d1812110952975542d365872a7474b5306924bca4faa1",
                "size": "",
                "archive": "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-aarch64-none-linux-gnu.tar.xz",
                "strip_prefix": "arm-gnu-toolchain-14.2.rel1-x86_64-aarch64-none-linux-gnu",
            },
            "arm-linux-gnueabihf": {
                "sha256sum": "32301a5a33aab47810837cdab848a5a513ca22804d3168d3ada5833828b07912",
                "size": "",
                "archive": "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-linux-gnueabihf.tar.xz",
                "strip_prefix": "arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-linux-gnueabihf",
            },
        },
        "x86_64-macos": {
            "arm-none-eabi": {
                "sha256sum": "2d9e717dd4f7751d18936ae1365d25916534105ebcb7583039eff1092b824505",
                "size": "",
                "archive": "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-darwin-x86_64-arm-none-eabi.tar.xz",
                "strip_prefix": "arm-gnu-toolchain-14.2.rel1-darwin-x86_64-arm-none-eabi",
            },
            "aarch64-none-elf": {
                "sha256sum": "c02735606d69ed000cc8fae2c1467e489e1325c14a7874f553c42f7ef193fc21",
                "size": "",
                "archive": "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-darwin-x86_64-aarch64-none-elf.tar.xz",
                "strip_prefix": "arm-gnu-toolchain-14.2.rel1-darwin-x86_64-aarch64-none-elf",
            },
        },
        "x86_64-windows": {
            "arm-none-eabi": {
                "sha256sum": "f074615953f76036e9a51b87f6577fdb4ed8e77d3322a6f68214e92e7859888f",
                "size": "",
                "archive": "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-mingw-w64-x86_64-arm-none-eabi.zip",
                "strip_prefix": "arm-gnu-toolchain-14.2.rel1-mingw-w64-x86_64-arm-none-eabi",
            },
        },
        "src": {
            "sha256sum": "e6405f20f8a817a50d92dbf7974d0ee77708dfdf9e79900a59c5d343b464ef9c",
            "size": "",
            "archive": "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/srcrel/arm-gnu-toolchain-src-snapshot-14.2.rel1.tar.xz",
            "strip_prefix": "arm-gnu-toolchain-src-snapshot-14.2.rel1",
        },
    },
}
