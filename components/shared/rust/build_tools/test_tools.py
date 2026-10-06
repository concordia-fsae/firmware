"""Integration checks for shared Cargo staging and configuration."""
import argparse
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parent.resolve()))

import build_firmware


class StagingTests(unittest.TestCase):
    def test_package_layout_rejects_paths_outside_staging(self):
        for path in ("/absolute", "../escape", "crate/../../escape", ""):
            with self.assertRaises(ValueError):
                build_firmware.relative_path(path)

    def test_build_supports_a_different_binary_and_dependency_layout(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            for name in ("app", "dep", "protocol"):
                (base / name).mkdir()
            (base / "app/Cargo.toml").write_text("application fixture")
            (base / "dep/Cargo.toml").write_text("dependency fixture")
            output = base / "out"
            command = ["build_firmware.py", "--source", str(base / "app"),
                       "--package-path", "controllers/other", "--dependency", "libraries/common", str(base / "dep"),
                       "--protocol", str(base / "protocol"), "--output", str(output),
                       "--binary", "other-controller", "--target", "other-target", "--feature", "embedded",
                       "--rust-toolchain", "test-version"]

            def compile_fixture(args, cwd, env, check):
                self.assertEqual(args[:4], ["rustup", "run", "test-version", "cargo"])
                self.assertIn("--locked", args)
                self.assertEqual(args[args.index("--bin") + 1], "other-controller")
                self.assertEqual(args[args.index("--target") + 1], "other-target")
                self.assertEqual(args[args.index("--features") + 1], "embedded")
                self.assertTrue(check)
                self.assertEqual(cwd.relative_to(cwd.parents[1]).as_posix(), "controllers/other")
                self.assertTrue((cwd.parents[1] / "libraries/common/Cargo.toml").is_file())
                self.assertEqual(env["FIRMWARE_PROTOCOL"], str(base / "protocol"))
                for name in ("RUSTFLAGS", "CARGO_ENCODED_RUSTFLAGS", "CARGO_TARGET_DIR", "CARGO_BUILD_TARGET"):
                    self.assertNotIn(name, env)
                elf = cwd / "target/other-target/release/other-controller"
                elf.parent.mkdir(parents=True)
                elf.write_bytes(b"fixture ELF")
                (cwd / "other-controller.map").write_text("fixture map")

            with patch.object(sys, "argv", command), patch.object(build_firmware.subprocess, "run", compile_fixture), patch.dict(os.environ, {"RUSTFLAGS": "ambient flags", "CARGO_TARGET_DIR": "outside", "CARGO_BUILD_TARGET": "outside"}):
                build_firmware.main()
            self.assertEqual((output / "other-controller.elf").read_bytes(), b"fixture ELF")
            self.assertEqual((output / "other-controller.map").read_text(), "fixture map")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__]))
    if not result.wasSuccessful():
        sys.exit(1)
    args.output.write_text(f"{result.testsRun} shared build-tool tests passed\n")
