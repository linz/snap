# The regression tests, which run the programs that are built from the sources and check what they write.
# Each test is a ctest test, registered by the functions in cmake/SnapTests.cmake.

# Suites of tests defined by a test.config, each run by run_regression_test.py.
foreach(suite concord dat2site snap snapconv snapgeoid snapmerge snapspec)
    snap_add_regression_suite(${suite})
endforeach()

# The snapspec tests again, with snapspec's -k option, which uses a different spatial index
snap_add_regression_suite(snapspec NAME snapspec_k PARAMETERS -k)

# How concord reads its input, with a test for each case of the script that runs them. They leave their
# output in the directory of the concord suite, which is emptied by the suite's setup test.
set(_concord_script "${CMAKE_CURRENT_SOURCE_DIR}/concord/run_concord_test.py")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${_concord_script}" --list
    OUTPUT_VARIABLE _concord_cases
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
string(REPLACE "\n" ";" _concord_cases "${_concord_cases}")
foreach(case IN LISTS _concord_cases)
    snap_add_regression_command("concord/input/${case}" FIXTURE concord COMMAND
        Python3::Interpreter "${_concord_script}" --new "${SNAP_PROGRAMS_DIR}/concord${CMAKE_EXECUTABLE_SUFFIX}"
        --case "${case}"
        --output-dir "${SNAP_TEST_OUTPUT_DIR}")
endforeach()
snap_add_regression_command("concord/input/unused-results" COMMAND
    Python3::Interpreter "${_concord_script}" --check-unused)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_concord_script}")

# The .bin format round trips byte for byte
snap_add_output_setup(binroundtrip "${SNAP_TEST_OUTPUT_DIR}/regression_tests/binroundtrip")
snap_add_regression_command(binroundtrip FIXTURE binroundtrip COMMAND
    Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/binroundtrip/run_binroundtrip_test.py"
    --build-dir "${SNAP_PROGRAMS_DIR}" --output-dir "${SNAP_TEST_OUTPUT_DIR}")
