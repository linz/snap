#!/usr/bin/env python3
"""
Verifies the .bin format round-trips byte-for-byte. See BINFILE_FORMAT.md.

Two comparisons against a committed golden copy, not one:

  1. Fresh `snap` output vs golden, via binroundtrip --dump on each side -
     catches a regression, or snap doing something new/system-dependent
     on this platform/build. Compared with tolerance (compare_dump.py),
     not a raw byte cmp: a cross-compiler build can legitimately differ
     in low-order floating-point digits on genuinely computed fields
     without that being a real regression.
  2. `binroundtrip` output (reload then re-dump the golden copy) vs
     golden - proves the read and write paths agree with each other.
     A same-platform self-consistency check, so this one stays a plain
     byte cmp - there's no floating-point recomputation involved, just
     a reload and rewrite of values already on disk.

Both use SNAP_TEST_FIXED_DATE so comparison 2's byte diff has no
timestamp field to mask.
"""

import argparse
import contextlib
import filecmp
import os
import shutil
import subprocess
import sys
import tempfile
from collections.abc import Iterator

import compare_dump

HERE = os.path.dirname(os.path.abspath(__file__))
IN_DIR = os.path.join(HERE, "in")
GOLDEN = os.path.join(IN_DIR, "binroundtrip.bin")
COORDSYSDEF = os.path.join(HERE, "..", "test_coordsys", "coordsys.def")

# Default build output directory, per BUILD.md: Linux's build.py places a
# release build in build-release/; the Windows CMake preset places one in
# build/windows-release/ instead. Override with SNAP_BUILD_DIR if needed.
if sys.platform == "win32":
    _DEFAULT_BUILD_DIR = os.path.join(HERE, "..", "..", "build", "windows-release", "src")
    EXE_SUFFIX = ".exe"
else:
    _DEFAULT_BUILD_DIR = os.path.join(HERE, "..", "..", "build-release", "src")
    EXE_SUFFIX = ""
BUILD_DIR = os.environ.get("SNAP_BUILD_DIR", _DEFAULT_BUILD_DIR)


def run_env() -> dict[str, str]:
    """Returns the environment used for both snap and binroundtrip."""
    env = dict(os.environ)
    env["COORDSYSDEF"] = COORDSYSDEF
    env["SNAP_TEST_FIXED_DATE"] = "1-JAN-2000 00:00:00"
    return env


def run(args: list[str], cwd: str, env: dict[str, str]) -> subprocess.CompletedProcess:
    """Runs a command, capturing combined stdout/stderr as text."""
    return subprocess.run(
        args, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, check=False,
    )


def fail(message: str, result: subprocess.CompletedProcess | None = None) -> int:
    """Prints a failure message (and captured command output, if any) and returns 1."""
    print(f"FAIL: {message}", file=sys.stderr)
    if result is not None:
        print(result.stdout, file=sys.stderr)
    return 1


def _check_fresh_snap_output(
    snap: str, binroundtrip: str, work: str, env: dict[str, str]
) -> bool:
    """Comparison 1: runs snap fresh, dumps it and the golden .bin, and
    compares them with tolerance. Returns whether they match beyond
    floating-point noise; prints a failure message first if not.
    """
    result = run([snap, "binroundtrip.snp"], work, env)
    fresh_bin = os.path.join(work, "binroundtrip.bin")
    if not os.path.isfile(fresh_bin):
        fail("snap did not produce binroundtrip.bin", result)
        return False

    fresh_dump = os.path.join(work, "fresh_dump.txt")
    result = run([binroundtrip, "--dump", fresh_bin, fresh_dump], work, env)
    if result.returncode != 0:
        fail("binroundtrip failed to dump the fresh .bin", result)
        return False

    golden_dump = os.path.join(work, "golden_dump.txt")
    result = run([binroundtrip, "--dump", GOLDEN, golden_dump], work, env)
    if result.returncode != 0:
        fail("binroundtrip failed to dump the golden .bin", result)
        return False

    with open(golden_dump, encoding="utf-8") as f:
        golden_lines = f.read().splitlines()
    with open(fresh_dump, encoding="utf-8") as f:
        fresh_lines = f.read().splitlines()

    comparison = compare_dump.compare_dumps(golden_lines, fresh_lines)
    if not comparison.mismatches and not comparison.suppressed:
        return True

    message = (
        "fresh snap output differs from the golden .bin beyond "
        "floating-point noise - snap is behaving differently on "
        "this platform/build, or a real regression was introduced:\n"
        + "\n".join(comparison.mismatches)
    )
    if comparison.suppressed:
        message += f"\n... {comparison.suppressed} further difference(s) suppressed"
    fail(message)
    return False


