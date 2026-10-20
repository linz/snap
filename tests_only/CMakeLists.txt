# Runs the tests against programs that have already been built, so it needs no compiler and none of the
# libraries that the programs need. It registers the same tests as the project that builds the programs, with
# the programs, the unit tests and binroundtrip taken from SNAP_PROGRAMS_DIR.
#
#   cmake -S tests_only -B <build dir> -DSNAP_PROGRAMS_DIR=<absolute path of the programs>
#   ctest --test-dir <build dir>
cmake_minimum_required(VERSION 3.20)

project(snap_tests LANGUAGES NONE)

if(NOT SNAP_PROGRAMS_DIR)
    message(FATAL_ERROR "SNAP_PROGRAMS_DIR must be the directory of the built programs")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

enable_testing()
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/SnapTests.cmake")

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../src/test" src/test)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../regression_tests" regression_tests)
