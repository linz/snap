#!/usr/bin/env python3
"""
build.py - SNAP build helper (cmake wrapper)

Usage:
    build.py [TYPE] [TARGET] [OPTIONS]

TYPE (default: release):
    release    Optimised build (-Ofast -mfma)
    debug      Debug build (-g, CHECKBLT enabled)
    profile    Profiling build (-O2 -g -pg)

TARGET (default: all):
    all        Build all targets
    snap_cmd   Build command-line tools only
    test       Build snap_cmd and the unit tests, and run the regression and unit tests
    install    Build all and install to system (release only, requires a
               clean git tree, so VERSIONID reflects what was actually built)
    package    Build a Debian package (Linux, release only; --build-dir sets SNAP_BUILD_DIR),
               or a Windows ZIP/NSIS package with --mingw (release only)
    clean      Remove the build directory
    tidy       Report what clang-tidy would change (the checks enabled in .clang-tidy), changing
               nothing; exits 1 if there are findings (debug only). Needs clang-tidy 21 (the
               version CI uses), wxWidgets 3.2 and the C++ build dependencies. Always configures
               with the GUI on, in build-tidy rather than build-debug (--build-dir overrides it);
               the full report is written there
    tidy-fix   Apply clang-tidy's fixes to the sources in place (debug only; requires a clean git
               tree), then run tidy to confirm that nothing is left. Same requirements as tidy
    iwyu       Report the #includes that include-what-you-use suggests adding or removing, after
               dropping the suggestions it gets wrong for this code base (debug only). A report
               only: it exits 0 whatever it finds. Needs the include-what-you-use built on
               clang 21 (the version CI uses; see BUILD.md), wxWidgets 3.2 and the C++ build
               dependencies, but not clang-tidy. Configures its own compile database in
               build-tidy, in the same way as tidy, and writes iwyu-report.txt (sorted by file)
               and iwyu-raw.txt there
    format     Report which sources clang-format would change (the style in .clang-format), changing
               nothing; exits 1 if there are any. Needs clang-format 21 (the version CI uses). The
               build type is ignored, as nothing is built
    format-fix Reformat the sources in place (requires a clean git tree), then run format to
               confirm that nothing is left. Same requirements as format

Options:
    --no-gui         Skip wxWidgets GUI targets (snap_manager, snapadjust, snapplot); not allowed
                     with tidy, tidy-fix or iwyu
    --jobs N         Parallel jobs (default: all cores)
    --build-dir DIR  Build output directory (default: build-{type}); for package,
                     sets SNAP_BUILD_DIR passed to debuild/debian/rules
    --no-efence      Do not link efence (debug builds link efence by default)
    --mingw          Cross-compile for Windows using MinGW-w64 (from Linux only;
                     release only). Requires BOOST_ROOT set to a MinGW-built Boost
                     tree, and (unless --no-gui) WX_MINGW_CONFIG set to a MinGW-built
                     wxWidgets wx-config script - see BUILD.md. Uses the
                     windows-mingw-release[-gui] CMake presets rather than the plain
                     -D flags used for the native Linux build; snap_cmd/test/install
                     targets are not supported with --mingw.
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import os
import platform
import re
import shutil
import subprocess
import sys
from collections import Counter
from collections.abc import Sequence
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent

SNAP_CMD_TARGETS = [
    "snap",
    "concord",
    "snapspec",
    "dat2site",
    "snaplist",
    "snapconv",
    "snapgeoid",
    "snapmerge",
    "site2gps",
]

BUILD_TYPE_MAP = {
    "release": "Release",
    "debug": "Debug",
    "profile": "Profile",
}

# Major version of clang-tidy and clang-format that the tidy, tidy-fix, format and format-fix targets
# require.  CI installs the same version, so a developer fixing a CI failure must match it (see
# BUILD.md).
REQUIRED_CLANG_VERSION = 21

TIDY_BUILD_DIR = REPO_ROOT / "build-tidy"

# clang-tidy runs on the files in the compile database, so only sources that the build compiles
# are analysed (src/wxtest has no CMake target, so it is not).  This regex selects from them the
# .cpp files under src/, leaving out the vendored third_party code and scriptimp_parse.cpp, which
# lapg generates from its grammar and every build overwrites.  It is anchored at REPO_ROOT so
# that tools/lapg/src is not matched.
TIDY_FILE_REGEX = rf"^{re.escape(str(REPO_ROOT))}/src/(?!third_party/)(?!.*/scriptimp_parse\.cpp$).*\.cpp$"


def build_dir(build_type: str) -> Path:
    """The default build directory for build_type, when --build-dir isn't given."""
    return REPO_ROOT / f"build-{build_type}"


