#!/usr/bin/env python3
"""Cross-builds the static netCDF stack (zlib, HDF5, netCDF-C) for the MinGW-w64 Windows build.

The three libraries are built from source with cmake/toolchain-mingw-w64.cmake and installed to one prefix,
<work dir>/install. Point NETCDF_MINGW_ROOT at that prefix before configuring the windows-mingw-release[-gui]
presets. Every step is timed and the times are printed at the end.

The work directory must not exist yet, so each run is a complete build. See BUILD.md for what the build needs.
"""

import argparse
import os
import subprocess
import sys
import tarfile
import time
import urllib.request
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent
TOOLCHAIN_FILE = REPO_ROOT / "cmake" / "toolchain-mingw-w64.cmake"

ZLIB_URL = "https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz"
HDF5_URL = "https://github.com/HDFGroup/hdf5/releases/download/hdf5_1.14.5/hdf5-1.14.5.tar.gz"
NETCDF_URL = "https://github.com/Unidata/netcdf-c/archive/refs/tags/v4.9.3.tar.gz"

# netCDF-C's installed CMake config references HDF5::HDF5, hdf5::hdf5_hl and ZLIB::ZLIB but never defines them for a
# consumer. They are defined in the installed config, just before it includes its own targets file.
NETCDF_CONFIG_MARKER = 'include("${CMAKE_CURRENT_LIST_DIR}/netCDFTargets.cmake")'
NETCDF_CONFIG_PATCH = """add_library(ZLIB::ZLIB STATIC IMPORTED)
set_target_properties(ZLIB::ZLIB PROPERTIES
  IMPORTED_LOCATION "${netCDF_LIB_DIR}/libzlibstatic.a"
  INTERFACE_INCLUDE_DIRECTORIES "${netCDF_INCLUDE_DIR}")
include("${netCDF_INSTALL_PREFIX}/cmake/hdf5-config.cmake")
add_library(HDF5::HDF5 ALIAS hdf5-static)
add_library(hdf5::hdf5_hl ALIAS hdf5_hl-static)
"""

# zlib is built for HDF5's compression. It is not wxWidgets' own zlib, whose symbols are all renamed wx_zlib_*.
ZLIB_OPTIONS = ["-DBUILD_SHARED_LIBS=OFF"]

# Static only, no szip, MPI, Fortran, C++ or Java bindings, and no tools, tests or examples.
HDF5_OPTIONS = [
    "-DBUILD_SHARED_LIBS=OFF",
    "-DBUILD_STATIC_LIBS=ON",
    "-DHDF5_BUILD_EXAMPLES=OFF",
    "-DHDF5_BUILD_TOOLS=OFF",
    "-DHDF5_BUILD_TESTS=OFF",
    "-DBUILD_TESTING=OFF",
    "-DHDF5_BUILD_UTILS=OFF",
    "-DHDF5_ENABLE_SZIP_SUPPORT=OFF",
    "-DHDF5_ENABLE_Z_LIB_SUPPORT=ON",
    "-DHDF5_ENABLE_PARALLEL=OFF",
    "-DHDF5_BUILD_FORTRAN=OFF",
    "-DHDF5_BUILD_CPP_LIB=OFF",
    "-DHDF5_BUILD_JAVA=OFF",
    "-DHDF5_BUILD_HL_LIB=ON",
]

# DAP, S3, NCZarr, parallel and filter plugin support are off, because GGRXF only reads local netCDF-4 files.
# NETCDF_ENABLE_LIBXML2 is off even with DAP off: left on, netCDF links the host's own native libxml2.a into
# the cross-compiled library, and off it uses its bundled tinyxml2.
NETCDF_OPTIONS = [
    "-DBUILD_SHARED_LIBS=OFF",
    "-DENABLE_TESTS=OFF",
    "-DENABLE_EXAMPLES=OFF",
    "-DENABLE_UTILITIES=OFF",
    "-DBUILD_UTILITIES=OFF",
    "-DENABLE_DAP=OFF",
    "-DENABLE_DAP2=OFF",
    "-DENABLE_DAP4=OFF",
    "-DENABLE_BYTERANGE=OFF",
    "-DENABLE_NCZARR=OFF",
    "-DENABLE_NCZARR_ZIP=OFF",
    "-DENABLE_HDF4=OFF",
    "-DENABLE_PNETCDF=OFF",
    "-DENABLE_PARALLEL4=OFF",
    "-DENABLE_PLUGINS=OFF",
    "-DENABLE_FILTER_SZIP=OFF",
    "-DENABLE_FILTER_ZSTD=OFF",
    "-DENABLE_FILTER_BZ2=OFF",
    "-DENABLE_FILTER_BLOSC=OFF",
    "-DENABLE_QUANTIZE=OFF",
    "-DNETCDF_ENABLE_LIBXML2=OFF",
]

timings: list[tuple[str, float]] = []


