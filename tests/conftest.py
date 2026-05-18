"""Shared pytest hooks for PCMFlow tests."""

import shutil
from pathlib import Path


def pytest_runtest_setup(item):
    """Remove the per-test `output/` directory before each test runs.

    Host-profile sketches can write artifacts (WAV files, dumps, etc.) via
    `fopen("output/...", ...)`. The CWD at run time is the sketch directory,
    so the files land at `tests/<name>/output/`. That folder is gitignored
    and intentionally left in place after a run so the user can inspect it;
    this hook wipes it before the next run for a clean slate.

    This is implemented as a hook (rather than an autouse fixture) so it
    runs before the pytest-embedded `dut` fixture builds and launches the
    sketch — otherwise the cleanup would race the sketch's write.
    """
    output_dir = Path(item.fspath).parent / "output"
    if output_dir.exists():
        shutil.rmtree(output_dir)