def run(cmd: Sequence[str | Path], cwd: Path | None = None, env: dict[str, str] | None = None) -> None:
    """Runs cmd, echoing it first, and exits with its return code if it fails."""
    print("+", " ".join(str(part) for part in cmd))
    result = subprocess.run(cmd, cwd=cwd or REPO_ROOT, env=env, check=False)
    if result.returncode != 0:
        sys.exit(result.returncode)


def configure(build_type: str, no_gui: bool, efence: bool, build_d: Path) -> None:
    """Runs cmake's configure step for build_d with the given build options."""
    run(
        [
            "cmake",
            "-S",
            ".",
            "-B",
            build_d,
            f"-DCMAKE_BUILD_TYPE={BUILD_TYPE_MAP[build_type]}",
            f"-DSNAP_BUILD_GUI={'OFF' if no_gui else 'ON'}",
            f"-DSNAP_EFENCE={'ON' if efence else 'OFF'}",
        ]
    )


def touch_version_files() -> None:
    """Touches every translation unit defining GETVERSION_SET_PROGRAM_DATE, forcing the
    compiler to re-instantiate getversion.h's programDate (__DATE__ " " __TIME__) with
    today's date on every release build, keeping the "Version date:" output current."""
    result = subprocess.run(
        ["grep", "-rl", "GETVERSION_SET_PROGRAM_DATE", "src"],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    for path in result.stdout.splitlines():
        os.utime(REPO_ROOT / path, None)


def copy_config_files(build_d: Path, build_type: str) -> None:
    """Merges the per-component config directories from source into build_d's own
    config/ subdirectory, next to the executable (system_config_dir() =
    image_dir() + "/config"), so the build-tree executable is runnable."""
    config_dst = build_d / "src" / "config"
    config_dst.mkdir(parents=True, exist_ok=True)
    for src_subdir in ["snap/config", "snapspec/config", "snaplist/config", "snap_manager/config"]:
        src = REPO_ROOT / "src" / src_subdir
        if src.is_dir():
            shutil.copytree(src, config_dst, dirs_exist_ok=True)

    perl_src = REPO_ROOT / "src" / "perl"
    perl_dst = config_dst / "perl"
    if perl_src.is_dir():
        shutil.copytree(perl_src, perl_dst, dirs_exist_ok=True)

    # Unlike perl_src above, src/python also holds dev-only content (tests, tool
    # caches, dependency/lock files) that a runnable build doesn't need, so only
    # lib/ and the top-level scripts are copied, rather than the whole directory -
    # matching src/CMakeLists.txt's own install rules for the same reason.
    python_lib_src = REPO_ROOT / "src" / "python" / "lib"
    python_dst = config_dst / "python"
    if python_lib_src.is_dir():
        shutil.copytree(
            python_lib_src, python_dst / "lib", dirs_exist_ok=True, ignore=shutil.ignore_patterns("__pycache__")
        )
        for script in ("grid.py", "trig.py", "linzdeformationmodel.py"):
            shutil.copy2(REPO_ROOT / "src" / "python" / script, python_dst / script)

    version_src = REPO_ROOT / "VERSION"
    if version_src.is_file():
        shutil.copy2(version_src, build_d / "src" / "VERSION")

    help_src = REPO_ROOT / "src" / "help" / "help"
    help_dst = build_d / "src" / "help"
    if help_src.is_dir():
        shutil.copytree(help_src, help_dst, dirs_exist_ok=True)

    versionid = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"], cwd=REPO_ROOT, capture_output=True, text=True, check=False
    )
    if versionid.returncode == 0:
        (build_d / "src" / "VERSIONID").write_text(versionid.stdout.strip())

    if build_type == "debug":
        devel_src = REPO_ROOT / "src" / "packages" / "devel"
        devel_dst = config_dst / "package" / "devel"
        if devel_src.is_dir():
            shutil.copytree(devel_src, devel_dst, dirs_exist_ok=True)


