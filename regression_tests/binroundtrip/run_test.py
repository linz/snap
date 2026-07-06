#!/usr/bin/env python3
"""
Verifies the .bin format round-trips byte-for-byte. See BINFILE_FORMAT.md.

Two comparisons against a committed golden copy, not one:

  1. Fresh `snap` output vs golden - catches a regression, or snap
     doing something new/system-dependent on this platform/build.
  2. `binroundtrip` output (reload then re-dump the golden copy) vs
     golden - proves the read and write paths agree with each other.

Both use SNAP_TEST_FIXED_DATE so the comparison is a plain byte diff,
with no need to mask a timestamp field.
"""

import filecmp
import os
import shutil
import subprocess
import sys
import tempfile

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


def main() -> int:
    """Runs both comparisons against the golden .bin, returning a process exit code."""
    if not os.path.isfile(GOLDEN):
        return fail(f"missing golden fixture: {GOLDEN}")

    env = run_env()
    snap = os.path.join(BUILD_DIR, "snap" + EXE_SUFFIX)
    binroundtrip = os.path.join(BUILD_DIR, "binroundtrip" + EXE_SUFFIX)

    with tempfile.TemporaryDirectory() as work:
        for name in ("binroundtrip.snp", "binroundtrip.crd", "binroundtrip.dat"):
            shutil.copy(os.path.join(IN_DIR, name), work)

        result = run([snap, "binroundtrip.snp"], work, env)
        fresh_bin = os.path.join(work, "binroundtrip.bin")
        if not os.path.isfile(fresh_bin):
            return fail("snap did not produce binroundtrip.bin", result)

        if not filecmp.cmp(fresh_bin, GOLDEN, shallow=False):
            return fail(
                "fresh snap output differs from the golden .bin - snap is "
                "behaving differently on this platform/build, or a real "
                "regression was introduced"
            )

        roundtrip_out = os.path.join(work, "roundtrip_out.bin")
        result = run([binroundtrip, GOLDEN, roundtrip_out], work, env)
        if result.returncode != 0:
            return fail("binroundtrip failed to reload/re-dump the golden .bin", result)

        if not filecmp.cmp(roundtrip_out, GOLDEN, shallow=False):
            return fail(
                "round-tripped .bin differs from the golden copy - the read "
                "and write paths disagree with each other"
            )

    print("binroundtrip: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
