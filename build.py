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
    test       Build snap_cmd and run regression tests
    install    Build all and install to system (release only, requires a
               clean git tree, so VERSIONID reflects what was actually built)
    package    Build a Debian package (Linux, release only; --build-dir sets SNAP_BUILD_DIR),
               or a Windows ZIP/NSIS package with --mingw (release only)
    clean      Remove the build directory

Options:
    --no-gui         Skip wxWidgets GUI targets (snap_manager, snapadjust, snapplot)
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
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent

SNAP_CMD_TARGETS = [
    "snap", "concord", "snapspec", "dat2site",
    "snaplist", "snapconv", "snapgeoid", "snapmerge", "site2gps",
]

BUILD_TYPE_MAP = {
    "release": "Release",
    "debug":   "Debug",
    "profile": "Profile",
}


def build_dir(build_type: str) -> Path:
    return REPO_ROOT / f"build-{build_type}"


def run(cmd: list[str | Path], cwd: Path | None = None, env: dict | None = None) -> None:
    print("+", " ".join(str(part) for part in cmd))
    result = subprocess.run(cmd, cwd=cwd or REPO_ROOT, env=env, check=False)
    if result.returncode != 0:
        sys.exit(result.returncode)


def configure(build_type: str, no_gui: bool, efence: bool, build_d: Path) -> None:
    run([
        "cmake", "-S", ".", "-B", build_d,
        f"-DCMAKE_BUILD_TYPE={BUILD_TYPE_MAP[build_type]}",
        f"-DSNAP_BUILD_GUI={'OFF' if no_gui else 'ON'}",
        f"-DSNAP_EFENCE={'ON' if efence else 'OFF'}",
    ])


def touch_version_files() -> None:
    # Each executable has one translation unit that defines GETVERSION_SET_PROGRAM_DATE,
    # which causes getversion.h to instantiate programDate = __DATE__ " " __TIME__.
    # Touching those files forces the compiler to re-instantiate programDate with
    # today's date on every release build, keeping the "Version date:" output current.
    result = subprocess.run(
        ["grep", "-rl", "GETVERSION_SET_PROGRAM_DATE", "src"],
        cwd=REPO_ROOT, capture_output=True, text=True, check=False)
    for path in result.stdout.splitlines():
        os.utime(REPO_ROOT / path, None)


def copy_config_files(build_d: Path, build_type: str) -> None:
    # snap locates config files in a config/ subdirectory next to the executable
    # (system_config_dir() = image_dir() + "/config").  Merge the per-component
    # config directories from source so the build-tree executable is runnable.
    config_dst = build_d / "src" / "config"
    config_dst.mkdir(parents=True, exist_ok=True)
    for src_subdir in ["snap/config", "snapspec/config", "snaplist/config",
                       "snap_manager/config"]:
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
        shutil.copytree(python_lib_src, python_dst / "lib", dirs_exist_ok=True,
                         ignore=shutil.ignore_patterns("__pycache__"))
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
        ["git", "rev-parse", "--short", "HEAD"],
        cwd=REPO_ROOT, capture_output=True, text=True, check=False)
    if versionid.returncode == 0:
        (build_d / "src" / "VERSIONID").write_text(versionid.stdout.strip())

    if build_type == "debug":
        devel_src = REPO_ROOT / "src" / "packages" / "devel"
        devel_dst = config_dst / "package" / "devel"
        if devel_src.is_dir():
            shutil.copytree(devel_src, devel_dst, dirs_exist_ok=True)


def cmake_build(build_d: Path, targets: list[str] | None = None, jobs: int | None = None) -> None:
    cmd: list[str | Path] = (
        ["cmake", "--build", build_d, "--parallel"]
        + ([str(jobs)] if jobs else [])
        + (["--target"] + targets if targets else [])
    )
    run(cmd)


def run_tests(build_type: str) -> None:
    testall = REPO_ROOT / "regression_tests" / "testall.pl"
    run(["perl", testall, "-e"] + (["-r"] if build_type == "release" else []))


def mingw_build(args) -> None:
    if platform.system() != "Linux":
        print("ABORTED: --mingw cross-compiles for Windows and is only supported "
              "when run from Linux")
        sys.exit(1)
    if args.type != "release":
        print("ABORTED: --mingw only supports the release build type")
        sys.exit(1)
    if args.target not in ("all", "package", "clean"):
        print(f"ABORTED: --mingw does not support the '{args.target}' target "
              "(snap_cmd/test/install don't apply to a cross-compiled Windows build)")
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
        print("ABORTED: WX_MINGW_CONFIG must be set to a MinGW-built wxWidgets "
              "wx-config script (or pass --no-gui) - see BUILD.md")
        sys.exit(1)

    run(["cmake", "--preset", preset])
    build_cmd = ["cmake", "--build", "--preset", preset]
    if args.jobs:
        build_cmd += ["--parallel", str(args.jobs)]
    run(build_cmd)

    if args.target == "package":
        run(["cpack"], cwd=build_d)


def check_committed() -> None:
    result = subprocess.run(["git", "diff", "--quiet", "HEAD"], cwd=REPO_ROOT, check=False)
    if result.returncode != 0:
        print("ABORTED: current files not committed")
        sys.exit(1)


def check_version_changelog() -> None:
    version = (REPO_ROOT / "VERSION").read_text().strip()
    with (REPO_ROOT / "debian" / "changelog").open() as f:
        first_line = f.readline()
    if f"({version}-" not in first_line:
        print(f"ABORTED: changelog version does not match {version}")
        sys.exit(1)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="SNAP build helper",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)
    parser.add_argument("type", nargs="?", default="release",
                        choices=list(BUILD_TYPE_MAP),
                        help="Build type (default: release)")
    parser.add_argument("target", nargs="?", default="all",
                        choices=["all", "snap_cmd", "test", "install", "package", "clean"],
                        help="Build target (default: all)")
    parser.add_argument("--no-gui", action="store_true",
                        help="Skip wxWidgets GUI targets (snap_manager, snapadjust, snapplot)")
    parser.add_argument("--jobs", type=int, metavar="N",
                        help="Parallel build jobs (default: all cores)")
    parser.add_argument("--build-dir", type=Path, metavar="DIR", default=None,
                        help="Build output directory (default: build-{type})")
    parser.add_argument("--no-efence", action="store_true",
                        help="Do not link efence (debug builds link efence by default)")
    parser.add_argument("--mingw", action="store_true",
                        help="Cross-compile for Windows using MinGW-w64 (Linux host, "
                             "release only) - see BUILD.md")
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
        cmake_build(build_d, targets=SNAP_CMD_TARGETS + ["binroundtrip"], jobs=args.jobs)
        copy_config_files(build_d, args.type)
        run_tests(args.type)
    elif args.target == "install":
        cmake_build(build_d, jobs=args.jobs)
        run(["cmake", "--install", build_d])


if __name__ == "__main__":
    main()
