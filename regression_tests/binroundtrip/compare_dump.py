"""
Tolerance-aware comparison of two binroundtrip --dump text outputs.

Each dump line is one of three shapes (binroundtrip.cpp): a "label = value"
line (dump_value - SNAP_GLOBALS, Network, STNADJ, OBS_CLASSES), a bare
value with no label (dump_bare_value - the table-driven sections and
OBSERVATIONS), or a "=== section[i] ===" marker. There's no single place
to consult a field's real type, so every line is split on " = " first
(markers and bare values have no " = ", so this is a no-op for them): the
label part, if any, must match exactly; the value part is compared with
checktests.pl's own numeric tolerance (its reltol/abstol defaults - not
its extra text-precision heuristics, which exist for lower-precision
formatted list-file output, not this dump's full-precision doubles) if it
parses as a float on both sides, otherwise it also needs an exact match.
"""

from typing import NamedTuple

# checktests.pl:256-257 - the same defaults every other regression test
# tolerates floating-point noise with, kept in sync deliberately rather
# than duplicated by coincidence.
ABSTOL = 1.0e-8
RELTOL = 1.0e-10

MAX_REPORTED_MISMATCHES = 20


def _split_label(line: str) -> tuple[str, str]:
    """Splits a "label = value" line on its first " = "; ("", line) if there's none."""
    label, sep, value = line.partition(" = ")
    if not sep:
        return "", line
    return label, value


def _try_float(text: str) -> float | None:
    """Returns text parsed as a float, or None if it isn't one."""
    try:
        return float(text)
    except ValueError:
        return None


def _values_close(expected: float, actual: float) -> bool:
    """True if actual is within tolerance of expected (checktests.pl's formula)."""
    diff = abs(actual - expected)
    return diff <= ABSTOL or diff <= abs(expected) * RELTOL


def _lines_match(expected: str, actual: str) -> bool:
    """True if two dump lines are equivalent (exact, or within tolerance for a numeric value)."""
    if expected == actual:
        return True
    expected_label, expected_value = _split_label(expected)
    actual_label, actual_value = _split_label(actual)
    if expected_label != actual_label:
        return False
    expected_float = _try_float(expected_value)
    actual_float = _try_float(actual_value)
    if expected_float is not None and actual_float is not None:
        return _values_close(expected_float, actual_float)
    return expected_value == actual_value


class ComparisonResult(NamedTuple):
    """mismatches: up to MAX_REPORTED_MISMATCHES human-readable descriptions.
    suppressed: how many further mismatches were found beyond that cap -
    kept separate from `mismatches` rather than folded into it as a string,
    so a caller can report both without parsing text back out of a message.
    """

    mismatches: list[str]
    suppressed: int


def compare_dumps(expected_lines: list[str], actual_lines: list[str]) -> ComparisonResult:
    """Compares two dumps line-by-line; both lists empty/zero if they're equivalent.

    Mismatches beyond MAX_REPORTED_MISMATCHES are counted in `suppressed`
    but not individually listed, so a structural divergence (e.g. a
    differing record count) can't silently flood the report while also
    not silently hiding its scale.
    """
    mismatches = []
    if len(expected_lines) != len(actual_lines):
        mismatches.append(
            f"line count differs: expected {len(expected_lines)} lines, got {len(actual_lines)}"
        )

    suppressed = 0
    for i, (expected, actual) in enumerate(zip(expected_lines, actual_lines), start=1):
        if _lines_match(expected, actual):
            continue
        if len(mismatches) >= MAX_REPORTED_MISMATCHES:
            suppressed += 1
            continue
        mismatches.append(f"line {i}: expected {expected!r}, got {actual!r}")

    return ComparisonResult(mismatches, suppressed)
