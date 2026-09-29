#!/usr/bin/env python3
"""
Runs the standalone C++ unit-test executables built alongside snap (the
*_test targets in src/CMakeLists.txt, gathered by its unit_tests target).

Each executable reports pass/fail through its exit code. They are found by
globbing the build directory rather than from a fixed list, so a new
*_test target is picked up without touching this script. Finding none is a
failure, not a pass - it means the unit_tests target wasn't built.
"""

import glob
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))

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


def find_unit_tests() -> list[str]:
    """Returns the paths of the unit-test executables in the build directory, sorted by name."""
    return sorted(glob.glob(os.path.join(BUILD_DIR, "*_test" + EXE_SUFFIX)))


def run_unit_test(executable: str, work: str) -> bool:
    """Runs one unit-test executable in the work directory. Returns whether
    it passed; prints its output first if it did not.
    """
    result = subprocess.run(
        [executable], cwd=work, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, check=False,
    )
    name = os.path.basename(executable)
    if result.returncode == 0:
        print(f"{name}: PASS")
        return True
    print(f"{name}: FAIL (exit code {result.returncode})", file=sys.stderr)
    print(result.stdout, file=sys.stderr)
    return False


def main() -> int:
    """Runs every unit test, returning a process exit code.

    All tests run even if an earlier one fails, so one failure doesn't hide
    others.
    """
    executables = find_unit_tests()
    if not executables:
        print(f"FAIL: no unit-test executables found in {BUILD_DIR}", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory() as work:
        results = [run_unit_test(executable, work) for executable in executables]

    failed = results.count(False)
    if failed:
        print(f"unittests: FAIL ({failed} of {len(results)} failed)", file=sys.stderr)
        return 1
    print(f"unittests: PASS ({len(results)} unit tests)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