def cmake_build(build_d: Path, targets: list[str] | None = None, jobs: int | None = None) -> None:
    """Runs cmake's build step for build_d, optionally restricted to targets."""
    cmd: list[str | Path] = ["cmake", "--build", build_d, "--parallel"]
    if jobs:
        cmd.append(str(jobs))
    if targets:
        cmd += ["--target", *targets]
    run(cmd)


def run_tests(build_type: str) -> None:
    """Runs testall.pl (the regression suites and the unit tests) against the already-built targets."""
    testall = REPO_ROOT / "regression_tests" / "testall.pl"
    cmd: list[str | Path] = ["perl", testall, "-e"]
    if build_type == "release":
        cmd.append("-r")
    run(cmd)


def mingw_build(args: argparse.Namespace) -> None:
    """Handles the --mingw cross-compile path: validates the host/type/target
    combination, checks the required MinGW dependency env vars, then configures,
    builds, and (for the package target) cpacks via the windows-mingw-release[-gui]
    presets."""
    if platform.system() != "Linux":
        print("ABORTED: --mingw cross-compiles for Windows and is only supported when run from Linux")
        sys.exit(1)
    if args.type != "release":
        print("ABORTED: --mingw only supports the release build type")
        sys.exit(1)
    if args.target not in ("all", "package", "clean"):
        print(
            f"ABORTED: --mingw does not support the '{args.target}' target "
            "(snap_cmd/test/install don't apply to a cross-compiled Windows build)"
        )
        sys.exit(1)

    preset = "windows-mingw-release" if args.no_gui else "windows-mingw-release-gui"
    build_d = REPO_ROOT / "build" / preset

    if args.target == "clean":
        if build_d.exists():
            print(f"Removing {build_d}")
            shutil.rmtree(build_d)
        return

    if not os.environ.get("BOOST_ROOT"):
        print("ABORTED: BOOST_ROOT must be set to a MinGW-built Boost tree - see BUILD.md")
        sys.exit(1)
    if not args.no_gui and not os.environ.get("WX_MINGW_CONFIG"):
        print(
            "ABORTED: WX_MINGW_CONFIG must be set to a MinGW-built wxWidgets "
            "wx-config script (or pass --no-gui) - see BUILD.md"
        )
        sys.exit(1)

    run(["cmake", "--preset", preset])
    build_cmd = ["cmake", "--build", "--preset", preset, "--parallel"]
    if args.jobs:
        build_cmd.append(str(args.jobs))
    run(build_cmd)

    if args.target == "package":
        run(["cpack"], cwd=build_d)


def clang_major_version(clang_tool: str) -> int | None:
    """The major version that clang_tool --version reports, or None if it can't be read.  clang-tidy
    prints "LLVM version 21.1.8" and clang-format "clang-format version 21.1.8"."""
    result = subprocess.run([clang_tool, "--version"], capture_output=True, text=True, check=False)
    match = re.search(r"(?:LLVM|clang-format) version (\d+)\.", result.stdout)
    return int(match.group(1)) if match else None


def find_clang_tidy() -> str:
    """Finds clang-tidy at REQUIRED_CLANG_VERSION, or aborts."""
    for name in (f"clang-tidy-{REQUIRED_CLANG_VERSION}", "clang-tidy"):
        path = shutil.which(name)
        if path and clang_major_version(path) == REQUIRED_CLANG_VERSION:
            return path
    print(f"ABORTED: clang-tidy {REQUIRED_CLANG_VERSION} is required - see BUILD.md")
    sys.exit(1)


def find_run_clang_tidy() -> str:
    """Finds the run-clang-tidy driver at REQUIRED_CLANG_VERSION, or aborts.  An unversioned driver can
    belong to a different LLVM, whose options and output differ."""
    path = shutil.which(f"run-clang-tidy-{REQUIRED_CLANG_VERSION}")
    if path:
        return path
    print(
        f"ABORTED: run-clang-tidy-{REQUIRED_CLANG_VERSION} (from the clang-tidy-{REQUIRED_CLANG_VERSION} "
        "package) is required - see BUILD.md"
    )
    sys.exit(1)


