# Settings and helper functions for registering the project's tests with ctest.
#
# Every test is a ctest test, so ctest can run them in parallel, a failure names the test that failed, and
# a test that hangs is stopped by its TIMEOUT. The timeouts are only meant to catch a test that is far
# slower than it should be, not to judge how fast the tests are, so they must allow for a slow CI runner.
#
# The files that a test leaves behind are under SNAP_TEST_OUTPUT_DIR, at the path that the test's suite has
# in the repository. A group of tests that share a directory there have a setup test that empties it before
# any of them runs, so a run never sees what an earlier run left, and each test makes and empties its own
# directory under it.

set(SNAP_UNIT_TEST_TIMEOUT 30 CACHE STRING
    "Seconds a unit test may run before ctest stops it")
set(SNAP_REGRESSION_TEST_TIMEOUT 60 CACHE STRING
    "Seconds a regression test may run before ctest stops it")
set(SNAP_TEST_OUTPUT_DIR "${CMAKE_BINARY_DIR}/test_output" CACHE PATH
    "Where the files that the tests leave behind are put")

# EXPERIMENTAL, AND NOT WORKING YET: a cross compiled program can only be tested if the machine that builds
# it can run it, as WSL2 can run a program built for Windows. At the time of writing, a MinGW build tested from
# WSL2 fails 284 of 318 regression tests, because the path of the coordinate system file that WSL2 gives a
# Windows program, \\wsl.localhost\..., is a UNC path, and normalize_path in src/snaplib/util/fileutil.cpp
# (added by #168) loses the second leading backslash, so the grid files next to that file are not found. With
# paths on a Windows drive, 11 of the 318 tests still fail, mostly because of paths in the output. This keeps
# the mechanism in place, for when that is fixed. It, and SNAP_TEST_ENVIRONMENT below, are all there is to it.
option(SNAP_TEST_CROSS_BUILD
    "EXPERIMENTAL, NOT WORKING YET: register the tests of a cross compiled build, to run on the machine that builds it"
    OFF)
if(NOT CMAKE_CROSSCOMPILING OR SNAP_TEST_CROSS_BUILD)
    set(SNAP_TESTS_CAN_RUN TRUE)
else()
    set(SNAP_TESTS_CAN_RUN FALSE)
endif()

# WSL2 gives a program built for Windows the Windows form of the paths in the variables that WSLENV lists
# with /p, and COORDSYSDEF, the file of coordinate systems the programs read, is a path.
set(SNAP_TEST_ENVIRONMENT "")
if(CMAKE_CROSSCOMPILING)
    set(_wslenv "COORDSYSDEF/p")
    if(NOT "$ENV{WSLENV}" STREQUAL "")
        string(APPEND _wslenv ":$ENV{WSLENV}")
    endif()
    set(SNAP_TEST_ENVIRONMENT "WSLENV=${_wslenv}")
endif()

set(SNAP_REGRESSION_TESTS_DIR "${CMAKE_SOURCE_DIR}/regression_tests")

# Registers the setup test of a fixture, which empties a directory before the tests that require the fixture
# run, and then makes the directories that the tests need to exist before they start, if there are any.
# A test requires the fixture with FIXTURES_REQUIRED <name>-output.
#
#   snap_add_output_setup(<name> <directory> [CREATE <directory>...])
function(snap_add_output_setup name directory)
    cmake_parse_arguments(PARSE_ARGV 2 SETUP "" "" "CREATE")
    add_test(NAME "${name}/clear-output" COMMAND "${CMAKE_COMMAND}" -E remove_directory "${directory}")
    set(setup_tests "${name}/clear-output")
    if(SETUP_CREATE)
        add_test(NAME "${name}/create-output" COMMAND "${CMAKE_COMMAND}" -E make_directory ${SETUP_CREATE})
        set_tests_properties("${name}/create-output" PROPERTIES DEPENDS "${name}/clear-output")
        list(APPEND setup_tests "${name}/create-output")
    endif()
    set_tests_properties(${setup_tests} PROPERTIES
        FIXTURES_SETUP "${name}-output"
        LABELS setup
        TIMEOUT ${SNAP_REGRESSION_TEST_TIMEOUT})
endfunction()

