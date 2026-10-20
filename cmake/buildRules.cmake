# The rules for building the programs: the C++ standard, the build types, the compiler flags, the libraries
# that the programs need, and where they are installed. The top level CMakeLists.txt includes this after
# project(), which is where the language is enabled.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release if nothing is specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Release Debug Profile)
endif()

# ── Compiler flags ────────────────────────────────────────────────────────────
if(WIN32)
    # Static-link everything on Windows, so the built exe has no dependency on
    # a redistributable (MSVC runtime / MinGW's libgcc+libstdc++) on the
    # target machine. Mechanism differs per compiler; the goal is shared.
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        add_link_options(-static-libgcc -static-libstdc++ -static)
    endif()
endif()

if(MSVC)
    add_compile_options(/W4 /wd4244)
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _USE_MATH_DEFINES
        BOOST_ALL_NO_LIB
        $<$<CONFIG:Debug>:CHECKBLT>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>)
else()
    # Register Profile as a valid build type with its own flag set
    set(CMAKE_CXX_FLAGS_PROFILE   "-O2 -g -pg -Wall"
        CACHE STRING "Flags for Profile build" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_PROFILE "-pg"
        CACHE STRING "Linker flags for Profile build" FORCE)

    add_compile_options(
        -Wall
        $<$<CONFIG:Release>:-Ofast>
        $<$<CONFIG:Release>:-mfma>
        $<$<CONFIG:Debug>:-g>)

    add_link_options($<$<CONFIG:Release>:-s>)

    option(SNAP_EFENCE "Link efence" OFF)
    if(SNAP_EFENCE)
        add_link_options(-lefence)
    endif()

    # CMake injects -DNDEBUG automatically for Release; Profile needs it added.
    # UNIX is only correct for an actual Unix target - a non-MSVC compiler
    # (e.g. MinGW) targeting Windows must not get it, or it contradicts
    # snapconfig.h's own _WIN32/_MSC_VER platform check.
    add_compile_definitions(
        $<$<NOT:$<BOOL:${WIN32}>>:UNIX>
        $<$<CONFIG:Debug>:CHECKBLT>
        $<$<CONFIG:Profile>:NDEBUG>)
endif()

# ── Dependencies ──────────────────────────────────────────────────────────────
# CMP0167 (CMake 3.30+): whether to use Boost's own BoostConfig.cmake or the
# FindBoost module.  We use OLD (FindBoost) because Ubuntu's packaged Boost
# does not install per-component cmake config files (boost_system-config.cmake
# etc.) that BoostConfig.cmake requires.  FindBoost locates Boost via headers
# and .so files directly, which works with distro-packaged Boost.
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()
# CMP0144 (CMake 3.27+): FindBoost respects upper-case BOOST_ROOT.  Set OLD so
# the BOOST_ROOT env/cache variable is matched case-insensitively on Windows too.
if(POLICY CMP0144)
    cmake_policy(SET CMP0144 OLD)
endif()
# Boost.Regex is used where snapconfig.h defines REGEX_BOOST, which is every build except Windows (the same
# condition that defines UNIX above). The Windows builds use std::regex, so they need only the Boost headers.
if(WIN32)
    find_package(Boost REQUIRED)
    set(BOOST_REGEX_LIB "")
else()
    find_package(Boost REQUIRED COMPONENTS regex)
    set(BOOST_REGEX_LIB Boost::regex)
endif()
# boost::system is header-only since Boost 1.69; newer distros ship no .so for it.
# FindBoost creates Boost::system as a side-effect of regex on older Boost — link
# it explicitly only if that target exists, so we don't fail on Ubuntu 24.04+.
if(TARGET Boost::system)
    set(BOOST_SYSTEM_LIB Boost::system)
else()
    set(BOOST_SYSTEM_LIB "")
endif()
find_package(Threads REQUIRED)

# Needed by the custom command in src/CMakeLists.txt that runs build_help.py
# to generate src/help/help/index.html and js/wordindex.js at build time.
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# netCDF-C, for reading GGRXF deformation model files (netCDF-4/HDF5-based).
# Ubuntu's libnetcdf-dev ships a netCDFConfig.cmake providing the
# netCDF::netcdf target, so a plain config-mode find_package works here
# (unlike Boost above).
find_package(netCDF REQUIRED)

option(SNAP_BUILD_GUI
    "Build wxWidgets GUI targets (snap_manager, snapadjust, snapplot)" ON)
if(SNAP_BUILD_GUI)
    find_package(wxWidgets REQUIRED COMPONENTS core base adv html)
endif()

# ── Install paths ─────────────────────────────────────────────────────────────
include(GNUInstallDirs)

# Path to the coordsys data directory on the target system (Linux/Debian only)
set(SNAP_COORDSYSDIR "/usr/share/linz/coordsys"
    CACHE PATH "Path to installed coordinate system data")

# On Windows install flat under the prefix; on Linux follow the FHS tree.
if(WIN32)
    set(SNAP_INSTALL_ROOT ".")
else()
    set(SNAP_INSTALL_ROOT "${CMAKE_INSTALL_DATADIR}/linz/snap")
endif()
