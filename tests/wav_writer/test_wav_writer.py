"""WavWriter end-to-end tests (host-only).

The sketch:
  - runs in-memory assertions and round-trip checks (printed as PASS/FAIL),
  - writes three WAV files into output/ via FILE*-backed sink.

This Python side then verifies the on-disk files with the standard
`wave` module — an independent implementation — so the test confirms our
header layout matches the canonical RIFF/WAVE format.

Everything is bundled into a single test function because pytest-embedded
spins up the sketch fresh per test.
"""

import re
import struct
import wave
from pathlib import Path


def _verify_sine_mono16(path: Path) -> None:
    with wave.open(str(path), "rb") as w:
        assert w.getnchannels() == 1
        assert w.getsampwidth() == 2
        assert w.getframerate() == 22050
        assert w.getnframes() == 1102
        frames = w.readframes(w.getnframes())
    samples = [s[0] for s in struct.iter_unpack("<h", frames)]
    assert abs(samples[0]) < 100, f"first sample not near 0: {samples[0]}"
    peak = max(abs(s) for s in samples)
    assert 16000 <= peak <= 16500, f"unexpected peak {peak}"


def _verify_stereo16_pattern(path: Path) -> None:
    with wave.open(str(path), "rb") as w:
        assert w.getnchannels() == 2
        assert w.getsampwidth() == 2
        assert w.getframerate() == 44100
        assert w.getnframes() == 32
        frames = w.readframes(w.getnframes())
    samples = [s[0] for s in struct.iter_unpack("<h", frames)]
    for i in range(32):
        l, r = samples[2 * i], samples[2 * i + 1]
        assert l ==  i * 100, f"L frame {i}: {l}"
        assert r == -i * 100, f"R frame {i}: {r}"


def _verify_mono8_ramp(path: Path) -> None:
    with wave.open(str(path), "rb") as w:
        assert w.getnchannels() == 1
        assert w.getsampwidth() == 1
        assert w.getframerate() == 8000
        assert w.getnframes() == 16
        frames = w.readframes(w.getnframes())
    for i, s in enumerate(frames):
        assert s == i * 16, f"frame {i}: {s}"


def test_wav_writer(dut):
    dut.expect("TEST start", timeout=10)
    match = dut.expect(re.compile(rb"TEST done (\d+)/(\d+)"), timeout=30)
    passed, total = int(match.group(1)), int(match.group(2))
    assert passed == total, f"{total - passed} of {total} sketch assertions failed"

    out = Path(__file__).parent / "output"
    _verify_sine_mono16   (out / "sine_440hz_mono_16bit_22050.wav")
    _verify_stereo16_pattern(out / "pattern_stereo_16bit_44100.wav")
    _verify_mono8_ramp    (out / "ramp_mono_8bit_8000.wav")
