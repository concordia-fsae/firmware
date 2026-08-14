#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

from clang.cindex import Config, CursorKind, Index


import re

RUST_ENUM_RE = re.compile(r"pub\s+enum\s+(\w+)\s*\{(?P<body>.*?)\n\}", re.DOTALL)
RUST_MEMBER_RE = re.compile(r"^\s*(\w+)(?:\s*=\s*(-?\d+))?,", re.MULTILINE)



def camel_to_screaming(name: str) -> str:
    words = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
    words = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", words)
    return words.upper()


def python_member_name(
    name: str, strip_prefix: str, strip_suffix: str, case: str
) -> str:
    if strip_prefix and name.startswith(strip_prefix):
        name = name[len(strip_prefix) :]
    if strip_suffix and name.endswith(strip_suffix):
        name = name[: -len(strip_suffix)]
    if case == "camel":
        name = camel_to_screaming(name)
    name = name.upper()
    return f"_{name}" if name[:1].isdigit() else name


def find_libclang() -> str | None:
    for path in (
        "/usr/lib/llvm-18/lib/libclang.so.1",
        "/usr/lib/llvm-18/lib/libclang.so",
        "/usr/lib/llvm-15/lib/libclang.so.1",
        "/usr/lib/llvm-15/lib/libclang.so",
    ):
        if Path(path).exists():
            return path
    return None


def parse_c_enums(
    wrapper: Path, clang_args: list[str]
) -> dict[str, list[tuple[str, int]]]:
    libclang = find_libclang()
    if libclang:
        Config.set_library_file(libclang)

    translation_unit = Index.create().parse(str(wrapper), args=clang_args)
    errors = [
        diag for diag in translation_unit.diagnostics if diag.severity >= diag.Error
    ]
    if errors:
        message = "\n".join(str(diag) for diag in errors)
        raise RuntimeError(f"libclang failed to parse {wrapper}:\n{message}")

    enums: dict[str, list[tuple[str, int]]] = {}

    def walk(cursor) -> None:
        if cursor.kind == CursorKind.ENUM_DECL and cursor.spelling:
            members = [
                (child.spelling, child.enum_value)
                for child in cursor.get_children()
                if child.kind == CursorKind.ENUM_CONSTANT_DECL
            ]
            enums[cursor.spelling] = members
        for child in cursor.get_children():
            walk(child)

    walk(translation_unit.cursor)
    return enums


def parse_rust_enum(source: str, enum_name: str) -> list[tuple[str, int]]:
    for match in RUST_ENUM_RE.finditer(source):
        if match.group(1) == enum_name:
            members = []
            next_value = 0
            for name, value in RUST_MEMBER_RE.findall(match.group("body")):
                if value:
                    next_value = int(value)
                members.append((name, next_value))
                next_value += 1
            return members
    raise ValueError(f"Rust enum {enum_name!r} was not found")


def enum_spec(spec: str) -> tuple[str, str, str, str, str]:
    parts = spec.split(":")
    if len(parts) != 5:
        raise ValueError(
            f"enum spec {spec!r} must be enum:python_enum:strip_prefix:strip_suffix:case"
        )
    return tuple(parts)  # type: ignore[return-value]


def emit_enum(
    members: list[tuple[str, int]],
    python_enum: str,
    strip_prefix: str,
    strip_suffix: str,
    case: str,
) -> list[str]:
    lines = [f"class {python_enum}(IntEnum):"]
    for name, value in members:
        member = python_member_name(name, strip_prefix, strip_suffix, case)
        lines.append(f"    {member} = {value}")
    if len(lines) == 1:
        lines.append("    pass")
    return lines


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--c-wrapper", required=True, type=Path)
    parser.add_argument("--c-enum", action="append", default=[])
    parser.add_argument("--rust-source", action="append", default=[])
    parser.add_argument("--rust-enum", action="append", default=[])
    parser.add_argument("--c-enums-auto", action="store_true")
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("clang_args", nargs=argparse.REMAINDER)
    args = parser.parse_args()

    clang_args = args.clang_args
    if clang_args and clang_args[0] == "--":
        clang_args = clang_args[1:]

    c_enums = parse_c_enums(args.c_wrapper, clang_args)
    rust_sources = [Path(path).read_text() for path in args.rust_source]
    output = [
        "from __future__ import annotations",
        "",
        "from enum import IntEnum",
        "",
        "",
    ]

    for spec in args.c_enum:
        c_enum, python_enum, strip_prefix, strip_suffix, case = enum_spec(spec)
        if c_enum not in c_enums:
            raise ValueError(f"C enum {c_enum!r} was not found")
        output.extend(
            emit_enum(c_enums[c_enum], python_enum, strip_prefix, strip_suffix, case)
        )
        output.extend(["", ""])

    if args.c_enums_auto:
        for c_enum, members in c_enums.items():
            if not (c_enum.startswith("CAN_") and c_enum.endswith("_E")):
                continue
            enum_name = c_enum.removeprefix("CAN_").removesuffix("_E")
            enum_name = enum_name[:1].upper() + enum_name[1:]
            output.extend(
                emit_enum(
                    members,
                    enum_name,
                    f"CAN_{enum_name.upper()}_",
                    "",
                    "upper",
                )
            )
            output.extend(["", ""])

    for spec in args.rust_enum:
        rust_enum, python_enum, strip_prefix, strip_suffix, case = enum_spec(spec)
        for source in rust_sources:
            try:
                members = parse_rust_enum(source, rust_enum)
                break
            except ValueError:
                continue
        else:
            raise ValueError(f"Rust enum {rust_enum!r} was not found")
        output.extend(emit_enum(members, python_enum, strip_prefix, strip_suffix, case))
        output.extend(["", ""])

    args.out.write_text("\n".join(output).rstrip() + "\n")


if __name__ == "__main__":
    main()
