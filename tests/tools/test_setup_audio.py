"""Validate the pinned upstream SID checkout."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("setup_audio", ROOT / "tools/setup_audio.py")
SETUP = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SETUP)


def git(directory, *args):
    return subprocess.check_output(["git", *args], cwd=directory,
                                   stderr=subprocess.DEVNULL, text=True).strip()


class SetupAudioTests(unittest.TestCase):
    def test_windows_commands_use_msys_shell_with_separate_arguments(self):
        with patch.object(SETUP.sys, "platform", "win32"), \
             patch.object(SETUP.subprocess, "run") as run:
            SETUP.run(["configure", "--prefix=C:/a path/with $characters"], cwd=ROOT)
        run.assert_called_once_with(
            ["bash", "-c", 'exec "$@"', "bash", "configure",
             "--prefix=C:/a path/with $characters"], cwd=ROOT, check=True)

    def test_windows_configure_paths_use_cygpath(self):
        with patch.object(SETUP.sys, "platform", "win32"), \
             patch.object(SETUP.subprocess, "check_output", return_value="/c/game path\n") as output:
            self.assertEqual(SETUP.shell_path(self.root), "/c/game path")
        output.assert_called_once_with(["cygpath", "-u", str(self.root)], text=True)

    def setUp(self):
        self.storage = tempfile.TemporaryDirectory(prefix="weatherwar-sid-source-")
        self.addCleanup(self.storage.cleanup)
        self.root = Path(self.storage.name)
        self.source = self.root / "source"
        self.source.mkdir()
        (self.source / "file.txt").write_text("upstream\n")
        (self.source / "COPYING").write_text("SID license fixture\n")
        git(self.source, "init", "-q")
        git(self.source, "config", "user.name", "tests")
        git(self.source, "config", "user.email", "tests@example.invalid")
        git(self.source, "add", ".")
        git(self.source, "commit", "-qm", "fixture")
        self.revision = git(self.source, "rev-parse", "HEAD")

    @staticmethod
    def write_install(prefix: Path, *, revision: str = SETUP.REVISION,
                      source: str = "upstream") -> None:
        (prefix / "lib").mkdir(parents=True)
        (prefix / "include/residfp").mkdir(parents=True)
        (prefix / "lib/libresidfp.a").write_bytes(b"pinned SID library")
        (prefix / "include/residfp/residfp.h").write_text("/* pinned */\n")
        (prefix / "SOURCE.txt").write_text(
            f"revision={revision}\nsource={source}\n", encoding="utf-8")

    def test_valid_install_is_reused_without_setup(self):
        prefix = self.root / "prefix"
        self.write_install(prefix)
        with patch.object(SETUP, "ensure_source") as ensure_source, \
             patch.object(SETUP, "run") as run:
            result = SETUP.setup(prefix=prefix)
        self.assertEqual(Path(result).resolve(), prefix.resolve())
        ensure_source.assert_not_called()
        run.assert_not_called()

    def test_stale_or_incomplete_install_is_rejected(self):
        cases = (
            ("missing library", lambda prefix: (prefix / "lib/libresidfp.a").unlink()),
            ("missing header", lambda prefix: (prefix / "include/residfp/residfp.h").unlink()),
            ("missing metadata", lambda prefix: (prefix / "SOURCE.txt").unlink()),
            ("wrong revision", lambda prefix: (prefix / "SOURCE.txt").write_text(
                "revision=stale\nsource=upstream\n")),
            ("wrong source", lambda prefix: (prefix / "SOURCE.txt").write_text(
                f"revision={SETUP.REVISION}\nsource=patched\n")),
        )
        for name, mutate in cases:
            with self.subTest(name=name):
                prefix = self.root / name.replace(" ", "-")
                self.write_install(prefix)
                mutate(prefix)
                self.assertFalse(SETUP.is_installed(prefix))

    def test_missing_source_uses_pinned_remote_clone(self):
        missing_source = self.root / "cached-source"
        build = self.root / "build"
        prefix = self.root / "prefix"
        with patch.object(SETUP, "SOURCE", missing_source), \
             patch.object(SETUP, "BUILD", build), \
             patch.object(SETUP, "clone_remote", return_value=self.source) as clone_remote, \
             patch.object(SETUP, "run") as run:
            result = SETUP.setup(prefix=prefix)
        self.assertEqual(Path(result).resolve(), prefix.resolve())
        clone_remote.assert_called_once()
        self.assertEqual(clone_remote.call_args.args[0], missing_source)
        self.assertTrue(run.called)

    def test_clone_preserves_pristine_revision(self):
        target = self.root / "clone"
        self.assertEqual(SETUP.clone_local(self.source, target, self.revision), target)
        self.assertEqual(SETUP.verify_source(target, self.revision), target)
        self.assertEqual(git(self.source, "status", "--porcelain"), "")

    def test_wrong_revision_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "Unexpected SID library revision"):
            SETUP.verify_source(self.source, "0" * 40)

    def test_modified_source_is_rejected(self):
        (self.source / "file.txt").write_text("edited\n")
        with self.assertRaisesRegex(RuntimeError, "modifications"):
            SETUP.verify_source(self.source, self.revision)

    def test_untracked_source_is_rejected(self):
        (self.source / "extra.txt").write_text("extra\n")
        with self.assertRaisesRegex(RuntimeError, "modifications"):
            SETUP.verify_source(self.source, self.revision)

    def test_subdirectory_is_rejected(self):
        child = self.source / "subdir"
        child.mkdir()
        with self.assertRaisesRegex(RuntimeError, "repository root"):
            SETUP.verify_source(child, self.revision)


if __name__ == "__main__":
    unittest.main()