def configure_for_analysis(build_d: Path) -> None:
    """Runs cmake's configure step for build_d, writing the compile database that the static
    analysers (tidy, tidy-fix and iwyu) read.  Nothing is compiled, so the result depends only on the
    sources and CMakeLists.txt, not on file times or on what an earlier run left in build_d.

    Always the GUI build: it is the configuration that compiles the most source files."""
    run(
        [
            "cmake",
            "-S",
            ".",
            "-B",
            build_d,
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            "-DCMAKE_BUILD_TYPE=Debug",
            "-DSNAP_BUILD_GUI=ON",
            "-DSNAP_EFENCE=OFF",
        ]
    )


def _tidy(fix: bool, jobs: int | None, build_d: Path) -> None:
    """Runs clang-tidy over the C++ sources, with the checks enabled in .clang-tidy, writing the
    full report to build_d.  Only tidy_dry_run and tidy_fix call this.

    Without fix nothing is changed: it prints the number of findings per file and exits 1 if there
    are any.  With fix it rewrites the sources in place and exits 0.  In both cases, if clang-tidy
    itself fails (a compile error, a crash) it exits with clang-tidy's own return code."""
    clang_tidy = find_clang_tidy()
    run_clang_tidy = find_run_clang_tidy()

    configure_for_analysis(build_d)

    cmd: list[str | Path] = [run_clang_tidy, "-p", build_d, "-clang-tidy-binary", clang_tidy, "-quiet"]
    if jobs:
        cmd.append(f"-j{jobs}")
    if fix:
        cmd.append("-fix")
    # A machine with two GCC versions installed can make clang pick the one without libstdc++ headers.
    gcc_install_dir = os.environ.get("SNAP_TIDY_GCC_INSTALL_DIR")
    if gcc_install_dir:
        cmd.append(f"-extra-arg=--gcc-install-dir={gcc_install_dir}")
    cmd.append(TIDY_FILE_REGEX)

    report = build_d / "clang-tidy-report.txt"
    print("+", " ".join(str(part) for part in cmd), ">", report)
    with report.open("w") as report_file:
        result = subprocess.run(cmd, cwd=REPO_ROOT, stdout=report_file, stderr=subprocess.STDOUT, check=False)
    report_text = report.read_text()

    if result.returncode != 0:
        for line in [line for line in report_text.splitlines() if " error: " in line][:10]:
            print(line)
        print(f"clang-tidy failed with exit code {result.returncode}; see {report}")
        sys.exit(result.returncode)

    # A file regex that matches nothing would otherwise report "no findings".
    files_run = re.search(r"Running clang-tidy for (\d+) files out of (\d+)", report_text)
    if not files_run or int(files_run.group(1)) == 0:
        print(f"ABORTED: clang-tidy ran on no files, check TIDY_FILE_REGEX against the compile database; see {report}")
        sys.exit(1)
    print(f"clang-tidy ran on {files_run.group(1)} of the {files_run.group(2)} files in the compile database")

    if fix:
        print(f"clang-tidy fixes applied (see git diff --stat); full report in {report}")
        return

    findings = set(re.findall(r"^(.+?):(\d+):(\d+): warning: .*\[[\w.,-]+\]$", report_text, re.MULTILINE))
    findings_per_file = Counter(os.path.relpath(file, REPO_ROOT) for file, _line, _column in findings)
    for file, count in sorted(findings_per_file.items()):
        print(f"{count:6d}  {file}")
    if findings:
        print(f"clang-tidy found {len(findings)} findings in {len(findings_per_file)} files; full report in {report}")
        sys.exit(1)
    print("clang-tidy found no findings")


def tidy_dry_run(jobs: int | None, build_d: Path) -> None:
    """Reports what clang-tidy would change, without changing anything.  Exits 1 if there are findings."""
    _tidy(False, jobs, build_d)


def tidy_fix(jobs: int | None, build_d: Path) -> None:
    """Applies clang-tidy's fixes to the sources in place.  Aborts if there are uncommitted changes, so
    that the fixes are the only difference in git diff.  Run tidy_dry_run afterwards to confirm that
    nothing is left."""
    check_committed()
    _tidy(True, jobs, build_d)


def check_committed() -> None:
    """Aborts if the working tree has uncommitted changes."""
    result = subprocess.run(["git", "diff", "--quiet", "HEAD"], cwd=REPO_ROOT, check=False)
    if result.returncode != 0:
        print("ABORTED: current files not committed")
        sys.exit(1)


