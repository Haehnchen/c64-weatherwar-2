#!/usr/bin/env python3
"""Check continuous SID playback, levels, clipping and scene transitions."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import wave

PAL_HZ = 985248
TAIL_CYCLES = PAL_HZ


def is_sid(event: dict) -> bool:
    if "register" in event:
        return isinstance(event["register"], int) and 0 <= event["register"] <= 24
    address = event.get("address")
    return isinstance(address, int) and 0xD400 <= address <= 0xD418


def sid_pair(event: dict) -> tuple[int, int]:
    register = event.get("register")
    if register is None:
        register = event["address"] - 0xD400
    return int(register), int(event["value"])


def read_native_scene(directory: Path, label: str) -> dict:
    timing = json.loads((directory / "timing.json").read_text())
    events = [event for event in json.loads((directory / "events.json").read_text()) if is_sid(event)]
    return {"label": label, "end_cycle": timing["end_cycle"],
            "events": events, "requires_signal": True}


def dump_native(executable: Path, flag: str, output: Path) -> None:
    subprocess.run([str(executable.resolve()), flag, str(output)], check=True,
                   capture_output=True, text=True, timeout=120)


def compose(segments: list[dict]) -> tuple[bytes, list[dict], int]:
    cursor = 0
    events = []
    boundaries = []
    for segment in segments:
        start = cursor
        for event in segment["events"]:
            events.append((start + int(event["cycle"]), *sid_pair(event)))
        cursor += int(segment["end_cycle"])
        boundaries.append({"label": segment["label"], "start_cycle": start,
                           "end_cycle": cursor,
                           "requires_signal": segment.get("requires_signal", False)})
    end_cycle = cursor + TAIL_CYCLES
    output = bytearray(b"WWS1")
    output.extend(struct.pack("<QI", end_cycle, len(events)))
    for event in events:
        output.extend(struct.pack("<QBB", *event))
    return bytes(output), boundaries, end_cycle


def analyze_wav(path: Path, boundaries: list[dict], end_cycle: int) -> dict:
    with wave.open(str(path)) as stream:
        if (stream.getnchannels(), stream.getsampwidth(), stream.getframerate()) != (1, 2, 48000):
            raise ValueError(f"unexpected PCM format: {path}")
        count = stream.getnframes()
        samples = struct.unpack(f"<{count}h", stream.readframes(count))
    if not samples:
        raise ValueError(f"empty PCM export: {path}")
    index = lambda cycle: min(len(samples), round(cycle * len(samples) / end_cycle))
    segment_metrics = []
    for segment in boundaries:
        start, end = index(segment["start_cycle"]), index(segment["end_cycle"])
        part = samples[start:end]
        peak = max(map(abs, part), default=0)
        signal_passed = not segment["requires_signal"] or peak > 500
        segment_metrics.append({"label": segment["label"], "samples": len(part),
                                "peak": peak,
                                "signal_required": segment["requires_signal"],
                                "signal_passed": signal_passed})
    transition_jumps = []
    for segment in boundaries[:-1]:
        boundary = index(segment["end_cycle"])
        if 0 < boundary < len(samples):
            transition_jumps.append({"after": segment["label"],
                                     "jump": abs(samples[boundary] - samples[boundary - 1])})
    clips = sum(abs(sample) >= 32767 for sample in samples)
    peak = max(map(abs, samples))
    max_jump = max((abs(right - left) for left, right in zip(samples, samples[1:])), default=0)
    max_transition_jump = max((row["jump"] for row in transition_jumps), default=0)
    tail = samples[-4800:]
    tail_peak = max(map(abs, tail), default=0)
    passed = (500 < peak < 32000 and clips == 0 and max_jump < 16384 and
              max_transition_jump < 8192 and tail_peak < 1000 and
              all(row["signal_passed"] for row in segment_metrics))
    return {"samples": len(samples), "peak": peak, "clips": clips,
            "max_sample_jump": max_jump, "max_transition_jump": max_transition_jump,
            "tail_peak": tail_peak, "segments": segment_metrics,
            "transition_jumps": transition_jumps, "passed": passed}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderer", type=Path, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="weatherwar-audio-transitions-") as temporary:
        root = Path(temporary)
        for name in ("sequences", "attack", "nature", "match", "turn"):
            dump_native(args.executable, "--dump-" + name, root / name)

        def sequence(name):
            data = json.loads((root / "sequences" / f"{name}.json").read_text())
            return {**data, "label": name, "requires_signal": True}

        round_tone = sequence("round")
        prelude = [sequence(name) for name in
                   ("startup", "entry", "first-name", "second-name", "board", "round")]
        weapons = list(prelude)
        for name in ("H0", "R100", "L0", "L-150", "L150", "T0", "T150"):
            weapons.extend((read_native_scene(root / "attack" / name, name), round_tone))
        intros = list((root / "nature").glob("move-*/nature-intro/events.json"))
        if len(intros) != 1:
            raise ValueError(f"expected one nature intro, found {len(intros)}")
        nature = intros[0].parent.parent
        statistics = json.loads((root / "turn/statistics-timing.json").read_text())
        if statistics["sid_stores"] != 0:
            raise ValueError("statistics must not write SID registers")
        gap = {"label": "statistics/tail", "end_cycle": statistics["delay_cycles"],
               "events": [], "requires_signal": True}
        chains = {
            "title_setup_round": prelude,
            "weapons_rounds": weapons,
            "nature_weather_round": [*prelude,
                read_native_scene(nature / "nature-intro", "nature/intro"),
                read_native_scene(nature, "nature/weather"), round_tone],
            "result_statistics_replay": [*prelude,
                read_native_scene(root / "match/result", "result"), gap,
                read_native_scene(root / "match/replay-music", "replay/music"),
                read_native_scene(root / "match/replay-board", "replay/board"), round_tone],
        }
        for name, segments in chains.items():
            data, boundaries, end_cycle = compose(segments)
            sid_path, wav_path = root / f"{name}.sid", root / f"{name}.wav"
            sid_path.write_bytes(data)
            subprocess.run([str(args.renderer.resolve()), str(sid_path), str(wav_path)],
                           check=True, capture_output=True, timeout=60)
            metrics = analyze_wav(wav_path, boundaries, end_cycle)
            if not metrics["passed"]:
                raise ValueError(f"{name}: {metrics}")
            print(f"{name}: {metrics['samples']} samples, peak={metrics['peak']}, no clipping")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
