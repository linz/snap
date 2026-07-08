cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SNAP_VERSION)
string(STRIP "${SNAP_VERSION}" SNAP_VERSION)

project(snap
    VERSION ${SNAP_VERSION}
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
    # Match wxWidgets' RUNTIME_LIBS=static build (/MT) so the exe has no
    # dependency on the MSVC redistributable on the target machine.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
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
# CMP0144 (CMake 3.27+): FindBoost respects upper-case BOOST_ROOT.  Set OLD so
# the BOOST_ROOT env/cache variable is matched case-insensitively on Windows too.
if(POLICY CMP0144)
    cmake_policy(SET CMP0144 OLD)
endif()
# Match the static CRT build of Boost on Windows (/MT) so FindBoost selects
# the 'mt-s' variants rather than the default 'mt' (/MD) variants.
if(MSVC)
    set(Boost_USE_STATIC_RUNTIME ON)
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

# On Windows install flat under the prefix; on Linux follow the FHS tree.
if(WIN32)
    set(SNAP_INSTALL_ROOT ".")
else()
    set(SNAP_INSTALL_ROOT "${CMAKE_INSTALL_DATADIR}/linz/snap")
endif()

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

# ── Packaging (CPack) ─────────────────────────────────────────────────────────
if(WIN32)
    set(CPACK_PACKAGE_NAME "snap")
    set(CPACK_PACKAGE_VENDOR "LINZ")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_VERBATIM_VARIABLES TRUE)
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.md")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "LINZ\\snap")
    set(CPACK_NSIS_DISPLAY_NAME "SNAP ${PROJECT_VERSION}")
    # CPack's own modify-PATH option (CPACK_NSIS_MODIFY_PATH) always targets a
    # hardcoded "$INSTDIR\bin", which doesn't exist in our flat install layout
    # (see CPACK_NSIS_EXECUTABLES_DIRECTORY below) - it would silently do nothing.
    # The plain NSIS AddToPath/RemoveFromPath functions it would otherwise use
    # are also unsafe: they read/write PATH through a fixed-length NSIS string
    # and silently truncate (corrupting PATH) if it's already long, which is
    # common on a machine with a lot installed - see
    # https://nsis.sourceforge.io/Path_Manipulation. So instead, reuse the old
    # vdproj installer's snap_setup.vbs logic (ported to snap_setup.js - see
    # that file for why), which does this safely via WScript.Shell (no
    # fixed-length buffer) and has done so correctly for years.
    set(CPACK_NSIS_MODIFY_PATH OFF)
    # Executables install flat under SNAP_INSTALL_ROOT ("."), not the NSIS-default "bin".
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS [[
  ExecWait 'cscript //nologo "$INSTDIR\snap_setup.js" install "$INSTDIR"'
  SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000
]])
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS [[
  ExecWait 'cscript //nologo "$INSTDIR\snap_setup.js" uninstall "$INSTDIR"'
  SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000
]])
    if(SNAP_BUILD_GUI)
        set(CPACK_PACKAGE_EXECUTABLES
            "snap_manager" "SNAP Manager"
            "snapadjust" "SNAP Adjust"
            "snapplot" "SNAP Plot")
        # The old vdproj installer always created a Desktop shortcut for SNAP
        # Manager (not optional, named "Run SNAP"). CPack's own "Create Desktop
        # Icon" checkbox is non-functional in the current NSIS template, so add
        # it directly.
        #
        # It also registered .snp/.snap files to open in SNAP Manager.
        string(APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS [[
  CreateShortCut "$DESKTOP\Run SNAP.lnk" "$INSTDIR\snap_manager.exe"

  WriteRegStr SHCTX "Software\Classes\.snp" "" "SNAP.CommandFile"
  WriteRegStr SHCTX "Software\Classes\.snap" "" "SNAP.CommandFile"
  WriteRegStr SHCTX "Software\Classes\SNAP.CommandFile" "" "SNAP command file"
  WriteRegStr SHCTX "Software\Classes\SNAP.CommandFile\DefaultIcon" "" "$INSTDIR\snap_manager.exe,0"
  WriteRegStr SHCTX "Software\Classes\SNAP.CommandFile\shell\open\command" "" '"$INSTDIR\snap_manager.exe" "%1"'
]])
        string(APPEND CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS [[
  Delete "$DESKTOP\Run SNAP.lnk"

  DeleteRegKey SHCTX "Software\Classes\.snp"
  DeleteRegKey SHCTX "Software\Classes\.snap"
  DeleteRegKey SHCTX "Software\Classes\SNAP.CommandFile"
]])
    endif()
    include(CPack)
endif()
