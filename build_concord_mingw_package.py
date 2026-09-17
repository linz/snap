#!/usr/bin/env python3
"""
build_concord_mingw_package.py - build a standalone concord.exe + linz-coordsys
data package for Windows (MinGW-w64 cross-compile)

Usage:
    build_concord_mingw_package.py --snap-tag TAG --coordsys-tag TAG --output ZIP

Reproduces the old "SNAP and CONCORD downloads" concord zip: concord.exe plus
the linz-coordsys data files it needs at runtime (config/coordsys, next to the
executable - see system_config_dir() in src/snaplib/util/fileutil.cpp). Both
snap and linz-coordsys are cloned fresh at the given tag into a temporary
directory, so the result only ever reflects what those tags actually contain,
never the caller's own working tree. Fails loudly (via git's own error) if
either tag doesn't exist.

Requires BOOST_ROOT set to a MinGW-built Boost tree (see BUILD.md) - concord
links snaplib, which links Boost unconditionally.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

from build import run

SNAP_REPO_URL = "https://github.com/linz/snap.git"
COORDSYS_REPO_URL = "https://github.com/linz/linz-coordsys.git"
MINGW_PRESET = "windows-mingw-release"


def clone_at_tag(repo_url: str, tag: str, dest: Path) -> str:
    """Shallow-clones repo_url at tag into dest and returns the resolved commit hash.
    An unresolvable tag makes git itself exit non-zero with its own error message,
    so nothing here needs to check the tag first."""
    run(["git", "clone", "--branch", tag, "--single-branch", "--depth", "1", repo_url, dest])
    commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=dest, capture_output=True, text=True, check=True)
    return commit.stdout.strip()


def require_env(name: str) -> None:
    """Aborts with an explanatory message if the named environment variable isn't set."""
    if not os.environ.get(name):
        print(f"ABORTED: {name} must be set - see BUILD.md")
        sys.exit(1)


def build_concord(snap_dir: Path) -> Path:
    """Cross-compiles the concord target from snap_dir via the MinGW-w64 release
    preset and returns the path to the resulting exe."""
    run(["cmake", "--preset", MINGW_PRESET], cwd=snap_dir)
    run(["cmake", "--build", "--preset", MINGW_PRESET, "--target", "concord"], cwd=snap_dir)
    return snap_dir / "build" / MINGW_PRESET / "src" / "concord.exe"


def write_package(concord_exe: Path, coordsys_files_dir: Path, manifest: str, output: Path) -> None:
    """Zips concord_exe, the linz-coordsys data files, and the manifest text into
    output, laid out to match concord's own runtime config lookup."""
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.write(concord_exe, "concord.exe")
        for data_file in sorted(coordsys_files_dir.iterdir()):
            zf.write(data_file, f"config/coordsys/{data_file.name}")
        zf.writestr("MANIFEST.txt", manifest)


def main() -> None:
    """Parses arguments, clones snap and linz-coordsys at the given tags, builds
    concord, and writes the packaged zip."""
    parser = argparse.ArgumentParser(
        description="Build a standalone concord.exe + linz-coordsys package for Windows",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--snap-tag", required=True, help="snap git tag to build concord from")
    parser.add_argument("--coordsys-tag", required=True, help="linz-coordsys git tag to bundle data from")
    parser.add_argument("--output", type=Path, required=True, help="Output zip path")
    args = parser.parse_args()

    require_env("BOOST_ROOT")

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        snap_dir = tmp_path / "snap"
        coordsys_dir = tmp_path / "linz-coordsys"

        print(f"Cloning snap at {args.snap_tag}...")
        snap_commit = clone_at_tag(SNAP_REPO_URL, args.snap_tag, snap_dir)

        print(f"Cloning linz-coordsys at {args.coordsys_tag}...")
        coordsys_commit = clone_at_tag(COORDSYS_REPO_URL, args.coordsys_tag, coordsys_dir)

        print("Building concord...")
        concord_exe = build_concord(snap_dir)
        if not concord_exe.is_file():
            print(f"ABORTED: expected build output not found at {concord_exe}")
            sys.exit(1)

        manifest = (
            f"snap tag: {args.snap_tag}\n"
            f"snap commit: {snap_commit}\n"
            f"linz-coordsys tag: {args.coordsys_tag}\n"
            f"linz-coordsys commit: {coordsys_commit}\n"
        )

        print(f"Writing package to {args.output}...")
        write_package(concord_exe, coordsys_dir / "files", manifest, args.output)

    print(f"Wrote {args.output}")


if __name__ == "__main__":
    main()
