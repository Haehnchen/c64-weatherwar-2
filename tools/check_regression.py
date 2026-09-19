#!/usr/bin/env python3
"""Compare native scene exports with checked regression snapshots."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    baseline = Path(__file__).resolve().parents[1] / "tests/fixtures/native_regression.json"
    with tempfile.TemporaryDirectory(prefix="weatherwar-regression-") as temporary:
        for scene, expected in json.loads(baseline.read_text()).items():
            output = Path(temporary) / scene
            output.mkdir()
            target = output / "startup.wav" if scene == "audio" else output
            subprocess.run([str(args.executable.resolve()), "--dump-" + scene, str(target)],
                           check=True, capture_output=True, timeout=120)
            # PCM dithering varies; separate audio checks validate signal integrity.
            # Path ordering ignores case on Windows; keep the POSIX snapshot order.
            files = sorted(
                (path for path in output.rglob("*") if path.is_file() and path.suffix != ".wav"),
                key=lambda path: path.relative_to(output).parts,
            )
            digest = hashlib.sha256()
            for path in files:
                digest.update(path.relative_to(output).as_posix().encode() + bytes([0]))
                data = path.read_bytes()
                if path.suffix in (".json", ".txt"):
                    data = data.replace(b"\r\n", b"\n")
                digest.update(hashlib.sha256(data).digest())
            actual = {"files": len(files), "sha256": digest.hexdigest()}
            if actual != expected:
                raise ValueError(f"{scene}: expected {expected}, got {actual}")
            print(f"{scene}: {len(files)} files unchanged")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
