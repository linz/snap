BUILD INSTRUCTIONS
==================


Build instructions on Windows - MSVC
====================================

Only 64-bit builds are supported.

**Prerequisites**

1. **CMake** 3.20 or later — https://cmake.org/download/
2. **Ninja** — `winget install Ninja-build.Ninja` or download from https://ninja-build.org
3. **Visual Studio 2022 or later** with the *Desktop development with C++* workload
4. **Boost** — download source from https://www.boost.org/users/download/ and build the
   required components from the Boost root directory:
   ```
   ./b2 toolset=msvc-14.3 --with-regex --with-system
   ```
   Replace the toolset version to match your Visual Studio installation:
   | Visual Studio | Toolset   |
   |---------------|-----------|
   | 2022          | msvc-14.3 |
   | 2026          | msvc-14.5 |

   Then set the `BOOST_ROOT` environment variable to the Boost root directory
   (e.g. `set BOOST_ROOT=C:\boost_1_78_0`).
5. **Python 3** — https://www.python.org/downloads/windows/ (needed to run
   grid.py/trig.py/linzdeformationmodel.py, the tools that convert grid,
   triangulation, and deformation-model definitions to their binary form, and to
   run the tests)
6. **netCDF-C**, via vcpkg — needed to read GGRXF deformation model files
   (netCDF-4/HDF5-based):
   ```
   git clone https://github.com/microsoft/vcpkg.git
   .\vcpkg\bootstrap-vcpkg.bat
   .\vcpkg\vcpkg install netcdf-c:x64-windows
   ```
   Set the `VCPKG_ROOT` environment variable to the cloned `vcpkg` directory
   (e.g. `set VCPKG_ROOT=C:\vcpkg`) — the MSVC presets below pick vcpkg's own
   toolchain file up from there automatically.