# Extensions of the C and C++ sources that the format and format-fix targets cover.
FORMAT_EXTENSIONS = (".c", ".cpp", ".h", ".hpp")


def find_clang_format() -> str:
    """Finds clang-format at REQUIRED_CLANG_VERSION, or aborts."""
    for name in (f"clang-format-{REQUIRED_CLANG_VERSION}", "clang-format"):
        path = shutil.which(name)
        if path and clang_major_version(path) == REQUIRED_CLANG_VERSION:
            return path
    print(f"ABORTED: clang-format {REQUIRED_CLANG_VERSION} is required - see BUILD.md")
    sys.exit(1)


def format_files() -> list[str]:
    """The tracked C and C++ sources under src/ that clang-format covers, as paths relative to
    REPO_ROOT: every one that git tracks there except those that .clang-format-ignore lists (the
    vendored third_party code, resource.h and the generated scriptimp_parse.cpp).  The ignore file is
    read here so that it stays the one list for clang-format, editors and this script."""
    tracked = subprocess.run(
        ["git", "ls-files", "-z", "--", "src"], cwd=REPO_ROOT, capture_output=True, text=True, check=True
    ).stdout.split("\0")
    patterns = [
        line.strip()
        for line in (REPO_ROOT / ".clang-format-ignore").read_text().splitlines()
        if line.strip() and not line.startswith("#")
    ]
    return [
        path
        for path in tracked
        if path.endswith(FORMAT_EXTENSIONS) and not any(fnmatch.fnmatch(path, pattern) for pattern in patterns)
    ]


def _format(fix: bool) -> None:
    """Runs clang-format over the sources that format_files() lists, in the style of .clang-format.

    Without fix nothing is changed: it prints the number of places clang-format would change in each
    file and exits 1 if there are any.  With fix it rewrites the sources in place and exits 0.  In both
    cases, if clang-format itself fails it exits with its own return code."""
    clang_format = find_clang_format()
    files = format_files()
    if not files:
        print("ABORTED: no sources to format, check format_files() against the git tree")
        sys.exit(1)

    mode = ["-i"] if fix else ["--dry-run", "-Werror"]
    result = subprocess.run(
        [clang_format, "--style=file", *mode, *files], cwd=REPO_ROOT, capture_output=True, text=True, check=False
    )

    violations = re.findall(r"^(.+?):\d+:\d+: error: code should be clang-formatted", result.stderr, re.MULTILINE)
    if result.returncode != 0 and (fix or not violations):
        print(result.stderr)
        print(f"clang-format failed with exit code {result.returncode}")
        sys.exit(result.returncode)

    if fix:
        print(f"clang-format applied to {len(files)} files (see git diff --stat)")
        return

    # clang-format reports each path as it was given, which is relative to REPO_ROOT, not to the current directory.
    for file, count in sorted(Counter((REPO_ROOT / path).relative_to(REPO_ROOT) for path in violations).items()):
        print(f"{count:6d}  {file}")
    if violations:
        print(f"clang-format would change {len(set(violations))} of {len(files)} files")
        sys.exit(1)
    print("clang-format would change nothing")


def format_dry_run() -> None:
    """Reports which sources clang-format would change, without changing anything.  Exits 1 if there
    are any."""
    _format(False)


def format_fix() -> None:
    """Reformats the sources in place.  Aborts if there are uncommitted changes, so that the
    formatting is the only difference in git diff.  Then runs format_dry_run to confirm that a second
    pass would change nothing."""
    check_committed()
    _format(True)
    _format(False)


# What the iwyu target writes into the build directory: everything that iwyu_tool.py printed, and the
# report left after the filters below.
IWYU_RAW_REPORT = "iwyu-raw.txt"
IWYU_REPORT = "iwyu-report.txt"

# No wxWidgets header or class is suggested.  The sources get wx from wx_includes.hpp, the project's
# umbrella header, so single wx headers would only duplicate it.  IWYU also suggests the header of the
# one wx port that it analysed (wx/gtk/frame.h for wxFrame), which does not exist on the other
# ports, so applying it would break the Windows build.
IWYU_WX = re.compile(r'#include [<"]wx/|class wx\w+;')

