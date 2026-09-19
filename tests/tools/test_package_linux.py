"""Regression tests for Linux runtime isolation and launcher behavior."""

from __future__ import annotations

import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zipfile


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("package_linux", ROOT / "tools/package_linux.py")
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("Cannot import Linux packager")
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class LinuxPackageTests(unittest.TestCase):
    def test_archive_keeps_runtime_libraries_and_excludes_host_glibc(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            executable = root / "weatherwar"
            executable.write_bytes(b"game")
            host = ["libc.so.6", "libm.so.6", "libmvec.so.1", "libpthread.so.0",
                    "libdl.so.2", "librt.so.1", "libresolv.so.2", "libutil.so.1",
                    "libnss_dns.so.2", "ld-linux-x86-64.so.2"]
            runtime = ["libstdc++.so.6", "libgcc_s.so.1", "libSDL3.so.0"]
            lines = []
            for name in host + runtime:
                library = root / name
                library.write_bytes(name.encode())
                lines.append(f"{name} => {library} (0x1234)")
            lines.append(f"{root}/ld-linux-x86-64.so.2 (0x5678)")
            result = subprocess.CompletedProcess([], 0, "\n".join(lines), "")
            archive = root / "release.zip"
            with patch.object(PACKAGE, "_run", return_value=result):
                PACKAGE.package(executable, archive, "strip")
            with zipfile.ZipFile(archive) as bundled:
                self.assertEqual(set(bundled.namelist()), {
                    "weatherwar", "bin/weatherwar", "README.txt",
                    *(f"lib/{name}" for name in runtime),
                })
                self.assertTrue(bundled.getinfo("weatherwar").external_attr >> 16 & 0o111)
                launcher = bundled.read("weatherwar").decode()
                self.assertNotIn("ld-linux", launcher)
                self.assertNotIn("--library-path", launcher)

    @unittest.skipIf(os.name == "nt", "Launcher requires a POSIX shell")
    def test_launcher_handles_spaces_arguments_and_existing_library_path(self) -> None:
        with tempfile.TemporaryDirectory(prefix="weatherwar bundle ") as temporary:
            root = Path(temporary)
            (root / "bin").mkdir()
            game = root / "bin/weatherwar"
            game.write_text('#!/bin/sh\nprintf "%s\\n" "$LD_LIBRARY_PATH" "$@"\n')
            game.chmod(0o755)
            launcher = root / "weatherwar"
            PACKAGE._write_launcher(launcher)
            for previous in (None, "", "/custom/lib:/another/lib"):
                with self.subTest(previous=previous):
                    env = dict(os.environ)
                    env.pop("LD_LIBRARY_PATH", None)
                    if previous is not None:
                        env["LD_LIBRARY_PATH"] = previous
                    result = subprocess.run([str(launcher), "argument with spaces", "--smoke-test"],
                                            env=env, cwd="/", check=True,
                                            capture_output=True, text=True)
                    expected = str(root / "lib") + (f":{previous}" if previous else "")
                    self.assertEqual(result.stdout.splitlines(),
                                     [expected, "argument with spaces", "--smoke-test"])

    def test_missing_dependencies_are_reported(self) -> None:
        with self.assertRaisesRegex(PACKAGE.PackagingError, "missing library"):
            PACKAGE._parse_ldd("libSDL3.so.0 => not found")


if __name__ == "__main__":
    unittest.main()
