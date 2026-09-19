#!/usr/bin/env python3
"""Check repeatable SID events and bounded PCM variation across processes."""
import argparse
import hashlib
from pathlib import Path
import math
import struct
import subprocess
import tempfile
import wave


def verify(executable: Path):
    reference = None
    sid_hash = None
    largest_error = 0.0
    with tempfile.TemporaryDirectory(prefix="weatherwar-audio-repeat-") as temporary:
        for index in range(3):
            path = Path(temporary) / f"startup-{index}.wav"
            subprocess.run([str(executable.resolve()), "--dump-audio", str(path)],
                           check=True, timeout=30, capture_output=True)
            with wave.open(str(path), "rb") as stream:
                count = stream.getnframes()
                if count == 0 or (stream.getnchannels(), stream.getsampwidth(),
                                  stream.getframerate()) != (1, 2, 48000):
                    raise ValueError("Invalid startup PCM export")
                samples = struct.unpack(f"<{count}h", stream.readframes(count))
            digest = hashlib.sha256(Path(str(path) + ".sid.txt").read_bytes()).hexdigest()
            if reference is None:
                reference, sid_hash = samples, digest
                energy = sum(sample * sample for sample in reference)
                if energy == 0:
                    raise ValueError("Silent startup PCM")
            else:
                if digest != sid_hash or len(samples) != len(reference):
                    raise ValueError("SID events or PCM length changed between processes")
                # Upstream filter-table dithering varies with thread scheduling.
                error = math.sqrt(sum((a - b) ** 2 for a, b in zip(reference, samples)) / energy)
                if error > 0.005:
                    raise ValueError(f"PCM variation exceeds 0.5% RMS: {error:.4%}")
                largest_error = max(largest_error, error)
    print(f"Three exports: identical SID events and duration; PCM RMS variation {largest_error:.4%}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    verify(parser.parse_args().executable)
