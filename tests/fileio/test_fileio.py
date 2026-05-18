"""Probe: where does host-profile fopen() write?

The sketch writes to `output/pcmflow_fileio_probe.bin` (relative). The
empirically observed CWD is the sketch directory, so the file should
land at tests/fileio/output/. The `output/` folder is gitignored — the
test does NOT delete the file, so it can be inspected afterwards.
"""

from pathlib import Path


PROBE_REL = "output/pcmflow_fileio_probe.bin"
EXPECTED_PAYLOAD = b"PCMFLOW!"


def test_fopen_writes_under_sketch_output(dut):
    sketch_dir = Path(__file__).parent
    expected_path = sketch_dir / PROBE_REL

    dut.expect("TEST start", timeout=10)
    dut.expect(f"WROTE bytes=8 file={PROBE_REL}", timeout=10)
    dut.expect("TEST done", timeout=10)

    assert expected_path.exists(), f"probe file not found at {expected_path}"
    data = expected_path.read_bytes()
    assert data == EXPECTED_PAYLOAD, f"unexpected payload: {data!r}"
