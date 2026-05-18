"""PCMRingBuffer unit tests.

The sketch prints per-assertion `PASS <name>` / `FAIL <name> ...` lines and a
final `TEST done <pass>/<total>` line. The test passes only when no FAIL line
appears and the totals match.
"""

import re

import pytest


def test_ringbuffer(dut):
    dut.expect("TEST start", timeout=10)
    match = dut.expect(re.compile(rb"TEST done (\d+)/(\d+)"), timeout=30)
    passed, total = int(match.group(1)), int(match.group(2))
    assert passed == total, f"{total - passed} of {total} assertions failed"
    assert total > 0, "no assertions ran"