# The Boost headers that IWYU suggests despite boost-all.imp, which are implementation details.
IWYU_BOOST_INTERNAL = re.compile(r"#include <boost/(?:regex/v5|.*/detail)/")

# snapconfig.h is the configuration header that every source file includes first: it defines UNIX and
# the macros that stand in for the Windows names, which later headers test, so no use of a symbol in
# it is what IWYU can see.  wx headers are only suggested for removal because the platform header
# replaces them.  wx_includes.hpp is the project's wxWidgets umbrella header, which is kept for the
# same reason as the wx headers, instead of the many single wx headers that IWYU would put in its place.
IWYU_NEVER_REMOVE = re.compile(r'#include (?:<wx/|[<"](?:snapconfig\.(?:h|hpp)|wx_includes\.hpp)[>"])')

# After each include that it suggests adding, IWYU comments the symbols that the file uses from it:
#     add    #include "symmatrx.h"    // for ltmat, _SYMMATRX_H
# It counts an include guard (_SYMMATRX_H) among them, though that is no use of the header.  So a
# suggestion is dropped only when guards are all that the comment lists, as in "// for _SYMMATRX_H".
# One that also names a real symbol (ltmat), or whose list is cut short ("..."), is kept.
IWYU_GUARD_ONLY = re.compile(r"#include \S+\s+// for _[A-Z0-9_]+_H(?:, _[A-Z0-9_]+_H)*$")

# IWYU analyses the Linux build only, so in a file with a Windows branch it cannot see the includes
# that the other branch needs.
IWYU_PLATFORM_BRANCH = re.compile(r"WIN32|_MSC_VER|__MINGW32__")

# The header of each part of IWYU's output: "<file> should add these lines:" or "... remove ...".
IWYU_SECTION = re.compile(r"^(/.+?) should (add|remove) these lines:$")


def find_iwyu() -> Path:
    """Finds the include-what-you-use (IWYU) that is built on clang REQUIRED_CLANG_VERSION, or aborts.
    Each IWYU release is built for one clang, and its --version says which ("include-what-you-use 0.25
    (git:791e69e) based on Ubuntu clang version 21.1.8"), so that is what ties it to the clang that
    tidy and format require, and no IWYU version number is kept here.  IWYU_BINARY names it, as it
    does for iwyu_tool.py; otherwise it is the one on the PATH."""
    candidate = os.environ.get("IWYU_BINARY") or shutil.which("include-what-you-use")
    if candidate:
        result = subprocess.run([candidate, "--version"], capture_output=True, text=True, check=False)
        clang = re.search(r"include-what-you-use \d+\.\d+ .*? clang version (\d+)\.", result.stdout + result.stderr)
        if clang and int(clang.group(1)) == REQUIRED_CLANG_VERSION:
            return Path(candidate).resolve()
    print(f"ABORTED: include-what-you-use built on clang {REQUIRED_CLANG_VERSION} is required - see BUILD.md")
    sys.exit(1)


def parse_iwyu_output(output: str) -> dict[Path, set[tuple[str, str]]]:
    """The suggestions in IWYU's output, for each file, as pairs of "add" or "remove" and the line
    suggested.  A file that several sources include is reported once for each of them, so the
    suggestions are a set.  Each part of the output runs to the next blank line, which leaves out the
    full include-list that follows."""
    suggestions: dict[Path, set[tuple[str, str]]] = {}
    section: tuple[Path, str] | None = None
    for line in output.splitlines():
        header = IWYU_SECTION.match(line)
        if header:
            section = (Path(header.group(1)), header.group(2))
            suggestions.setdefault(section[0], set())
        elif not line.strip():
            section = None
        elif section:
            suggestions[section[0]].add((section[1], line.removeprefix("- ")))
    return suggestions


def wanted_iwyu_suggestions(path: Path, suggestions: set[tuple[str, str]]) -> list[tuple[str, str]]:
    """The suggestions for path that are left after dropping the ones that IWYU gets wrong about
    this code base (see IWYU_WX and the other IWYU_ patterns), in the order that the report lists
    them."""
    try:
        text = path.read_text(errors="replace")
    except OSError:
        text = None
    # A file that cannot be read cannot be checked for a Windows branch, so its removals are dropped.
    may_remove = text is not None and not IWYU_PLATFORM_BRANCH.search(text)
    wanted = []
    for kind, line in sorted(suggestions):
        if kind == "add":
            wrong_adds = (IWYU_WX, IWYU_BOOST_INTERNAL, IWYU_GUARD_ONLY)
            if any(wrong.match(line) for wrong in wrong_adds):
                continue
        elif not may_remove or IWYU_NEVER_REMOVE.match(line):
            continue
        wanted.append((kind, line))
    return wanted


