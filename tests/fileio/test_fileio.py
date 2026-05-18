"""Probes: where does host-profile filesystem I/O land?

Two mechanisms are checked:

  1. `fopen("rel/path", ...)` — resolves against the sketch directory.
     Files end up at tests/fileio/<rel-path>.

  2. `fs::FS SD("SD")` (lang-ship host mock) — resolves against the
     directory of the running .out executable. Files end up at
     tests/fileio/build/host/SD/<abs-path>.

Both paths are gitignored. The test leaves the files in place after the
run so users can poke at them; conftest.py wipes `output/` before the
next run, and the build dir is rebuilt each run.
"""

from pathlib import Path


FOPEN_REL = "output/pcmflow_fileio_probe.bin"
FOPEN_PAYLOAD = b"PCMFLOW!"

SD_REL = "build/host/SD/pcmflow_sd_probe.bin"
SD_PAYLOAD = b"SD-PROBE"


def test_fopen_and_sd_round_trip(dut):
    sketch_dir = Path(__file__).parent

    dut.expect("TEST start", timeout=10)
    dut.expect(f"WROTE bytes=8 file={FOPEN_REL}", timeout=10)
    dut.expect("SD WROTE bytes=8 path=/pcmflow_sd_probe.bin", timeout=10)
    dut.expect("SD READ bytes=8 payload=SD-PROBE", timeout=10)
    dut.expect("TEST done", timeout=10)

    fopen_path = sketch_dir / FOPEN_REL
    assert fopen_path.exists(), f"fopen probe not found at {fopen_path}"
    assert fopen_path.read_bytes() == FOPEN_PAYLOAD

    sd_path = sketch_dir / SD_REL
    assert sd_path.exists(), f"SD probe not found at {sd_path}"
    assert sd_path.read_bytes() == SD_PAYLOAD
