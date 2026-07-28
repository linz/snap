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