Perl is not needed to build SNAP. It is needed to run SNAP: the scripts in `src/perl` use
it, and so does the function of SNAP Manager that retrieves coordinates from the geodetic
database. Install Strawberry Perl (https://strawberryperl.com/) or ActiveState Perl
(https://www.activestate.com/products/perl/) on a computer that is to use those.

**Building**

Run all commands from an *x64 Native Tools Command Prompt for VS* so that `cl.exe` is on
the PATH.

```
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
```

Binaries are placed in `build\windows-release\`. For a debug build use the
`windows-msvc-debug` preset, which places output in `build\windows-debug\`.

**Running the tests**

```
ctest --test-dir build\windows-release --parallel
```

For the debug build use `build\windows-debug`. The files that a test that fails leaves behind
are in `test_output` in the build directory. They are described under *Running the tests* in
the Linux instructions below, which also say how to run one test.

**Coordinate system data**

SNAP needs `coordsys.def` (and related deformation/geoid files). When running
straight out of `build\windows-release\`, before packaging, these aren't
present yet — they're only bundled in by the `cpack` step below, which
copies the repo's own `src\coordsys\coordsys.def`. To run the unpackaged
build directly, point SNAP at that file with the `COORDSYSDEF` environment
variable:

```
set COORDSYSDEF=<repo>\src\coordsys\coordsys.def
```

**Packaging**

Run `cpack` inside the build directory after a successful release build:

```
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
cd build\windows-release
cpack           # builds both ZIP archive and NSIS installer
cpack -G ZIP    # ZIP only — useful for a portable install without running a setup wizard
cpack -G NSIS   # NSIS installer only
```

NSIS must be installed for the `.exe` target: https://nsis.sourceforge.io/

**GUI targets (optional)**

The GUI programs (snap_manager, snapadjust, snapplot) are disabled by default on Windows.
Building them requires wxWidgets — see `wxwidgets\README.md` for instructions.

Once wxWidgets is built, set the `WXWIN` environment variable to the extracted wxWidgets
source root (e.g. `set WXWIN=<repo>\wxwidgets\wxWidgets`), then build with one of the
`-gui` presets:

```
cmake --preset windows-msvc-release-gui
cmake --build --preset windows-msvc-release-gui
```

Build instructions for Windows - MinGW-w64 cross-compile (from Linux)
=======================================================================

Windows binaries can also be built entirely from a Linux (or WSL) host using the
MinGW-w64 cross-compiler, without needing Visual Studio at all.

**Prerequisites**

1. **MinGW-w64 cross-compiler** — `sudo apt-get install g++-mingw-w64-x86-64`
2. **7zip** — `sudo apt-get install 7zip` (to extract the vendored wxWidgets archive;
   only needed for the GUI targets)
3. **NSIS** — `sudo apt-get install nsis` (only needed for packaging)
4. **Boost**, cross-built for MinGW. Reuse an existing Boost source tree if you have
   one already (no need to re-download), or fetch a fresh one from
   https://www.boost.org/users/download/, then from the Boost source root:
   ```
   cat > user-config.jam <<'EOF'
   using gcc : 13 : x86_64-w64-mingw32-g++ ;
   EOF
   ./bootstrap.sh
   ./b2 --user-config=$(pwd)/user-config.jam toolset=gcc-13 target-os=windows \
     address-model=64 architecture=x86 link=static runtime-link=static \
     threading=multi variant=release \
     --with-regex --with-system --stagedir=stage-mingw -j$(nproc)
   ```
   Replace `13` with the major GCC version of your installed
   `g++-mingw-w64-x86-64` (check with `x86_64-w64-mingw32-g++ --version`) - the
   toolset name field must be a numeric-parseable tag, not an arbitrary word.

   Set `BOOST_ROOT` to this Boost source root.
5. **wxWidgets**, cross-built for MinGW (only needed for the GUI targets). wx's own
   Windows makefile (`wxwidgets\build\msw\makefile.gcc`) is `cmd.exe`-only and does
   not work for cross-compiling from Linux - use wx's autotools `configure` build
   instead, which is designed for exactly this:
   ```
   cd wxwidgets
   mkdir wxWidgets_mingw && 7z x wxWidgets-3.2.4.7z -owxWidgets_mingw
   mkdir wx-mingw-build && cd wx-mingw-build
   ../wxWidgets_mingw/configure --host=x86_64-w64-mingw32 --build=x86_64-linux-gnu \
     --disable-shared --enable-unicode --prefix=$(pwd)/install
   make -j$(nproc)
   ```
   Set `WX_MINGW_CONFIG` to the generated `wxwidgets/wx-mingw-build/wx-config` script.
6. **netCDF-C**, cross-built for MinGW — needed to read GGRXF deformation model
   files (netCDF-4/HDF5-based). MSYS2 publishes a prebuilt `mingw-w64-x86_64-netcdf`
   package, but it isn't usable as-is: its `netCDFConfig.cmake` hardcodes the
   original build machine's own absolute path (`D:/M/msys64/mingw64/lib`), and it's
   a full shared build pulling in `libcurl`/`libxml2`/`libzip`/the AWS C++ SDK as
   runtime DLL dependencies — none of which GGRXF reading needs, and which would
   turn this build's existing zero-runtime-DLL property into one needing several
   bundled DLLs. Build netCDF-C from source instead, statically, with everything
   not needed for local netCDF-4 classic reads turned off — along with its own
   dependencies (zlib, HDF5), also built from source and static, all installed to
   one shared prefix:
   ```
   mkdir -p netcdf-mingw

   # zlib - HDF5's own compression dependency. Do NOT reuse the zlib vendored
   # under wxwidgets/wxWidgets_mingw/src/zlib for this - that's wxWidgets' own
   # patched fork with every symbol renamed wx_zlib_*, unusable by anything else
   # linking against it. Use genuine upstream zlib instead.
   curl -LO https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz
   tar xzf zlib-1.3.1.tar.gz -C netcdf-mingw
   cmake -S netcdf-mingw/zlib-1.3.1 -B netcdf-mingw/zlib-build \
     -DCMAKE_TOOLCHAIN_FILE=$(pwd)/cmake/toolchain-mingw-w64.cmake \
     -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(pwd)/netcdf-mingw/install \
     -DBUILD_SHARED_LIBS=OFF
   cmake --build netcdf-mingw/zlib-build -j$(nproc)
   cmake --install netcdf-mingw/zlib-build

   # HDF5 - netCDF-4's underlying storage format. Static only; no szip/MPI/
   # Fortran/C++/Java bindings, no tools/tests/examples.
   curl -LO https://github.com/HDFGroup/hdf5/releases/download/hdf5_1.14.5/hdf5-1.14.5.tar.gz
   tar xzf hdf5-1.14.5.tar.gz -C netcdf-mingw
   cmake -S netcdf-mingw/hdf5-1.14.5 -B netcdf-mingw/hdf5-build \
     -DCMAKE_TOOLCHAIN_FILE=$(pwd)/cmake/toolchain-mingw-w64.cmake \
     -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(pwd)/netcdf-mingw/install \
     -DCMAKE_PREFIX_PATH=$(pwd)/netcdf-mingw/install \
     -DBUILD_SHARED_LIBS=OFF -DBUILD_STATIC_LIBS=ON \
     -DHDF5_BUILD_EXAMPLES=OFF -DHDF5_BUILD_TOOLS=OFF -DHDF5_BUILD_TESTS=OFF \
     -DBUILD_TESTING=OFF -DHDF5_BUILD_UTILS=OFF -DHDF5_ENABLE_SZIP_SUPPORT=OFF \
     -DHDF5_ENABLE_Z_LIB_SUPPORT=ON -DHDF5_ENABLE_PARALLEL=OFF \
     -DHDF5_BUILD_FORTRAN=OFF -DHDF5_BUILD_CPP_LIB=OFF -DHDF5_BUILD_JAVA=OFF \
     -DHDF5_BUILD_HL_LIB=ON \
     -DZLIB_LIBRARY=$(pwd)/netcdf-mingw/install/lib/libzlibstatic.a \
     -DZLIB_INCLUDE_DIR=$(pwd)/netcdf-mingw/install/include
   cmake --build netcdf-mingw/hdf5-build -j$(nproc)
   cmake --install netcdf-mingw/hdf5-build

   # netCDF-C itself. DAP/S3/NCZarr/parallel/filter-plugin support all off, since
   # GGRXF only ever does local netCDF-4 classic reads.
   curl -L -o netcdf-c-4.9.3.tar.gz https://github.com/Unidata/netcdf-c/archive/refs/tags/v4.9.3.tar.gz
   tar xzf netcdf-c-4.9.3.tar.gz -C netcdf-mingw
   cmake -S netcdf-mingw/netcdf-c-4.9.3 -B netcdf-mingw/netcdf-c-build \
     -DCMAKE_TOOLCHAIN_FILE=$(pwd)/cmake/toolchain-mingw-w64.cmake \
     -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(pwd)/netcdf-mingw/install \
     -DCMAKE_PREFIX_PATH=$(pwd)/netcdf-mingw/install \
     -DBUILD_SHARED_LIBS=OFF -DENABLE_TESTS=OFF -DENABLE_EXAMPLES=OFF \
     -DENABLE_UTILITIES=OFF -DBUILD_UTILITIES=OFF \
     -DENABLE_DAP=OFF -DENABLE_DAP2=OFF -DENABLE_DAP4=OFF -DENABLE_BYTERANGE=OFF \
     -DENABLE_NCZARR=OFF -DENABLE_NCZARR_ZIP=OFF -DENABLE_HDF4=OFF \
     -DENABLE_PNETCDF=OFF -DENABLE_PARALLEL4=OFF -DENABLE_PLUGINS=OFF \
     -DENABLE_FILTER_SZIP=OFF -DENABLE_FILTER_ZSTD=OFF -DENABLE_FILTER_BZ2=OFF \
     -DENABLE_FILTER_BLOSC=OFF -DENABLE_QUANTIZE=OFF -DNETCDF_ENABLE_LIBXML2=OFF \
     -DHDF5_ROOT=$(pwd)/netcdf-mingw/install \
     -DZLIB_LIBRARY=$(pwd)/netcdf-mingw/install/lib/libzlibstatic.a \
     -DZLIB_INCLUDE_DIR=$(pwd)/netcdf-mingw/install/include
   cmake --build netcdf-mingw/netcdf-c-build -j$(nproc)
   cmake --install netcdf-mingw/netcdf-c-build
   ```
   `-DNETCDF_ENABLE_LIBXML2=OFF` matters even with DAP disabled — left on, it's
   used for NcML metadata parsing regardless of the DAP settings, and auto-detects
   and links the *host's own native Linux* `libxml2.a` into the cross-compiled
   static library. Off, netCDF falls back to its own bundled tinyxml2 and needs
   nothing external for XML.

   netCDF-C's own CMake export has a separate real gap: `netCDFTargets.cmake`
   references `HDF5::HDF5`/`hdf5::hdf5_hl`/`ZLIB::ZLIB` by name, but
   `netCDFConfig.cmake` never arranges for a consumer to have those targets
   defined — they only ever exist as a side effect inside netCDF's own build, via
   CMake's module-mode `FindHDF5`/`FindZLIB`. Patch that into the installed config
   once, right after the builds above:
   ```
   python3 - <<'EOF'
   import pathlib
   p = pathlib.Path("netcdf-mingw/install/lib/cmake/netCDF/netCDFConfig.cmake")
   text = p.read_text()
   marker = 'include("${CMAKE_CURRENT_LIST_DIR}/netCDFTargets.cmake")'
   patch = '''add_library(ZLIB::ZLIB STATIC IMPORTED)
   set_target_properties(ZLIB::ZLIB PROPERTIES
     IMPORTED_LOCATION "${netCDF_LIB_DIR}/libzlibstatic.a"
     INTERFACE_INCLUDE_DIRECTORIES "${netCDF_INCLUDE_DIR}")
   include("${netCDF_INSTALL_PREFIX}/cmake/hdf5-config.cmake")
   add_library(HDF5::HDF5 ALIAS hdf5-static)
   add_library(hdf5::hdf5_hl ALIAS hdf5_hl-static)
   '''
   p.write_text(text.replace(marker, patch + marker))
   EOF
   ```

   Set `NETCDF_MINGW_ROOT` to the combined install prefix used above (e.g.
   `export NETCDF_MINGW_ROOT=$(pwd)/netcdf-mingw/install`).

**Building**

Either use `build.py --mingw` (see below), or invoke the presets directly:

```
cmake --preset windows-mingw-release-gui
cmake --build --preset windows-mingw-release-gui
```

(Use `windows-mingw-release` instead for a non-GUI build.) Binaries are placed in
`build/windows-mingw-release[-gui]/src/`.

Using `build.py`:

```
python3 build.py release all --mingw
python3 build.py release package --mingw   # ZIP + NSIS installer, via cpack
```

`--no-gui` skips the GUI targets (and the `WX_MINGW_CONFIG` requirement). Only the
`release` type and the `all`/`package`/`clean` targets are supported with `--mingw` -
`snap_cmd`/`test`/`install` don't apply to a cross-compiled Windows build.

Coordinate system data and packaging otherwise work the same way as the MSVC build
above (`COORDSYSDEF`, `cpack`).

**Standalone concord package**

`build_concord_mingw_package.py` (repository root) builds a standalone `concord.exe` +
`linz-coordsys` data zip, reproducing the old "SNAP and CONCORD downloads" concord
package without needing a full SNAP install. It clones `linz/snap` and
`linz/linz-coordsys` fresh at the given tags (never the caller's own working tree),
cross-compiles just the `concord` target via the `windows-mingw-release` preset, and
zips the result together with a manifest recording both tags and their resolved
commits:

```
python3 build_concord_mingw_package.py --snap-tag 3.0.0-1 --coordsys-tag 1.15.0-1 \
    --output concord-3.0.0-1.zip
```

Requires `BOOST_ROOT` (see above) - no `WX_MINGW_CONFIG` needed, since concord has no
GUI dependency.

Build instructions for Linux
============================

Tested against Ubuntu 22.04 (Jammy) and 24.04 (Noble) amd64.

Install prerequisites (the last three are required to build a debian package
for installation):

```
apt-get install -y \
    cmake \
    g++ \
    libboost-all-dev \
    libnetcdf-dev \
    libwxgtk3.2-dev \
    python3 \
    debhelper \
    dpkg-dev \
    devscripts
```

On Jammy (22.04), the wxWidgets package is `libwxgtk3.0-gtk3-dev` — the `3.2`
package above is Noble-only.

Perl is not needed to build SNAP. It is needed to run SNAP: the scripts in `src/perl` use
it, and so does the function of SNAP Manager that retrieves coordinates from the geodetic
database. The debian package depends on `perl` for that, and a build that is run straight
out of `build-release/` needs it installed on the computer that uses those.

The snap software is built using `build.py` in the root of the repository:

```
python3 build.py
python3 build.py release test
```

Build types are `release` (default), `debug`, and `profile`. The release build
is placed in `build-release/`. The GUI targets (snap_manager, snapadjust,
snapplot) are built by default; pass `--no-gui` to skip them. See
`python3 build.py --help` for all options.

**Running the tests**

`python3 build.py release test` builds the programs and the unit tests, then runs the
tests with `ctest`, which runs them in parallel. To run them again without building,
run `ctest` in the build directory:

```
cd build-release
ctest --parallel
ctest -L unit          # only the unit tests, or -L regression for the others
ctest -R concord       # only the tests with concord in their name
```

`ctest` can also be run from any directory, with `--test-dir build-release` to say which
build directory to use.

Each unit test, each regression test and each case of the concord input test is a
test of its own, so a failure names it. The files that a regression test that fails
leaves behind are in `build-release/test_output/`, at the path the test has in the
repository, for example `regression_tests/snap/test1/output`. Running any test of a suite
empties the suite's directory first, so it only has what the latest run left. Beside `output` there is a
`test` directory with links to the test's definition and to its check data. To keep the
files of a test that passes, run it by hand with `--keep`, for example:

```
python3 regression_tests/run_regression_test.py --config regression_tests/snap/test.config --test test1 --keep
```

The temporary files of a unit test go in `build-release/test_output/src/test/<name>/output`,
by way of `TMPDIR` (`TMP` and `TEMP` on Windows). The tests called `.../clear-output` and
`unit-tests/create-output` that ctest shows are what empty and make those directories.

The CMake cache variables `SNAP_UNIT_TEST_TIMEOUT` and `SNAP_REGRESSION_TEST_TIMEOUT` are
the number of seconds a test may run before ctest stops it, and `SNAP_TEST_OUTPUT_DIR`
is where `test_output` is.

**Coordinate system data**

SNAP needs `coordsys.def` (and related deformation/geoid files) from the
separate `linz-coordsys` package, normally found at
`/usr/share/linz/coordsys`. When running straight out of `build-release/`,
before packaging, this isn't wired up yet — it's only symlinked in when SNAP
is installed from its own `.deb` package (see below). To run the unpackaged
build directly, point SNAP at the data with the `COORDSYSDEF` environment
variable, e.g. a `linz-coordsys` checkout or the system path if that package
is already installed:

```
export COORDSYSDEF=/usr/share/linz/coordsys/coordsys.def
```

To build a debian package (all changes must be committed first):

```
python3 build.py release package
```

The package will be created in the parent directory and can be installed using
`dpkg -i linz-snap-<version>.deb`. The snap components are installed into
`/usr/share/linz/snap`. To use the snap command itself you can add this
directory to the path, for example in the `.bashrc` file:

```
export PATH=/usr/share/linz/snap:${PATH}
```

**Static analysis and formatting (clang-tidy and clang-format)**

`build.py` can run clang-tidy and clang-format over the C++ sources. Their results differ between
versions, so the version is fixed (`REQUIRED_CLANG_VERSION` in `build.py`, currently 21) and
`build.py` refuses to run with any other. Anyone fixing a tidy or format failure needs the same
version.

Both tools come from the LLVM apt repository (https://apt.llvm.org). On Ubuntu 24.04 (Noble), add
the line `deb http://apt.llvm.org/noble/ llvm-toolchain-noble-21 main` to the apt sources, install
the repository's signing key, then `apt-get install clang-tidy-21 clang-format-21` (each only if
you run its targets).

**Static analysis (clang-tidy)**

`build.py` runs clang-tidy with the checks that are enforced, which are listed in `.clang-tidy`.

Requirements, on Ubuntu 24.04 (Noble):

- clang-tidy 21, installed as described above.
- The Linux prerequisites above, including `libwxgtk3.2-dev`. The analysis always includes the
  GUI sources, because the GUI build compiles the most source files, so `--no-gui` is rejected.
  Only files that the build compiles are analysed. `src/wxtest` has no build target, so it is
  not.

Ubuntu 22.04 (Jammy) can build and test SNAP but cannot run the analysis: its Boost 1.74
headers produce compile errors under clang-tidy 21, and its wxWidgets 3.0 cannot build the GUI.

```
python3 build.py debug tidy       # reports findings and changes nothing; exits 1 if there are any
python3 build.py debug tidy-fix   # applies the fixes to the sources (requires a clean git tree)
```

Both targets require the `debug` build type. They configure their own build directory,
`build-tidy/`, and leave `build-debug/` alone. The full report is written to
`build-tidy/clang-tidy-report.txt`. Run `tidy` after `tidy-fix` to confirm that nothing is
left. Vendored code (`src/third_party`) and the generated `scriptimp_parse.cpp` are skipped.

On a machine with more than one GCC installed, clang may pick a GCC that has no libstdc++
headers. Set `SNAP_TIDY_GCC_INSTALL_DIR` to the one that does, for example
`/usr/lib/gcc/x86_64-linux-gnu/13`.

**Formatting (clang-format)**

`build.py` can check and apply the code style in `.clang-format` (Google style with four-space
indentation and a 120 column limit). It needs clang-format 21, installed as described above, and
builds nothing, so no other dependencies are required.

```
python3 build.py debug format       # lists the files that would change and changes nothing; exits 1 if there are any
python3 build.py debug format-fix   # reformats the sources (requires a clean git tree), then runs format to confirm
```

The build type is ignored, but one must be given first, as for the other targets. The files are
the C and C++ sources that git tracks under `src/`, without those listed in `.clang-format-ignore`:
the vendored `src/third_party`, `src/snapplot/resource.h` and the generated `scriptimp_parse.cpp`.
`tools/lapg` is outside `src/` and is not formatted. A plain `clang-format` run (clang-format 21
does) reads the same ignore file, so editors skip those files too.

**Include analysis (include-what-you-use)**

`build.py` can run include-what-you-use (IWYU), which reports the `#include`s that each file should
add and the ones it does not need. It is a report only: it changes nothing and exits 0 whatever it
finds (it fails only if IWYU itself fails). Nothing in CI enforces it.

Requirements, on Ubuntu 24.04 (Noble):

- The Linux prerequisites above, including `libwxgtk3.2-dev`. The analysis configures a build
  directory in the same way as `tidy` does, with the GUI sources always included (so `--no-gui` is
  rejected), and reads the compile database from it. It uses `build-tidy/` by default, like `tidy`,
  but needs neither clang-tidy nor a previous `tidy` run.
- An IWYU built on clang 21. Each IWYU release is built for one clang, and `build.py` checks
  that the one it finds reports the same clang version as `REQUIRED_CLANG_VERSION`. CI builds IWYU
  from source, from the `clang_21` branch of
  https://github.com/include-what-you-use/include-what-you-use at the commit named in
  `lint-cpp.yml`, currently `791e69ea4662cb3e74e8128fd5fd69bd7f4ea6b3`. It needs
  `clang-21`, `libclang-21-dev` and `llvm-21-dev` from the LLVM apt repository (`clang-21` provides
  the `ClangConfig.cmake` that cmake looks for), and
  `cmake -S <source> -B <build> -DCMAKE_PREFIX_PATH=/usr/lib/llvm-21 -DClang_DIR=/usr/lib/llvm-21/lib/cmake/clang -DLLVM_DIR=/usr/lib/llvm-21/lib/cmake/llvm -DCMAKE_BUILD_TYPE=Release`,
  then `cmake --build <build> --parallel` and `cmake --install <build>`. The two `_DIR` settings
  stop cmake from picking up another LLVM that is installed, such as the one on a GitHub runner.

```
python3 build.py debug iwyu       # writes the report; exits 0 unless IWYU fails
```

It needs the `debug` build type. `build.py` uses the `include-what-you-use` on the `PATH`, or the
one that `IWYU_BINARY` names. The report is `build-tidy/iwyu-report.txt`, sorted by file, with
each suggested line marked `add` or `remove`; `build-tidy/iwyu-raw.txt` is everything that IWYU
printed.

The report is IWYU's output with the suggestions that are wrong for this code base left out:

- No wxWidgets header or class is suggested: the sources get wx from `src/wxutils/wx_includes.hpp`.
  That header marks its includes `// IWYU pragma: begin_exports`, so IWYU does not ask for them
  to be removed.
- Boost headers that are implementation details, and headers suggested only because of their
  include guard, are left out.
- IWYU analyses the Linux build only, so a file with a Windows branch gets no removals, and
  `snapconfig.h` (which every source includes first) and wx headers are never suggested for removal.

The sources write the C library's names with `std::` (`std::printf`, `std::FILE`, `std::size_t`),
which is why IWYU suggests the C++ headers (`<cstdio>`, `<cstddef>`). A name written without
`std::` makes it suggest the C header instead (`<stdio.h>`).

`tools/iwyu/snap.imp` is the mapping file that IWYU reads, in addition to the Boost mapping that
comes with it.




