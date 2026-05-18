"""PCMConvert unit tests.

Sketch prints `PASS <name>` / `FAIL <name> ...` per assertion and a final
`TEST done <pass>/<total>` line.
"""

import re


def test_convert(dut):
    dut.expect("TEST start", timeout=10)
    match = dut.expect(re.compile(rb"TEST done (\d+)/(\d+)"), timeout=30)
    passed, total = int(match.group(1)), int(match.group(2))
    assert passed == total, f"{total - passed} of {total} assertions failed"
    assert total > 0, "no assertions ran"
