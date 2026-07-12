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
    install    Build all and install to system
    package    Build a Debian package (Linux, release only; --build-dir sets SNAP_BUILD_DIR)
    clean      Remove the build directory

Options:
    --no-gui         Skip wxWidgets GUI targets (snap_manager, snapadjust, snapplot)
    --jobs N         Parallel jobs (default: all cores)
    --build-dir DIR  Build output directory (default: build-{type}); for package,
                     sets SNAP_BUILD_DIR passed to debuild/debian/rules
    --no-efence      Do not link efence (debug builds link efence by default)
"""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.abspath(__file__))

SNAP_CMD_TARGETS = [
    "snap", "concord", "snapspec", "dat2site",
    "snaplist", "snapconv", "snapgeoid", "snapmerge", "site2gps",
]

BUILD_TYPE_MAP = {
    "release": "Release",
    "debug":   "Debug",
    "profile": "Profile",
}


def build_dir(build_type: str) -> str:
    return os.path.join(REPO_ROOT, f"build-{build_type}")


def run(cmd: list[str], cwd: str | None = None, env: dict | None = None) -> None:
    print("+", " ".join(cmd))
    result = subprocess.run(cmd, cwd=cwd or REPO_ROOT, env=env, check=False)
    if result.returncode != 0:
        sys.exit(result.returncode)


def configure(build_type: str, no_gui: bool, efence: bool, build_d: str) -> None:
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
        os.utime(os.path.join(REPO_ROOT, path), None)


def copy_config_files(build_d: str, build_type: str) -> None:
    # snap locates config files in a config/ subdirectory next to the executable
    # (system_config_dir() = image_dir() + "/config").  Merge the per-component
    # config directories from source so the build-tree executable is runnable.
    config_dst = os.path.join(build_d, "src", "config")
    os.makedirs(config_dst, exist_ok=True)
    for src_subdir in ["snap/config", "snapspec/config", "snaplist/config",
                       "snap_manager/config"]:
        src = os.path.join(REPO_ROOT, "src", src_subdir)
        if os.path.isdir(src):
            shutil.copytree(src, config_dst, dirs_exist_ok=True)

    perl_src = os.path.join(REPO_ROOT, "src", "perl")
    perl_dst = os.path.join(config_dst, "perl")
    if os.path.isdir(perl_src):
        shutil.copytree(perl_src, perl_dst, dirs_exist_ok=True)

    version_src = os.path.join(REPO_ROOT, "VERSION")
    if os.path.isfile(version_src):
        shutil.copy2(version_src, os.path.join(build_d, "src", "VERSION"))

    help_src = os.path.join(REPO_ROOT, "src", "help", "help")
    help_dst = os.path.join(build_d, "src", "help")
    if os.path.isdir(help_src):
        shutil.copytree(help_src, help_dst, dirs_exist_ok=True)

    versionid = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"],
        cwd=REPO_ROOT, capture_output=True, text=True, check=False)
    if versionid.returncode == 0:
        with open(os.path.join(build_d, "src", "VERSIONID"), "w") as f:
            f.write(versionid.stdout.strip())

    if build_type == "debug":
        devel_src = os.path.join(REPO_ROOT, "src", "packages", "devel")
        devel_dst = os.path.join(config_dst, "package", "devel")
        if os.path.isdir(devel_src):
            shutil.copytree(devel_src, devel_dst, dirs_exist_ok=True)


def cmake_build(build_d: str, targets: list[str] | None = None, jobs: int | None = None) -> None:
    cmd = (
        ["cmake", "--build", build_d, "--parallel"]
        + ([str(jobs)] if jobs else [])
        + (["--target"] + targets if targets else [])
    )
    run(cmd)


def run_tests(build_type: str) -> None:
    testall = os.path.join(REPO_ROOT, "regression_tests", "testall.pl")
    run(["perl", testall, "-e"] + (["-r"] if build_type == "release" else []))


def check_committed() -> None:
    result = subprocess.run(["git", "diff", "--quiet", "HEAD"], cwd=REPO_ROOT, check=False)
    if result.returncode != 0:
        print("ABORTED: current files not committed")
        sys.exit(1)


def check_version_changelog() -> None:
    with open(os.path.join(REPO_ROOT, "VERSION")) as f:
        version = f.read().strip()
    with open(os.path.join(REPO_ROOT, "debian", "changelog")) as f:
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
    parser.add_argument("--build-dir", metavar="DIR", default=None,
                        help="Build output directory (default: build-{type})")
    parser.add_argument("--no-efence", action="store_true",
                        help="Do not link efence (debug builds link efence by default)")
    args = parser.parse_args()

    build_d = args.build_dir if args.build_dir else build_dir(args.type)
    efence = (args.type == "debug") and not args.no_efence

    if args.target == "clean":
        if os.path.exists(build_d):
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
            env["SNAP_BUILD_DIR"] = args.build_dir
        if args.no_gui:
            env["SNAP_BUILD_GUI"] = "OFF"
        run(["debuild", "--check-dirname-level=0", "-uc", "-us", "-b"], env=env)
        return

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
