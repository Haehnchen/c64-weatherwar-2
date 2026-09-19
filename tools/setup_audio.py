#!/usr/bin/env python3
"""Build the pinned upstream SID library."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import shutil
import sys
from typing import Optional, Sequence, Union


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "build/deps/libresidfp-src"
BUILD = ROOT / "build/deps/libresidfp-build"
PREFIX = ROOT / "build/deps/libresidfp"
URL = "https://github.com/libsidplayfp/libresidfp.git"
VERSION = "v1.2.2"
REVISION = "7c54a5988f9f1918439ee1180316a78c7a7729bb"

def run(args: Sequence[Union[str, Path]], cwd: Path = ROOT) -> None:
    command = [arg.as_posix() if isinstance(arg, Path) else str(arg) for arg in args]
    if sys.platform == "win32":
        # Resolve PATH first so Windows cannot prefer its System32 WSL launcher.
        bash = shutil.which("bash")
        if bash is None:
            raise RuntimeError("MSYS2 bash is required on PATH for the SID build")
        command = [bash, "-c", 'exec "$@"', "bash", *command]
    subprocess.run(command, cwd=cwd, check=True)


def shell_path(path: Path) -> str:
    if sys.platform == "win32":
        return subprocess.check_output(["cygpath", "-u", str(path)], text=True).strip()
    return str(path)


def git_output(*args: str, cwd: Path) -> str:
    return subprocess.check_output(["git", *args], cwd=cwd, text=True).strip()


def verify_source(path: Path, revision: str = REVISION) -> Path:
    """Require the exact pinned revision and a pristine source checkout."""

    path = path.resolve()
    if not path.is_dir():
        raise RuntimeError(f"SID source directory does not exist: {path}")
    try:
        top_path = git_output("rev-parse", "--show-toplevel", cwd=path)
        if sys.platform == "win32":
            top_path = subprocess.check_output(["cygpath", "-w", top_path], text=True).strip()
        top = Path(top_path).resolve()
        git_revision = git_output("rev-parse", "HEAD", cwd=path)
    except (OSError, subprocess.CalledProcessError) as exc:
        raise RuntimeError(f"SID source is not a git checkout: {path}") from exc
    if top != path:
        raise RuntimeError(f"SID source path is not the repository root: {path}")
    if revision != git_revision:
        raise RuntimeError(
            f"Unexpected SID library revision {git_revision}; expected {revision}"
        )
    status = git_output("status", "--porcelain=v1", "--untracked-files=all", cwd=path)
    if status:
        raise RuntimeError(f"SID source has tracked or untracked modifications: {path}")
    return path


def clone_local(source: Path, target: Path, revision: str = REVISION) -> Path:
    """Clone a local source without hard-linking files from its checkout."""

    if target.exists():
        return verify_source(target, revision)
    target.parent.mkdir(parents=True, exist_ok=True)
    run(["git", "clone", "--config", "core.autocrlf=false", "--no-hardlinks", source, target])
    return verify_source(target, revision)


def clone_remote(target: Path) -> Path:
    """Clone and verify the pinned release."""

    if target.exists():
        return verify_source(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    run(["git", "clone", "--config", "core.autocrlf=false", "--depth", "1", "--branch", VERSION, URL, target])
    return verify_source(target)


def ensure_source(source: Optional[Path]) -> Path:
    """Select or create the project-local source checkout."""

    if SOURCE.exists():
        return verify_source(SOURCE)

    selected = source.resolve() if source is not None else None
    if selected is not None:
        verify_source(selected)
        return clone_local(selected, SOURCE)
    return clone_remote(SOURCE)


def is_installed(prefix: Path) -> bool:
    if not (prefix / "lib/libresidfp.a").is_file() or not (prefix / "include/residfp/residfp.h").is_file():
        return False
    provenance = prefix / "SOURCE.txt"
    if not provenance.is_file():
        return False
    lines = provenance.read_text().splitlines()
    return f"revision={REVISION}" in lines and "source=upstream" in lines


def setup(source: Optional[Union[str, Path]] = None,
          prefix: Union[str, Path] = PREFIX) -> Path:
    """Prepare, build, and install the pinned SID dependency locally."""

    prefix_path = Path(prefix).resolve()
    if is_installed(prefix_path):
        print(f"Using pinned SID library at {prefix_path}")
        return prefix_path
    source_path = Path(source).resolve() if source is not None else None
    source_checkout = ensure_source(source_path)
    prefix_path.parent.mkdir(parents=True, exist_ok=True)
    run(["autoreconf", "-fi"], cwd=source_checkout)
    BUILD.mkdir(parents=True, exist_ok=True)
    run(
        [
            shell_path(source_checkout / "configure"),
            "--disable-tests",
            "--disable-shared",
            f"--prefix={shell_path(prefix_path)}",
        ],
        cwd=BUILD,
    )
    run(["make", "-j4"], cwd=BUILD)
    run(["make", "install"], cwd=BUILD)
    license_dir = prefix_path / "share/licenses/libresidfp"
    license_dir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source_checkout / "COPYING", license_dir / "COPYING")
    (prefix_path / "SOURCE.txt").write_text(
        f"url={URL}\nversion={VERSION}\nrevision={REVISION}\n"
        "source=upstream\nlicense=GPL-2.0-or-later\n", encoding="utf-8")
    print(f"Installed pinned SID chip library to {prefix_path}")
    return prefix_path


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source",
        type=Path,
        help="existing pristine libresidfp checkout at the pinned revision",
    )
    parser.add_argument(
        "--prefix",
        type=Path,
        default=PREFIX,
        help="project-local install prefix (default: build/deps/libresidfp)",
    )
    args = parser.parse_args(argv)
    setup(args.source, prefix=args.prefix)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
