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
5. **Perl** — ActiveState (https://www.activestate.com/products/perl/) or
   Strawberry Perl (https://strawberryperl.com/)
6. **Python 3** — https://www.python.org/downloads/windows/ (needed to run
   grid.py/trig.py/linzdeformationmodel.py, the tools that convert grid,
   triangulation, and deformation-model definitions to their binary form)
7. **netCDF-C**, via vcpkg — needed to read GGRXF deformation model files
   (netCDF-4/HDF5-based):
   ```
   git clone https://github.com/microsoft/vcpkg.git
   .\vcpkg\bootstrap-vcpkg.bat
   .\vcpkg\vcpkg install netcdf-c:x64-windows
   ```
   Set the `VCPKG_ROOT` environment variable to the cloned `vcpkg` directory
   (e.g. `set VCPKG_ROOT=C:\vcpkg`) — the MSVC presets below pick vcpkg's own
   toolchain file up from there automatically.

**Building**

Run all commands from an *x64 Native Tools Command Prompt for VS* so that `cl.exe` is on
the PATH.

```
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
```

Binaries are placed in `build\windows-release\`. For a debug build use the
`windows-msvc-debug` preset, which places output in `build\windows-debug\`.

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
6. **netCDF-C**, cross-built for MinGW — needed to read GGRXF deformation
   model files (netCDF-4/HDF5-based). MSYS2 already publishes a prebuilt
   `mingw-w64-x86_64-netcdf` package (built for the same mingw-w64 ABI the
   `g++-mingw-w64-x86-64` cross-compiler above targets), including a
   `netCDFConfig.cmake` — no from-source cross-build needed, just download
   and extract it:
   ```
   sudo apt-get install -y zstd
   curl -LO https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-netcdf-4.9.3-7-any.pkg.tar.zst
   mkdir netcdf-mingw && tar --zstd -xf mingw-w64-x86_64-netcdf-4.9.3-7-any.pkg.tar.zst -C netcdf-mingw
   ```
   Check https://repo.msys2.org/mingw/mingw64/ for the current package
   filename if this exact version is no longer available. Set
   `NETCDF_MINGW_ROOT` to the extracted `mingw64` subdirectory (e.g.
   `export NETCDF_MINGW_ROOT=$(pwd)/netcdf-mingw/mingw64`).

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
    perl \
    python3 \
    debhelper \
    dpkg-dev \
    devscripts
```

On Jammy (22.04), the wxWidgets package is `libwxgtk3.0-gtk3-dev` — the `3.2`
package above is Noble-only.

The snap software is built using `build.py` in the root of the repository:

```
python3 build.py
python3 build.py release test
```

Build types are `release` (default), `debug`, and `profile`. The release build
is placed in `build-release/`. The GUI targets (snap_manager, snapadjust,
snapplot) are built by default; pass `--no-gui` to skip them. See
`python3 build.py --help` for all options.

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