def iwyu_report(jobs: int | None, build_d: Path) -> None:
    """Runs include-what-you-use over the C++ sources that tidy checks, with the Boost mapping that
    comes with it and the project's own, tools/iwyu/snap.imp, and writes the suggestions that remain
    after the filters to build_d.  The raw
    output is kept next to it.  This is a report only: it exits 0 whatever IWYU suggests, and exits
    with an error only if IWYU itself fails (a compile error) or analyses nothing."""
    iwyu = find_iwyu()
    iwyu_tool = iwyu.parent / "iwyu_tool.py"
    boost_mapping = iwyu.parent.parent / "share" / "include-what-you-use" / "boost-all.imp"
    snap_mapping = REPO_ROOT / "tools" / "iwyu" / "snap.imp"
    for needed in (iwyu_tool, boost_mapping, snap_mapping):
        if not needed.is_file():
            print(f"ABORTED: {needed} is missing - see BUILD.md")
            sys.exit(1)

    configure_for_analysis(build_d)
    database = json.loads((build_d / "compile_commands.json").read_text())
    files = sorted({entry["file"] for entry in database if re.match(TIDY_FILE_REGEX, entry["file"])})
    if not files:
        print("ABORTED: no sources to analyse, check TIDY_FILE_REGEX against the compile database")
        sys.exit(1)

    # iwyu_tool.py exits non-zero whenever IWYU has suggestions, so its return code says nothing here.
    cmd = [sys.executable, str(iwyu_tool), "-p", str(build_d), "-j", str(jobs or os.cpu_count() or 1)]
    cmd += [*files, "--"]
    for mapping in (boost_mapping, snap_mapping):
        cmd += ["-Xiwyu", f"--mapping_file={mapping}"]
    raw_report = build_d / IWYU_RAW_REPORT
    print("+", " ".join(cmd[:6]), f"<{len(files)} files> --", " ".join(cmd[-4:]), ">", raw_report)
    env = {**os.environ, "IWYU_BINARY": str(iwyu)}
    with raw_report.open("w") as raw_file:
        subprocess.run(cmd, cwd=REPO_ROOT, env=env, stdout=raw_file, stderr=subprocess.STDOUT, check=False)
    output = raw_report.read_text()

    # A compile error, or IWYU stopping on its own assertion (a mapping file that it does not accept).
    errors = [line for line in output.splitlines() if " error: " in line or "Assertion failed" in line]
    if errors:
        for line in errors[:10]:
            print(line)
        print(f"include-what-you-use failed with {len(errors)} errors; see {raw_report}")
        sys.exit(1)
    suggestions = parse_iwyu_output(output)
    if not suggestions and "has correct #includes/fwd-decls" not in output:
        print(f"ABORTED: include-what-you-use analysed no files; see {raw_report}")
        sys.exit(1)

    report: list[str] = []
    counts = Counter[str]()
    for path in sorted(suggestions):
        wanted = wanted_iwyu_suggestions(path, suggestions[path])
        if wanted:
            report.append(str(path.relative_to(REPO_ROOT) if path.is_relative_to(REPO_ROOT) else path))
            report += [f"    {kind:<7}{line}" for kind, line in wanted]
            counts.update(kind for kind, _line in wanted)
    report_path = build_d / IWYU_REPORT
    report_path.write_text("\n".join(report) + "\n")
    print(
        f"include-what-you-use suggests {counts['add']} additions and {counts['remove']} removals, "
        f"after the filters, in {sum(not line.startswith(' ') for line in report)} files (the {len(files)} "
        f"sources analysed and the headers they include); full report in {report_path}"
    )


