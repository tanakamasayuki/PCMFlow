"""Generate canonical PCM/WAV test fixtures for PCMFlow tests.

All output is deterministic and produced by this script — no external
audio assets are checked in. License of the generated data follows the
repository (MIT). The generator itself is small enough to also serve as
a CC0-equivalent reference for the file format.

Usage (from anywhere; paths are resolved relative to this file):

    uv run python tools/gen_test_audio.py

Re-running overwrites existing fixtures. Generated files live under each
target test's `input/` directory and ARE committed (small, deterministic,
needed for the test to work).
"""

from __future__ import annotations

import argparse
import math
import shutil
import struct
import subprocess
import sys
from pathlib import Path
from typing import Iterable


TOOLS_DIR = Path(__file__).resolve().parent
TESTS_DIR = TOOLS_DIR.parent


# ---------------------------------------------------------------------------
# WAV writer (RIFF/WAVE, PCM)
# ---------------------------------------------------------------------------

def _wav_header(
    *,
    sample_rate: int,
    channels: int,
    bits_per_sample: int,
    data_bytes: int,
) -> bytes:
    """Standard 44-byte canonical PCM WAV header."""
    block_align   = channels * bits_per_sample // 8
    byte_rate     = sample_rate * block_align
    fmt_chunk_size = 16
    riff_size     = 4 + (8 + fmt_chunk_size) + (8 + data_bytes)
    audio_format  = 1  # PCM
    return b"".join([
        b"RIFF",
        struct.pack("<I", riff_size),
        b"WAVE",
        b"fmt ",
        struct.pack("<I", fmt_chunk_size),
        struct.pack("<H", audio_format),
        struct.pack("<H", channels),
        struct.pack("<I", sample_rate),
        struct.pack("<I", byte_rate),
        struct.pack("<H", block_align),
        struct.pack("<H", bits_per_sample),
        b"data",
        struct.pack("<I", data_bytes),
    ])


def _encode_samples(samples: Iterable[float], bits_per_sample: int) -> bytes:
    """Encode floats in [-1.0, 1.0] to PCM bytes per WAV conventions:
       - 8-bit  -> unsigned, center 128
       - 16-bit -> signed,   center 0
    """
    out = bytearray()
    if bits_per_sample == 8:
        for s in samples:
            v = int(round(max(-1.0, min(1.0, s)) * 127.0)) + 128
            out.append(max(0, min(255, v)))
    elif bits_per_sample == 16:
        for s in samples:
            v = int(round(max(-1.0, min(1.0, s)) * 32767.0))
            v = max(-32768, min(32767, v))
            out.extend(struct.pack("<h", v))
    else:
        raise ValueError(f"unsupported bits_per_sample={bits_per_sample}")
    return bytes(out)


def write_wav(
    path: Path,
    *,
    sample_rate: int,
    channels: int,
    bits_per_sample: int,
    samples_per_channel: list[list[float]],
) -> None:
    """Write a PCM WAV file. `samples_per_channel` is one float list per
    channel, all of equal length. Output is interleaved.
    """
    assert len(samples_per_channel) == channels, "channels mismatch"
    n = len(samples_per_channel[0])
    for ch in samples_per_channel:
        assert len(ch) == n, "channel length mismatch"

    interleaved: list[float] = []
    for i in range(n):
        for c in range(channels):
            interleaved.append(samples_per_channel[c][i])

    data = _encode_samples(interleaved, bits_per_sample)
    header = _wav_header(
        sample_rate=sample_rate,
        channels=channels,
        bits_per_sample=bits_per_sample,
        data_bytes=len(data),
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + data)


# ---------------------------------------------------------------------------
# Waveforms
# ---------------------------------------------------------------------------

def sine(frequency_hz: float, sample_rate: int, duration_sec: float,
         amplitude: float = 0.5) -> list[float]:
    n = int(round(sample_rate * duration_sec))
    return [
        amplitude * math.sin(2.0 * math.pi * frequency_hz * (i / sample_rate))
        for i in range(n)
    ]


def silence(sample_rate: int, duration_sec: float) -> list[float]:
    return [0.0] * int(round(sample_rate * duration_sec))


def ramp(sample_rate: int, duration_sec: float) -> list[float]:
    """Linear sweep from -1.0 to +1.0. Useful as a deterministic, non-zero
    full-range signal that exercises clipping boundaries without aliasing.
    """
    n = int(round(sample_rate * duration_sec))
    if n < 2:
        return [0.0] * n
    return [-1.0 + 2.0 * i / (n - 1) for i in range(n)]


# ---------------------------------------------------------------------------
# Fixture set
# ---------------------------------------------------------------------------

