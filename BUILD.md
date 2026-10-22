BUILD INSTRUCTIONS
==================


Build instructions on Windows - MSVC
====================================

Only 64-bit builds are supported.

**Prerequisites**

1. **CMake** 3.20 or later — https://cmake.org/download/
2. **Ninja** — `winget install Ninja-build.Ninja` or download from https://ninja-build.org
3. **Visual Studio 2022 or later** with the *Desktop development with C++* workload
4. **Boost** — download the source from https://www.boost.org/users/download/ and unpack it.
   The Windows builds use only the Boost headers, so nothing is built. Set the `BOOST_ROOT`
   environment variable to the Boost root directory (e.g. `set BOOST_ROOT=C:\boost_1_78_0`).
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
4. **Boost** headers. Reuse an existing Boost source tree if you have one already (no need
   to re-download), or fetch a fresh one from https://www.boost.org/users/download/ and
   unpack it. The Windows builds use only the Boost headers, so nothing is built. Set
   `BOOST_ROOT` to the Boost source root.
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
   one shared prefix. `build_mingw_netcdf_stack.py` does all of it:
   ```
   ./build_mingw_netcdf_stack.py
   ```
   It downloads and builds zlib, HDF5 and netCDF-C into `netcdf-mingw/` in the repository,
   with the options that are described in the script, and prints how long each step took. It
   stops if `netcdf-mingw/` already exists, so a run is always a complete build. `--work-dir`
   puts it somewhere else, and `--jobs` sets how many parallel jobs the builds use.

   Set `NETCDF_MINGW_ROOT` to the `install` directory that it prints at the end (e.g.
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

Tested against Ubuntu 24.04 (Noble) and 26.04 (Resolute) amd64.

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

**Running the tests on programs that were built elsewhere**

`tests_only/` is a CMake project that registers the same tests, and runs them against programs
that are already built. It needs no compiler and none of the libraries that the programs use. It
takes the test definitions and data from the repository that it is in, so that has to be the same
version as the programs:

```
cmake -S tests_only -B build-tests -DSNAP_PROGRAMS_DIR=<absolute path of the programs>
ctest --test-dir build-tests --parallel
```

The directory has to have the programs, `binroundtrip` and the unit test programs. A build has all of
them in `build-release/src`. Configuring a build with `-DSNAP_PACKAGE_TEST_PROGRAMS=ON` also installs
`binroundtrip` and the unit tests with the programs, so that they are in the package. The ZIP that the
`mingw-zip` job of `test.yml` makes is configured that way. It is not for release, as the test programs
are not part of the software.

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




