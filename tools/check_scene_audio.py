#!/usr/bin/env python3
"""Check attack PCM levels, clipping and SID event exports."""
import argparse, json, struct, subprocess, sys, tempfile, wave
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build/debug/tests/native_dump"
SCENES = ("H0", "R100", "L0", "L-150", "L150", "T0", "T150")
EXPECTED_CHANNELS = 1
EXPECTED_RATE = 48000
EXPECTED_SAMPLE_WIDTH = 2
MIN_PEAK = 1000
MIN_RMS = 500.0
MAX_PEAK = 32000


def _require(condition, message):
    if not condition:
        raise ValueError(message)


def _is_sid_event(event):
    """Accept both attack-export labels and the numeric enum export format."""
    kind = event.get("kind")
    return kind == "sid_write" or (type(kind) is int and kind == 1)


def sid_events(events, scene="scene"):
    """Validate SID writes in string or numeric event exports."""
    _require(isinstance(events, list), f"{scene}: events must be a list")
    result = []
    for index, event in enumerate(events):
        _require(isinstance(event, dict), f"{scene}: event {index} is not an object")
        if not _is_sid_event(event):
            continue
        address = event.get("address")
        _require(type(address) is int and 0xD400 <= address <= 0xD418,
                 f"{scene}: SID event {index} has invalid address {address!r}")
        result.append(event)
    _require(result, f"{scene}: no SID events")
    return result


def stats(path):
    """Return PCM statistics after validating the native WAV container."""
    with wave.open(str(path)) as w:
        n = w.getnframes()
        channels, sr, sw = w.getnchannels(), w.getframerate(), w.getsampwidth()
        _require(channels == EXPECTED_CHANNELS,
                 f"{path}: expected mono PCM, got {channels} channels")
        _require(sr == EXPECTED_RATE, f"{path}: expected {EXPECTED_RATE} Hz, got {sr}")
        _require(sw == EXPECTED_SAMPLE_WIDTH,
                 f"{path}: expected 16-bit PCM, got {sw * 8}-bit samples")
        _require(w.getcomptype() == "NONE",
                 f"{path}: compressed/non-PCM WAV is not supported ({w.getcomptype()})")
        _require(n > 0, f"{path}: empty PCM stream")
        raw = w.readframes(n)
    _require(len(raw) == n * EXPECTED_SAMPLE_WIDTH,
             f"{path}: truncated PCM payload")
    data = struct.unpack(f"<{n}h", raw)
    peak = max(abs(x) for x in data)
    rms = (sum(x * x for x in data) / n) ** 0.5
    clips = sum(1 for x in data if abs(x) >= 32767)
    lead_silence = next((i for i, x in enumerate(data) if abs(x) > 100), n) / sr
    tail = next((i for i, x in enumerate(reversed(data)) if abs(x) > 100), n) / sr
    _require(peak >= MIN_PEAK, f"{path}: PCM peak is too quiet ({peak})")
    _require(rms >= MIN_RMS,
             f"{path}: PCM RMS is too quiet ({rms:.1f}); isolated peaks are insufficient")
    _require(peak < MAX_PEAK, f"{path}: PCM peak is near clipping ({peak})")
    _require(clips == 0, f"{path}: clipped samples ({clips})")
    return {"frames": n, "seconds": round(n / sr, 2), "peak": peak,
            "rms": round(rms, 1), "clipped": clips,
            "lead_silence_s": round(lead_silence, 3),
            "tail_silence_s": round(tail, 3)}


def inspect(output):
    """Validate all required scene exports below *output* and return a report."""
    report = {}
    for scene in SCENES:
        scene_dir = Path(output) / scene
        wav = scene_dir / "attack.wav"
        events = json.loads((scene_dir / "events.json").read_text())
        scene_sid_events = sid_events(events, scene)
        s = stats(wav)
        s["all_events"] = len(events)
        s["sid_events"] = len(scene_sid_events)
        s["frames_per_sid_event"] = round(s["frames"] / len(scene_sid_events))
        report[scene] = s
    return report


def _run(executable, output):
    output = Path(output)
    subprocess.run([str(executable.resolve()), "--dump-attack", str(output)],
                   check=True, capture_output=True, timeout=120)
    report = inspect(output)
    for k, v in report.items():
        print(k.ljust(6) + " " + str(v["seconds"]).rjust(5) + "s peak=" + str(v["peak"]).rjust(5) +
              " rms=" + str(v["rms"]).rjust(7) + " SID=" + str(v["sid_events"]).rjust(3))
    print("SCENE AUDIO SANITY OK: " + str(len(report)) +
          " scenes, valid native PCM and SID event exports")
    return report


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, default=EXE)
    args = parser.parse_args(argv)
    with tempfile.TemporaryDirectory(prefix="weatherwar-scene-audio-") as temporary:
        _run(args.executable, Path(temporary))
    return 0

if __name__ == "__main__":
    sys.exit(main())