# Builds a standalone unit test, and registers it with ctest. The test passes if the executable's exit
# code is 0. The test executable is also added to the unit_tests target, which builds all of them.
# The test's temporary files go in its own directory under SNAP_TEST_OUTPUT_DIR, by way of the environment.
# snap_add_unit_test_setup must be called once all the unit tests have been added.
#
#   snap_add_unit_test(<name> SOURCES <source>... [INCLUDE_DIRECTORIES <dir>...] [TIMEOUT <seconds>])
function(snap_add_unit_test name)
    cmake_parse_arguments(PARSE_ARGV 1 TEST "" "TIMEOUT" "SOURCES;INCLUDE_DIRECTORIES")
    add_executable(${name} ${TEST_SOURCES})
    target_link_libraries(${name} PRIVATE snaplib snap_link_deps)
    if(TEST_INCLUDE_DIRECTORIES)
        target_include_directories(${name} PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    endif()
    set_property(GLOBAL APPEND PROPERTY SNAP_UNIT_TESTS ${name})

    if(SNAP_TESTS_CAN_RUN)
        if(NOT TEST_TIMEOUT)
            set(TEST_TIMEOUT ${SNAP_UNIT_TEST_TIMEOUT})
        endif()
        set(output "${SNAP_TEST_OUTPUT_DIR}/src/test/${name}/output")
        set_property(GLOBAL APPEND PROPERTY SNAP_UNIT_TEST_OUTPUT_DIRS "${output}")
        add_test(NAME ${name} COMMAND ${name})
        set_tests_properties(${name} PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
            LABELS unit
            FIXTURES_REQUIRED unit-tests-output
            ENVIRONMENT "TMPDIR=${output};TMP=${output};TEMP=${output}")
    endif()
endfunction()

# Registers the setup test of the unit tests, which empties their directory under SNAP_TEST_OUTPUT_DIR and
# makes the directory of each test, as std::filesystem::temp_directory_path fails if the directory it is
# told to use does not exist.
function(snap_add_unit_test_setup)
    if(SNAP_TESTS_CAN_RUN)
        get_property(output_dirs GLOBAL PROPERTY SNAP_UNIT_TEST_OUTPUT_DIRS)
        snap_add_output_setup(unit-tests "${SNAP_TEST_OUTPUT_DIR}/src/test" CREATE ${output_dirs})
    endif()
endfunction()

# Registers a regression test that runs a command, which passes if its exit code is 0. FIXTURE is the suite
# whose setup test has to run first, because the test leaves files in the suite's output directory.
#
#   snap_add_regression_command(<name> [FIXTURE <suite>] COMMAND <command>...)
function(snap_add_regression_command name)
    cmake_parse_arguments(PARSE_ARGV 1 TEST "" "FIXTURE" "COMMAND")
    add_test(NAME "${name}" COMMAND ${TEST_COMMAND})
    set_tests_properties("${name}" PROPERTIES
        TIMEOUT ${SNAP_REGRESSION_TEST_TIMEOUT}
        LABELS regression
        ENVIRONMENT "${SNAP_TEST_ENVIRONMENT}")
    if(TEST_FIXTURE)
        set_tests_properties("${name}" PROPERTIES FIXTURES_REQUIRED "${TEST_FIXTURE}-output")
    endif()
endfunction()

# Registers every test of a suite of regression tests, which are run by run_regression_test.py, with the
# setup test that empties the suite's output directory, and a test that all the suite's check data belongs
# to a test. The tests are found by asking the script, so a test that is added to the suite is registered
# the next time CMake runs, which it does by itself when a file is added to or removed from the suite's "in"
# or "check" directories. The script puts the output of a test at the path of its suite from the repository.
#
# A suite can be registered again, to run its tests with other parameters. NAME is what the suite is called
# then, in the names of its tests and in the path of their output, and PARAMETERS go in front of the parameters
# of every test. For example snap_add_regression_suite(snapspec NAME snapspec_k PARAMETERS -k) runs the
# snapspec tests with snapspec's -k option, and calls them snapspec_k/<test>. The "check data" test is
# not added again though, as it is the same check data.
#
#   snap_add_regression_suite(<suite> [NAME <name> PARAMETERS <parameters>])
function(snap_add_regression_suite suite)
    cmake_parse_arguments(PARSE_ARGV 1 SUITE "" "NAME;PARAMETERS" "")
    set(config "${SNAP_REGRESSION_TESTS_DIR}/${suite}/test.config")
    set(runner "${SNAP_REGRESSION_TESTS_DIR}/run_regression_test.py")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${runner}" --config "${config}" --list
        OUTPUT_VARIABLE names
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY)
    string(REPLACE "\n" ";" names "${names}")

    # A suite that is run again with other parameters is a suite of its own, with its own output
    set(name ${suite})
    set(extra_arguments "")
    if(SUITE_NAME)
        set(name ${SUITE_NAME})
        set(extra_arguments --suite-name "${name}")
    endif()
    if(SUITE_PARAMETERS)
        list(APPEND extra_arguments "--extra-parameters=${SUITE_PARAMETERS}")
    endif()

    snap_add_output_setup(${name} "${SNAP_TEST_OUTPUT_DIR}/regression_tests/${name}")
    foreach(test IN LISTS names)
        snap_add_regression_command("${name}/${test}" FIXTURE ${name} COMMAND
            Python3::Interpreter "${runner}" --config "${config}" --test "${test}"
            --build-dir "$<TARGET_FILE_DIR:snap>" --output-dir "${SNAP_TEST_OUTPUT_DIR}" ${extra_arguments})
    endforeach()
    if(NOT SUITE_NAME)
        snap_add_regression_command("${suite}/unused-check-data" COMMAND
            Python3::Interpreter "${runner}" --config "${config}" --check-unused)
    endif()

    file(GLOB_RECURSE _suite_files CONFIGURE_DEPENDS
        "${SNAP_REGRESSION_TESTS_DIR}/${suite}/in/*" "${SNAP_REGRESSION_TESTS_DIR}/${suite}/check/*")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${config}" "${runner}")
endfunction()