def check_version_changelog() -> None:
    """Aborts if debian/changelog's latest entry doesn't match VERSION."""
    version = (REPO_ROOT / "VERSION").read_text().strip()
    with (REPO_ROOT / "debian" / "changelog").open() as f:
        first_line = f.readline()
    if f"({version}-" not in first_line:
        print(f"ABORTED: changelog version does not match {version}")
        sys.exit(1)


def main() -> None:  # pylint: disable=too-many-branches
    """Parses arguments and dispatches to the requested build type/target."""
    parser = argparse.ArgumentParser(
        description="SNAP build helper", formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__
    )
    parser.add_argument(
        "type", nargs="?", default="release", choices=list(BUILD_TYPE_MAP), help="Build type (default: release)"
    )
    parser.add_argument(
        "target",
        nargs="?",
        default="all",
        choices=[
            "all",
            "snap_cmd",
            "test",
            "install",
            "package",
            "clean",
            "tidy",
            "tidy-fix",
            "iwyu",
            "format",
            "format-fix",
        ],
        help="Build target (default: all)",
    )
    parser.add_argument(
        "--no-gui", action="store_true", help="Skip wxWidgets GUI targets (snap_manager, snapadjust, snapplot)"
    )
    parser.add_argument("--jobs", type=int, metavar="N", help="Parallel build jobs (default: all cores)")
    parser.add_argument(
        "--build-dir", type=Path, metavar="DIR", default=None, help="Build output directory (default: build-{type})"
    )
    parser.add_argument(
        "--no-efence", action="store_true", help="Do not link efence (debug builds link efence by default)"
    )
    parser.add_argument(
        "--mingw",
        action="store_true",
        help="Cross-compile for Windows using MinGW-w64 (Linux host, release only) - see BUILD.md",
    )
    args = parser.parse_args()

    if args.mingw:
        mingw_build(args)
        return

    build_d = args.build_dir if args.build_dir else build_dir(args.type)
    efence = (args.type == "debug") and not args.no_efence

    if args.target == "clean":
        if build_d.exists():
            print(f"Removing {build_d}")
            shutil.rmtree(build_d)
        return

    if args.target in ("format", "format-fix"):
        if args.target == "format":
            format_dry_run()
        else:
            format_fix()
        return

    if args.target in ("tidy", "tidy-fix", "iwyu"):
        if args.type != "debug":
            print(f"{args.target} target only supported for debug builds")
            sys.exit(1)
        if args.no_gui:
            print(f"ABORTED: {args.target} always configures with the GUI on, so that the GUI sources are checked")
            sys.exit(1)
        tidy_build_d = args.build_dir if args.build_dir else TIDY_BUILD_DIR
        if args.target == "tidy":
            tidy_dry_run(args.jobs, tidy_build_d)
        elif args.target == "tidy-fix":
            tidy_fix(args.jobs, tidy_build_d)
        else:
            iwyu_report(args.jobs, tidy_build_d)
        return

    if args.target == "package":
        if args.type != "release":
            print("package target only supported for release builds")
            sys.exit(1)
        if platform.system() != "Linux":
            print("package target only supported on Linux")
            sys.exit(1)
        check_committed()
        check_version_changelog()
        env = os.environ.copy()
        if args.build_dir:
            env["SNAP_BUILD_DIR"] = str(args.build_dir)
        if args.no_gui:
            env["SNAP_BUILD_GUI"] = "OFF"
        run(["debuild", "--check-dirname-level=0", "-uc", "-us", "-b"], env=env)
        return

    if args.target == "install":
        if args.type != "release":
            print("install target only supported for release builds")
            sys.exit(1)
        check_committed()

    configure(args.type, args.no_gui, efence, build_d)

    if args.type == "release":
        touch_version_files()

    if args.target == "all":
        cmake_build(build_d, jobs=args.jobs)
        copy_config_files(build_d, args.type)
    elif args.target == "snap_cmd":
        cmake_build(build_d, targets=SNAP_CMD_TARGETS, jobs=args.jobs)
        copy_config_files(build_d, args.type)
    elif args.target == "test":
        cmake_build(build_d, targets=SNAP_CMD_TARGETS + ["binroundtrip", "unit_tests"], jobs=args.jobs)
        copy_config_files(build_d, args.type)
        run_tests(args.type)
    elif args.target == "install":
        cmake_build(build_d, jobs=args.jobs)
        run(["cmake", "--install", build_d])


if __name__ == "__main__":
    main()
