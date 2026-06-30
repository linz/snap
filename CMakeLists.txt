cmake_minimum_required(VERSION 3.20)

project(snap
    VERSION 2.8.8
    DESCRIPTION "SNAP network adjustment software"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release if nothing is specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Release Debug Profile)
endif()

# ── Compiler flags ────────────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /wd4244)
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _USE_MATH_DEFINES
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

    # CMake injects -DNDEBUG automatically for Release; Profile needs it added
    add_compile_definitions(
        UNIX
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
find_package(Boost REQUIRED COMPONENTS filesystem regex)
# boost::system is header-only since Boost 1.69; newer distros ship no .so for it.
# FindBoost creates Boost::system as a side-effect of filesystem on older Boost — link
# it explicitly only if that target exists, so we don't fail on Ubuntu 24.04+.
if(TARGET Boost::system)
    set(BOOST_SYSTEM_LIB Boost::system)
else()
    set(BOOST_SYSTEM_LIB "")
endif()
find_package(Threads REQUIRED)

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

# ── Sub-projects ──────────────────────────────────────────────────────────────
# lapg generates src/snap_manager/scriptimp_parse.cpp from its .syntax grammar.
# The generated file is not checked in, so lapg is always built when the GUI is
# enabled.  SNAP_BUILD_LAPG allows building it standalone (e.g. to regenerate
# the file manually without a full GUI build).
option(SNAP_BUILD_LAPG "Build the lapg parser-generator tool" OFF)
if(SNAP_BUILD_LAPG OR SNAP_BUILD_GUI)
    add_subdirectory(tools/lapg)
endif()

add_subdirectory(src)
