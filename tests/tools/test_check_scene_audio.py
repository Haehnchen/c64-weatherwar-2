"""Offline regression tests for the native scene-audio sanity checker."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import wave


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "check_scene_audio", ROOT / "tools/check_scene_audio.py"
)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("Cannot import scene-audio checker")
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


class SceneAudioTests(unittest.TestCase):
    @staticmethod
    def _write_wav(path: Path, samples: list[int], *, channels: int = 1,
                   rate: int = 48000) -> None:
        with wave.open(str(path), "wb") as output:
            output.setnchannels(channels)
            output.setsampwidth(2)
            output.setframerate(rate)
            output.writeframes(struct.pack(f"<{len(samples)}h", *samples))

    def test_sid_events_accept_numeric_enum_and_validate_address(self) -> None:
        events = [
            {"kind": "screen_write", "address": 0xD400},
            {"kind": 1, "address": 0xD400},
            {"kind": "sid_write", "address": 0xD418},
        ]
        self.assertEqual(CHECK.sid_events(events, "fixture"), events[1:])
        with self.assertRaisesRegex(ValueError, "invalid address"):
            CHECK.sid_events([{"kind": 1, "address": 0xD419}], "fixture")

    def test_missing_sid_events_fail_closed(self) -> None:
        with self.assertRaisesRegex(ValueError, "no SID events"):
            CHECK.sid_events([{"kind": "screen_write", "address": 0xD400}], "fixture")
        with self.assertRaisesRegex(ValueError, "no SID events"):
            CHECK.sid_events([], "fixture")

    def test_valid_mono_pcm_reports_rms(self) -> None:
        with tempfile.TemporaryDirectory(prefix="weatherwar-scene-audio-") as temporary:
            path = Path(temporary) / "attack.wav"
            self._write_wav(path, [0, 1200, -1200, 0])
            result = CHECK.stats(path)
        self.assertEqual(result["frames"], 4)
        self.assertEqual(result["peak"], 1200)
        self.assertEqual(result["rms"], 848.5)
        self.assertEqual(result["clipped"], 0)

    def test_wrong_container_format_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory(prefix="weatherwar-scene-audio-") as temporary:
            path = Path(temporary) / "stereo.wav"
            self._write_wav(path, [0, 1200, -1200, 0, 0, 1200, -1200, 0], channels=2)
            with self.assertRaisesRegex(ValueError, "mono"):
                CHECK.stats(path)

    def test_wrong_rate_and_empty_stream_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory(prefix="weatherwar-scene-audio-") as temporary:
            rate_path = Path(temporary) / "wrong-rate.wav"
            self._write_wav(rate_path, [0, 1200, -1200, 0], rate=44100)
            with self.assertRaisesRegex(ValueError, "48000"):
                CHECK.stats(rate_path)

            empty_path = Path(temporary) / "empty.wav"
            self._write_wav(empty_path, [])
            with self.assertRaisesRegex(ValueError, "empty"):
                CHECK.stats(empty_path)

    def test_isolated_peak_does_not_pass_rms_guard(self) -> None:
        with tempfile.TemporaryDirectory(prefix="weatherwar-scene-audio-") as temporary:
            path = Path(temporary) / "isolated.wav"
            self._write_wav(path, [12000] + [0] * 999)
            with self.assertRaisesRegex(ValueError, "RMS"):
                CHECK.stats(path)


if __name__ == "__main__":
    unittest.main()
