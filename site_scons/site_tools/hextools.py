import struct

from SCons.Script import Action, Builder
from fastcrc import crc32


# move the following builder to a tool
def inject_mpeg2(env, target, source):
    if "start_addr" not in env:
        return Exception("start_addr has not been provided")

    with open(source[0].abspath, "rb") as fd:
        first_word = struct.unpack("<L", fd.read(4))[0]
        fd.seek(0)
        # get the app start address from the first word of the binary
        if first_word > 0x08002000 and first_word < 0x20000000:
            app_start = first_word - env["start_addr"]
        else:
            app_start = 0

        # extract the app header since we don't want to crc it
        app_header = fd.read(app_start)

        # extract the app data
        fd.seek(app_start)
        app_bytes = fd.read()

    # crc initial value, per mpeg2 crc spec
    app_crc = 0xFFFFFFFF
    for word in struct.iter_unpack("<L", app_bytes):
        app_crc = crc32.mpeg_2(word[0].to_bytes(length=4, byteorder="big"), app_crc)

    app_crc_packed = struct.pack("<L", app_crc)

    print("=" * 10 + " CRC INFO " + "=" * 10)
    print("first word: " + hex(first_word))
    print(f"app addr: {env["start_addr"]} bytes")
    print(f"app start: {len(app_header)} bytes")
    print(f"app len: {len(app_bytes)} bytes")
    print(f"last 4 word: 0x{app_bytes[-16:].hex()}")
    print(f"app crc: {hex(app_crc)}")
    print(f"app crc packed: 0x{app_crc_packed.hex()}")
    print("=" * 30)

    print(f"Writing to file '{target[0]}'")
    with open(target[0].abspath, "wb") as fd_crc:
        fd_crc.write(app_header)
        fd_crc.write(app_bytes)
        fd_crc.write(app_crc_packed)
        # fd_crc.write(0x00.to_bytes(4, byteorder="big"))

    return None


def generate(env):
    env.Replace(CRCCOMSTR="Calculating and injecting CRC for file '$SOURCE'")

    env["BUILDERS"]["InjectMpeg2CRC"] = Builder(
        action=Action(
            inject_mpeg2,
            cmdstr="$CRCCOMSTR",
        ),
        src_suffix=".bin",
        suffix="_crc.bin",
        single_source=True,
    )


def exists():
    return True
