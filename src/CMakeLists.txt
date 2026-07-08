# ── git ls-files helper ───────────────────────────────────────────────────────
# Populates <var> with absolute paths of all tracked .cpp files under <path>
# (relative to CMAKE_CURRENT_SOURCE_DIR).  CMake re-configures automatically
# when the git index changes (i.e. after git add / git rm).
# HINTS cover VS's bundled git, which isn't on PATH during cmake configure.
# On Linux the env vars are unset so the hints are ignored and git is found via PATH.
find_program(GIT_EXECUTABLE git
    HINTS
        "$ENV{VSINSTALLDIR}Common7/IDE/CommonExtensions/Microsoft/TeamFoundation/Team Explorer/Git/cmd"
        "$ENV{ProgramFiles}/Git/cmd"
        "$ENV{ProgramW6432}/Git/cmd"
    REQUIRED)

set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY
    CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/.git/index")

function(git_sources var path)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" ls-files -- ":(glob)${path}**/*.cpp"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE _raw
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY)
    string(REPLACE "\n" ";" _files "${_raw}")
    list(TRANSFORM _files PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
    set(${var} "${_files}" PARENT_SCOPE)
endfunction()

# ── Global include paths ──────────────────────────────────────────────────────
# Mirrors the INCPATH variable in linux/makefile.
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/snaplib
    ${CMAKE_CURRENT_SOURCE_DIR}/snaplib/util
    ${CMAKE_CURRENT_SOURCE_DIR}/snaplib/dbl4
    ${CMAKE_CURRENT_SOURCE_DIR}/snap
    ${CMAKE_CURRENT_SOURCE_DIR}/snapspec
    ${CMAKE_CURRENT_SOURCE_DIR}/wxutils
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# ── Common link dependencies ──────────────────────────────────────────────────
add_library(snap_link_deps INTERFACE)
target_include_directories(snap_link_deps INTERFACE ${Boost_INCLUDE_DIRS})
target_link_libraries(snap_link_deps INTERFACE
    Boost::filesystem
    Boost::regex
    ${BOOST_SYSTEM_LIB}
    Threads::Threads
    $<$<PLATFORM_ID:Linux>:m>)

# ── snaplib ───────────────────────────────────────────────────────────────────
git_sources(SNAPLIB_SOURCES "snaplib/")
add_library(snaplib STATIC ${SNAPLIB_SOURCES})
target_link_libraries(snaplib PUBLIC snap_link_deps)

# ── snap_core (OBJECT library shared by snap and snapadjust) ──────────────────
# snapadjust reuses all of snap's translation units except main.cpp — this
# object library avoids compiling them twice.
git_sources(SNAP_SOURCES "snap/")
list(FILTER SNAP_SOURCES EXCLUDE REGEX "[/\\\\]main\\.cpp$")
add_library(snap_core OBJECT ${SNAP_SOURCES})
target_link_libraries(snap_core PUBLIC snaplib)

# ── Command-line executables ──────────────────────────────────────────────────
add_executable(snap $<TARGET_OBJECTS:snap_core> snap/main.cpp)
target_link_libraries(snap PRIVATE snaplib snap_link_deps)

add_executable(concord concord/concord.cpp)
target_link_libraries(concord PRIVATE snaplib snap_link_deps)

git_sources(SNAPSPEC_SOURCES "snapspec/")
add_executable(snapspec ${SNAPSPEC_SOURCES})
target_link_libraries(snapspec PRIVATE snaplib snap_link_deps)

add_executable(dat2site dat2site/dat2site.cpp)
target_link_libraries(dat2site PRIVATE snaplib snap_link_deps)

add_executable(snaplist snaplist/snaplist.cpp)
target_link_libraries(snaplist PRIVATE snaplib snap_link_deps)

add_executable(snapconv snapconv/snapconv.cpp)
target_link_libraries(snapconv PRIVATE snaplib snap_link_deps)

add_executable(snapgeoid snapgeoid/snapgeoid.cpp)
target_link_libraries(snapgeoid PRIVATE snaplib snap_link_deps)

add_executable(snapmerge snapmerge/snapmerge.cpp)
target_link_libraries(snapmerge PRIVATE snaplib snap_link_deps)

add_executable(site2gps site2gps/site2gps.cpp)
target_link_libraries(site2gps PRIVATE snaplib snap_link_deps)

# ── wxWidgets GUI targets ─────────────────────────────────────────────────────
if(SNAP_BUILD_GUI)
    include(${wxWidgets_USE_FILE})

    git_sources(WXUTILS_SOURCES "wxutils/")
    add_library(wxutils STATIC ${WXUTILS_SOURCES})
    target_link_libraries(wxutils PUBLIC snaplib ${wxWidgets_LIBRARIES})

    # snap_manager — scriptimp_parse.cpp is generated from its .syntax grammar
    # by lapg at build time (the file is not checked in).
    set(_parse_syntax ${CMAKE_CURRENT_SOURCE_DIR}/snap_manager/scriptimp_parse.syntax)
    set(_parse_cpp    ${CMAKE_CURRENT_SOURCE_DIR}/snap_manager/scriptimp_parse.cpp)
    add_custom_command(
        OUTPUT  ${_parse_cpp}
        COMMAND $<TARGET_FILE:lapg> ${_parse_syntax} ${_parse_cpp}
        DEPENDS lapg ${_parse_syntax}
        COMMENT "Generating scriptimp_parse.cpp from grammar")

    git_sources(SNAP_MANAGER_SOURCES "snap_manager/")
    list(APPEND SNAP_MANAGER_SOURCES ${_parse_cpp})
    add_executable(snap_manager WIN32
        ${SNAP_MANAGER_SOURCES}
        $<$<BOOL:${WIN32}>:snap_manager/snap_manager.rc>)
    target_link_libraries(snap_manager PRIVATE wxutils snaplib snap_link_deps)
    target_include_directories(snap_manager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/resources)

    add_executable(snapadjust WIN32
        snapadjust/snapadjust_app.cpp
        $<TARGET_OBJECTS:snap_core>
        $<$<BOOL:${WIN32}>:snapadjust/snapadjust.rc>)
    target_link_libraries(snapadjust PRIVATE wxutils snaplib snap_link_deps)
    target_include_directories(snapadjust PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/resources)

    git_sources(SNAPPLOT_SOURCES "snapplot/")
    add_executable(snapplot WIN32
        ${SNAPPLOT_SOURCES}
        $<$<BOOL:${WIN32}>:snapplot/snapplot.rc>)
    target_link_libraries(snapplot PRIVATE wxutils snaplib snap_link_deps)
    target_include_directories(snapplot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/resources)
endif()

# ── Install ───────────────────────────────────────────────────────────────────
set(SNAP_CMD_TARGETS snap concord snapspec dat2site snaplist snapconv
                     snapgeoid snapmerge site2gps)
set(SNAP_GUI_TARGETS "")
if(SNAP_BUILD_GUI)
    list(APPEND SNAP_GUI_TARGETS snap_manager snapadjust snapplot)
endif()

install(TARGETS ${SNAP_CMD_TARGETS} ${SNAP_GUI_TARGETS}
    RUNTIME DESTINATION ${SNAP_INSTALL_ROOT})

install(DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/snap/config/
        ${CMAKE_CURRENT_SOURCE_DIR}/snapspec/config/
        ${CMAKE_CURRENT_SOURCE_DIR}/snaplist/config/
        ${CMAKE_CURRENT_SOURCE_DIR}/snap_manager/config/
    DESTINATION ${SNAP_INSTALL_ROOT}/config)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/perl/
    DESTINATION ${SNAP_INSTALL_ROOT}/config/perl)

install(FILES ${CMAKE_SOURCE_DIR}/VERSION
    DESTINATION ${SNAP_INSTALL_ROOT})

# Invoked by the NSIS installer (see CPACK_NSIS_EXTRA_INSTALL/UNINSTALL_COMMANDS)
# to add/remove SNAP_INSTALL_ROOT from the system PATH.
if(WIN32)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/snap_setup.js
        DESTINATION ${SNAP_INSTALL_ROOT})
endif()

install(CODE "
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY \"${CMAKE_SOURCE_DIR}\"
        OUTPUT_VARIABLE _versionid
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(_versionid)
        file(WRITE \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${SNAP_INSTALL_ROOT}/VERSIONID\" \"\${_versionid}\")
    endif()
")

install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/help/help/
    DESTINATION ${SNAP_INSTALL_ROOT}/help)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/snap.ico
    DESTINATION ${SNAP_INSTALL_ROOT}/help
    RENAME favicon.ico)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/packages/devel/
        DESTINATION ${SNAP_INSTALL_ROOT}/config/package/devel)
endif()

# Coordsys data: on Linux this comes from a separate Debian package, so we
# just symlink to it; Windows has no equivalent package, so install the data
# directly.
if(UNIX)
    install(CODE "
        set(_link \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${SNAP_INSTALL_ROOT}/config/coordsys\")
        if(NOT EXISTS \"\${_link}\")
            file(CREATE_LINK \"${SNAP_COORDSYSDIR}\" \"\${_link}\" SYMBOLIC)
        endif()
    ")
else()
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/coordsys/
        DESTINATION ${SNAP_INSTALL_ROOT}/config/coordsys)
endif()