def _identifier(name: str) -> str:
    out = []
    for ch in name:
        if ch.isalnum():
            out.append(ch)
        else:
            out.append("_")
    s = "".join(out)
    if s and s[0].isdigit():
        s = "_" + s
    return s


def encode_mp3_fixtures(out_dir: Path) -> dict[str, Path]:
    """Encode a small set of MP3 fixtures from on-the-fly WAV inputs.

    Returns a mapping of {symbol -> path}. Returns an empty dict and prints
    a warning if `ffmpeg` is not on PATH — MP3 fixtures are then assumed
    to already exist (committed) or be regenerated on a developer machine
    that has ffmpeg installed.
    """
    ffmpeg = shutil.which("ffmpeg")
    if ffmpeg is None:
        print(
            "warning: ffmpeg not found — skipping MP3 fixture generation. "
            "Install ffmpeg to (re)generate these files.",
            file=sys.stderr,
        )
        return {}

    out_dir.mkdir(parents=True, exist_ok=True)

    # Specs: (sym, sample_rate, channels, duration_sec, kbps)
    specs = [
        ("Sine440Mono22050_128k",   22050, 1, 0.2, 128),
        ("Sine440Stereo44100_128k", 44100, 2, 0.2, 128),
    ]
    return _encode_via_ffmpeg(ffmpeg, out_dir, specs, codec="libmp3lame", ext="mp3")


def encode_flac_fixtures(out_dir: Path) -> dict[str, Path]:
    """Encode a small set of FLAC fixtures from on-the-fly WAV inputs."""
    ffmpeg = shutil.which("ffmpeg")
    if ffmpeg is None:
        print(
            "warning: ffmpeg not found — skipping FLAC fixture generation.",
            file=sys.stderr,
        )
        return {}

    out_dir.mkdir(parents=True, exist_ok=True)

    specs = [
        # (sym, sample_rate, channels, duration_sec, kbps_marker_unused)
        ("Sine440Mono22050",   22050, 1, 0.2, 0),
        ("Sine440Stereo44100", 44100, 2, 0.2, 0),
    ]
    return _encode_via_ffmpeg(ffmpeg, out_dir, specs, codec="flac", ext="flac")


def _encode_via_ffmpeg(
    ffmpeg: str,
    out_dir: Path,
    specs: list,
    codec: str,
    ext: str,
) -> dict[str, Path]:
    out: dict[str, Path] = {}
    for sym, sr, ch, dur, kbps in specs:
        wav_path = out_dir / f"_tmp_{sym}.wav"
        if ext == "mp3":
            target_name = f"sine_{sr}_{ch}ch_{kbps}k.{ext}"
        else:
            target_name = f"sine_{sr}_{ch}ch.{ext}"
        target_path = out_dir / target_name

        if ch == 1:
            channels = [sine(440.0, sr, dur)]
        else:
            channels = [sine(440.0, sr, dur), sine(880.0, sr, dur)]
        write_wav(
            wav_path,
            sample_rate=sr,
            channels=ch,
            bits_per_sample=16,
            samples_per_channel=channels,
        )

        cmd = [ffmpeg, "-y", "-loglevel", "error", "-i", str(wav_path)]
        if codec == "libmp3lame":
            cmd += ["-codec:a", "libmp3lame", "-b:a", f"{kbps}k"]
        elif codec == "flac":
            cmd += ["-codec:a", "flac"]
        cmd += [str(target_path)]
        subprocess.run(cmd, check=True)
        wav_path.unlink(missing_ok=True)
        out[sym] = target_path

    return out