def _check_round_trip(binroundtrip: str, work: str, env: dict[str, str]) -> bool:
    """Comparison 2: reloads and re-dumps the golden .bin, and byte-compares
    the result to the golden copy. Returns whether they're identical;
    prints a failure message first if not.
    """
    roundtrip_out = os.path.join(work, "roundtrip_out.bin")
    result = run([binroundtrip, GOLDEN, roundtrip_out], work, env)
    if result.returncode != 0:
        fail("binroundtrip failed to reload/re-dump the golden .bin", result)
        return False

    if filecmp.cmp(roundtrip_out, GOLDEN, shallow=False):
        return True

    fail(
        "round-tripped .bin differs from the golden copy - the read "
        "and write paths disagree with each other"
    )
    return False


@contextlib.contextmanager
def _work_dir(specified: str | None) -> Iterator[str]:
    """Yields the directory to run both checks in.

    With a caller-specified path, that directory is created if needed and
    left in place afterward - its contents (the freshly built .bin, both
    dumps, the round-tripped .bin) are exactly what a --work-dir call is
    for inspecting, so nothing here should delete them. Without one, an
    ephemeral temporary directory is used and cleaned up automatically, as
    before.
    """
    if specified:
        os.makedirs(specified, exist_ok=True)
        yield specified
    else:
        with tempfile.TemporaryDirectory() as tmp:
            yield tmp


def main(work_dir: str | None = None) -> int:
    """Runs both comparisons against the golden .bin, returning a process exit code.

    Both checks always run, even if the first fails - otherwise a fresh-
    output mismatch would hide whether the round trip itself, the actual
    goal of this test, still passes.
    """
    if not os.path.isfile(GOLDEN):
        return fail(f"missing golden fixture: {GOLDEN}")

    env = run_env()
    snap = os.path.join(BUILD_DIR, "snap" + EXE_SUFFIX)
    binroundtrip = os.path.join(BUILD_DIR, "binroundtrip" + EXE_SUFFIX)

    with _work_dir(work_dir) as work:
        for name in ("binroundtrip.snp", "binroundtrip.crd", "binroundtrip.dat"):
            shutil.copy(os.path.join(IN_DIR, name), work)

        fresh_output_ok = _check_fresh_snap_output(snap, binroundtrip, work, env)
        round_trip_ok = _check_round_trip(binroundtrip, work, env)

    if fresh_output_ok and round_trip_ok:
        print("binroundtrip: PASS")
        return 0

    failed = []
    if not fresh_output_ok:
        failed.append("fresh snap output vs golden")
    if not round_trip_ok:
        failed.append("round trip vs golden")
    return fail("binroundtrip: FAIL (" + ", ".join(failed) + ")")


def _parse_args() -> argparse.Namespace:
    """Parses command-line arguments. testall.pl invokes this script with
    none, so --work-dir only matters when running it directly by hand.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--work-dir",
        help="Directory to run both checks in and leave the results in "
        "(created if needed) - the freshly built .bin, both dumps, and "
        "the round-tripped .bin. Without this, an ephemeral temporary "
        "directory is used and deleted afterward.",
    )
    return parser.parse_args()


if __name__ == "__main__":
    sys.exit(main(_parse_args().work_dir))
