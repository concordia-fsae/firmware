from argparse import ArgumentParser
import struct

from fastcrc import crc32


def inject_mpeg2(input: str, output: str | None, start_address: int):
    output = output or f"{input.split('.')[:-1]}_crc.bin"

    with open(input, "rb") as fd:
        first_word = struct.unpack("<L", fd.read(4))[0]
        fd.seek(0)

        # get the app start address from the first word of the binary
        if first_word > 0x08002000 and first_word < 0x20000000:
            app_start = first_word - start_address
        else:
            app_start = 0

        # extract the app header since we don't want to crc it
        app_header = fd.read(app_start)

        # extract the app data
        fd.seek(app_start)
        app_bytes = fd.read()

    # initial crc value, per mpeg2 crc spec
    app_crc = 0xFFFFFFFF
    for word in struct.iter_unpack("<L", app_bytes):
        app_crc = crc32.mpeg_2(word[0].to_bytes(length=4, byteorder="big"), app_crc)

    app_crc_packed = struct.pack("<L", app_crc)

    print("=" * 10 + " CRC INFO " + "=" * 10)
    print("first word: " + hex(first_word))
    print(f"app addr: {start_address} bytes")
    print(f"app start: {len(app_header)} bytes")
    print(f"app len: {len(app_bytes)} bytes")
    print(f"last 4 word: 0x{app_bytes[-16:].hex()}")
    print(f"app crc: {hex(app_crc)}")
    print(f"app crc packed: 0x{app_crc_packed.hex()}")
    print("=" * 30)

    print(f"Writing to file '{output}'")
    with open(output, "wb") as fd_crc:
        fd_crc.write(app_header)
        fd_crc.write(app_bytes)
        fd_crc.write(app_crc_packed)

    return None


if __name__ == "__main__":
    parser = ArgumentParser("hextools")

    parser.add_argument(
        "-i",
        "--input",
        required=True,
        help="Path to input file to be CRCed",
        metavar="INPUT_FILE",
    )
    parser.add_argument(
        "-o",
        "--output",
        default=None,
        help="Optional destination for CRCed binary. Defaults to input file path with '_crc' inserted before the extension",
        metavar="OUTPUT_FILE",
    )
    parser.add_argument(
        "-a",
        "--start_address",
        required=True,
        type=int,
        help="App start address",
        metavar="START_ADDRESS",
    )

    args = parser.parse_args()

    inject_mpeg2(args.input, args.output, args.start_address)