def emit_c_header(header_path: Path, wavs: dict[str, Path], namespace: str) -> None:
    """Emit a single C header that exposes each WAV file as a `static const
    uint8_t kFoo[]` array plus a matching `kFooSize` constant.

    The header is included by the test sketch so the same fixtures run on
    host and on real hardware (the bytes go into flash).
    """
    lines: list[str] = []
    lines.append("// Auto-generated by tools/gen_test_audio.py — do not edit.")
    lines.append("#pragma once")
    lines.append("#include <stdint.h>")
    lines.append("#include <stddef.h>")
    lines.append("")
    lines.append(f"namespace {namespace} {{")
    lines.append("")
    for sym, path in sorted(wavs.items()):
        data = path.read_bytes()
        lines.append(f"// {path.name} ({len(data)} bytes)")
        lines.append(f"static const uint8_t k{sym}[] = {{")
        for i in range(0, len(data), 16):
            chunk = data[i : i + 16]
            lines.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
        lines.append("};")
        lines.append(f"static const size_t k{sym}Size = sizeof(k{sym});")
        lines.append("")
    lines.append(f"}}  // namespace {namespace}")
    lines.append("")
    header_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--out-root",
        type=Path,
        default=TESTS_DIR,
        help="Root under which per-test input/ directories are populated.",
    )
    args = parser.parse_args()
    root = args.out_root.resolve()

    # WAV reader fixtures. Short durations keep the binaries small enough
    # to embed in flash-constrained targets if ever needed.
    wav_dir = root / "wav_reader" / "input"

    # mono / 8-bit / 22050 Hz / 0.05s sine
    write_wav(
        wav_dir / "sine_440hz_mono_8bit_22050.wav",
        sample_rate=22050,
        channels=1,
        bits_per_sample=8,
        samples_per_channel=[sine(440.0, 22050, 0.05)],
    )

    # mono / 16-bit / 22050 Hz / 0.05s sine
    write_wav(
        wav_dir / "sine_440hz_mono_16bit_22050.wav",
        sample_rate=22050,
        channels=1,
        bits_per_sample=16,
        samples_per_channel=[sine(440.0, 22050, 0.05)],
    )

    # stereo / 16-bit / 44100 Hz / 0.05s — L: 440 Hz, R: 880 Hz
    write_wav(
        wav_dir / "sine_stereo_16bit_44100.wav",
        sample_rate=44100,
        channels=2,
        bits_per_sample=16,
        samples_per_channel=[
            sine(440.0, 44100, 0.05),
            sine(880.0, 44100, 0.05),
        ],
    )

    # stereo / 16-bit / 48000 Hz / 0.05s — silence (edge case)
    write_wav(
        wav_dir / "silence_stereo_16bit_48000.wav",
        sample_rate=48000,
        channels=2,
        bits_per_sample=16,
        samples_per_channel=[
            silence(48000, 0.05),
            silence(48000, 0.05),
        ],
    )

    # mono / 16-bit / 8000 Hz / 0.05s — ramp -1..+1 (full-range, exercises clipping)
    write_wav(
        wav_dir / "ramp_mono_16bit_8000.wav",
        sample_rate=8000,
        channels=1,
        bits_per_sample=16,
        samples_per_channel=[ramp(8000, 0.05)],
    )

    # MP3 fixtures (via ffmpeg, if available) for the Mp3Decoder test.
    mp3_dir = root / "mp3_decoder" / "input"
    mp3_files = encode_mp3_fixtures(mp3_dir)
    if mp3_files:
        emit_c_header(
            mp3_dir / "mp3_fixtures.h",
            mp3_files,
            namespace="Mp3Fixtures",
        )

    # FLAC fixtures (via ffmpeg, if available) for the FlacDecoder test.
    flac_dir = root / "flac_decoder" / "input"
    flac_files = encode_flac_fixtures(flac_dir)
    if flac_files:
        emit_c_header(
            flac_dir / "flac_fixtures.h",
            flac_files,
            namespace="FlacFixtures",
        )

    # Embedded-header companion (for in-memory tests on host + ESP32).
    wav_files = {
        "Sine440Mono8Bit22050":   wav_dir / "sine_440hz_mono_8bit_22050.wav",
        "Sine440Mono16Bit22050":  wav_dir / "sine_440hz_mono_16bit_22050.wav",
        "SineStereo16Bit44100":   wav_dir / "sine_stereo_16bit_44100.wav",
        "SilenceStereo16Bit48000": wav_dir / "silence_stereo_16bit_48000.wav",
        "RampMono16Bit8000":      wav_dir / "ramp_mono_16bit_8000.wav",
    }
    header_path = wav_dir / "wav_fixtures.h"
    emit_c_header(header_path, wav_files, namespace="WavFixtures")

    print(f"Wrote fixtures under: {wav_dir}")
    for p in sorted(wav_dir.glob("*.wav")):
        print(f"  {p.relative_to(root)}  ({p.stat().st_size} bytes)")
    print(f"  {header_path.relative_to(root)}  ({header_path.stat().st_size} bytes)")

    if mp3_files:
        mp3_header_path = mp3_dir / "mp3_fixtures.h"
        print(f"Wrote MP3 fixtures under: {mp3_dir}")
        for p in sorted(mp3_dir.glob("*.mp3")):
            print(f"  {p.relative_to(root)}  ({p.stat().st_size} bytes)")
        print(f"  {mp3_header_path.relative_to(root)}  ({mp3_header_path.stat().st_size} bytes)")

    if flac_files:
        flac_header_path = flac_dir / "flac_fixtures.h"
        print(f"Wrote FLAC fixtures under: {flac_dir}")
        for p in sorted(flac_dir.glob("*.flac")):
            print(f"  {p.relative_to(root)}  ({p.stat().st_size} bytes)")
        print(f"  {flac_header_path.relative_to(root)}  ({flac_header_path.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
