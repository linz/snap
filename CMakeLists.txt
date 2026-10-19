cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SNAP_VERSION)
string(STRIP "${SNAP_VERSION}" SNAP_VERSION)

project(snap
    VERSION ${SNAP_VERSION}
    DESCRIPTION "SNAP network adjustment software"
    LANGUAGES CXX)

# NO_POLICY_SCOPE keeps the policies that the rules set (CMP0167, CMP0144) for the rest of this project
include(cmake/buildRules.cmake NO_POLICY_SCOPE)

# ── Sub-projects ──────────────────────────────────────────────────────────────
# lapg generates src/snap_manager/scriptimp_parse.cpp from its .syntax grammar.
# The generated file is not checked in, so lapg is always built when the GUI is
# enabled.  SNAP_BUILD_LAPG allows building it standalone (e.g. to regenerate
# the file manually without a full GUI build).
option(SNAP_BUILD_LAPG "Build the lapg parser-generator tool" OFF)
if(SNAP_BUILD_LAPG OR SNAP_BUILD_GUI)
    add_subdirectory(tools/lapg)
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
# Run them with ctest. The unit tests are registered where they are built (src/test), and the
# regression tests, which run the built programs, are registered by regression_tests.
enable_testing()
include(cmake/SnapTests.cmake)

add_subdirectory(src)

if(SNAP_TESTS_CAN_RUN)
    add_subdirectory(regression_tests)
endif()

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
    # CPACK_NSIS_PACKAGE_NAME defaults to CPACK_NSIS_DISPLAY_NAME, but it also
    # seeds the Start Menu folder NSIS remembers across upgrades (unrelated to
    # its documented role as the installer title). Since installs are in-place
    # (fixed CPACK_PACKAGE_INSTALL_DIRECTORY, matching the old vdproj's fixed
    # DefaultLocation), that default must stay version-independent too, or
    # each upgrade's suggested Start Menu folder drifts to the last-installed
    # version's name instead of staying "SNAP".
    set(CPACK_NSIS_PACKAGE_NAME "SNAP")
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
