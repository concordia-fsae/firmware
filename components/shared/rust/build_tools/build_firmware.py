"""Run a locked Cargo build/test using only declared sources in an isolated layout."""
import argparse
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tempfile


def relative_path(value):
    path = PurePosixPath(value)
    if path.is_absolute() or ".." in path.parts or not path.parts:
        raise ValueError(f"package path must stay within the staging directory: {value}")
    return path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--package-path", required=True)
    parser.add_argument("--dependency", nargs=2, action="append", default=[])
    parser.add_argument("--protocol", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--rust-toolchain", required=True)
    parser.add_argument("--target", required=True)
    parser.add_argument("--feature", required=True)
    parser.add_argument("--test-package", action="append", default=[])
    parser.add_argument("--test", action="store_true")
    args = parser.parse_args()
    package_path = relative_path(args.package_path)
    dependencies = [(relative_path(path), Path(source).resolve()) for path, source in args.dependency]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="firmware-rust-", dir=os.environ.get("FIRMWARE_BUILD_TMPDIR")) as scratch:
        package = Path(scratch) / package_path
        shutil.copytree(args.source.resolve(), package, ignore=shutil.ignore_patterns("target", "*.map"))
        for path, source in dependencies:
            shutil.copytree(source, Path(scratch) / path)
        env = os.environ.copy()
        env.pop("FIRMWARE_PROTOCOL", None)
        env.pop("CARGO_TARGET_DIR", None)
        env.pop("CARGO_BUILD_TARGET", None)
        if args.protocol:
            env["FIRMWARE_PROTOCOL"] = str(args.protocol.resolve())
        compiler_tmp = Path(scratch) / "compiler-tmp"
        compiler_tmp.mkdir()
        env["TMPDIR"] = str(compiler_tmp)
        env.pop("RUSTFLAGS", None)
        env.pop("CARGO_ENCODED_RUSTFLAGS", None)
        command = ["rustup", "run", args.rust_toolchain, "cargo"]
        if args.test:
            subprocess.run(command + ["test", "--locked", "--lib"], cwd=package, env=env, check=True)
            for name in args.test_package:
                subprocess.run(command + ["test", "--locked", "-p", name, "--lib"], cwd=package, env=env, check=True)
            (output / "passed").write_text("Application and shared Rust host tests passed\n")
        else:
            subprocess.run(command + ["build", "--locked", "--release", "--bin", args.binary,
                                     "--features", args.feature, "--target", args.target], cwd=package, env=env, check=True)
            shutil.copyfile(package / "target" / args.target / "release" / args.binary, output / f"{args.binary}.elf")
            shutil.copyfile(package / f"{args.binary}.map", output / f"{args.binary}.map")


if __name__ == "__main__":
    main()