def timed(name: str, step: Callable[[], None]) -> None:
    """Runs step, prints how long it took and records the time under name for the final table."""
    print(f"\n=== {name}", flush=True)
    start = time.perf_counter()
    step()
    seconds = time.perf_counter() - start
    timings.append((name, seconds))
    print(f"=== {name}: {seconds:.1f} s", flush=True)


def run(command: list[str]) -> None:
    """Runs command, echoing it first, and raises if it fails."""
    print("+ " + " ".join(command), flush=True)
    subprocess.run(command, check=True)


def download_and_extract(url: str, tarball: Path, work_dir: Path) -> None:
    """Downloads the tarball at url to tarball and extracts it into work_dir."""
    urllib.request.urlretrieve(url, tarball)  # noqa: S310 (fixed https URLs above)
    with tarfile.open(tarball) as archive:
        archive.extractall(work_dir, filter="data")


def configure(source_dir: Path, build_dir: Path, install_dir: Path, options: list[str]) -> None:
    """Configures a Release cross-build of source_dir with the MinGW toolchain, installing to install_dir."""
    run(
        [
            "cmake",
            "-S",
            str(source_dir),
            "-B",
            str(build_dir),
            f"-DCMAKE_TOOLCHAIN_FILE={TOOLCHAIN_FILE}",
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_INSTALL_PREFIX={install_dir}",
            *options,
        ]
    )


def build_and_install(build_dir: Path, jobs: int) -> None:
    """Builds the configured tree in build_dir with jobs parallel jobs, then installs it."""
    run(["cmake", "--build", str(build_dir), f"-j{jobs}"])
    run(["cmake", "--install", str(build_dir)])


def patch_netcdf_config(install_dir: Path) -> None:
    """Defines the targets that the installed netCDFConfig.cmake references but never creates."""
    config = install_dir / "lib" / "cmake" / "netCDF" / "netCDFConfig.cmake"
    text = config.read_text()
    if NETCDF_CONFIG_MARKER not in text:
        raise RuntimeError(f"{config} has no line {NETCDF_CONFIG_MARKER!r}; netCDF's config has changed")
    config.write_text(text.replace(NETCDF_CONFIG_MARKER, NETCDF_CONFIG_PATCH + NETCDF_CONFIG_MARKER))


@dataclass(frozen=True)
class Library:
    """One library of the stack."""

    name: str  # used for the tarball, the build directory and the names in the timings
    url: str  # the source tarball
    source_name: str  # the directory the tarball extracts to
    options: list[str]  # the CMake options for configuring it


def build_library(library: Library, work_dir: Path, install_dir: Path, jobs: int) -> None:
    """Downloads, configures, builds and installs one library, timing each of the three stages."""
    name = library.name
    build_dir = work_dir / f"{name}-build"
    timed(f"{name} download+extract", lambda: download_and_extract(library.url, work_dir / f"{name}.tar.gz", work_dir))
    timed(
        f"{name} configure",
        lambda: configure(work_dir / library.source_name, build_dir, install_dir, library.options),
    )
    timed(f"{name} build+install", lambda: build_and_install(build_dir, jobs))


def main() -> int:
    """Builds the stack and prints the timings and the value for NETCDF_MINGW_ROOT."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "--work-dir",
        type=Path,
        default=REPO_ROOT / "netcdf-mingw",
        help="where the sources, build trees and install prefix go; must not exist (default: %(default)s)",
    )
    parser.add_argument(
        "--jobs", type=int, default=os.cpu_count() or 1, help="parallel build jobs (default: %(default)s)"
    )
    args = parser.parse_args()

    work_dir = args.work_dir.resolve()
    if work_dir.exists():
        print(f"ABORTED: {work_dir} already exists; remove it or pass another --work-dir")
        return 1
    work_dir.mkdir(parents=True)
    install_dir = work_dir / "install"
    zlib_options = [
        f"-DZLIB_LIBRARY={install_dir}/lib/libzlibstatic.a",
        f"-DZLIB_INCLUDE_DIR={install_dir}/include",
    ]
    dependent_options = [f"-DCMAKE_PREFIX_PATH={install_dir}", *zlib_options]

    start = time.perf_counter()
    libraries = [
        Library("zlib", ZLIB_URL, "zlib-1.3.1", ZLIB_OPTIONS),
        Library("hdf5", HDF5_URL, "hdf5-1.14.5", HDF5_OPTIONS + dependent_options),
        Library(
            "netcdf-c",
            NETCDF_URL,
            "netcdf-c-4.9.3",
            NETCDF_OPTIONS + [f"-DHDF5_ROOT={install_dir}", *dependent_options],
        ),
    ]
    for library in libraries:
        build_library(library, work_dir, install_dir, args.jobs)
    timed("netCDFConfig.cmake patch", lambda: patch_netcdf_config(install_dir))
    total = time.perf_counter() - start

    print("\n=== Timings")
    for name, seconds in timings:
        print(f"{name:30} {seconds:8.1f} s")
    print(f"{'total':30} {total:8.1f} s")
    print(f"\nNETCDF_MINGW_ROOT={install_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
