# Test-only executables, which are not shipped applications. Each unit test is a standalone program whose
# exit code is the pass/fail signal. snap_add_unit_test (cmake/SnapTests.cmake) builds it and registers it
# with ctest.

# Keep the executables in the directory of the programs they test, not in this one
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/..")

# Verifies the .bin format round-trips byte-for-byte. See BINFILE_FORMAT.md.
snap_add_test_program(binroundtrip SOURCES binroundtrip/binroundtrip.cpp)

# Unit test for util/fieldscanner.hpp.
snap_add_unit_test(fieldscanner_test SOURCES fieldscanner_test/fieldscanner_test.cpp)

# Unit test for util/iostring.cpp.
snap_add_unit_test(iostring_test SOURCES iostring_test/iostring_test.cpp)

# Unit test for stncodecmp in network/netlist.cpp.
snap_add_unit_test(stncodecmp_test SOURCES stncodecmp_test/stncodecmp_test.cpp)

# Unit test for util/fileutil.cpp.
snap_add_unit_test(fileutil_test SOURCES fileutil_test/fileutil_test.cpp)

# Unit test for util/readcfg_internal.h's filter_line()/cap_line().
snap_add_unit_test(readcfg_test SOURCES readcfg_test/readcfg_test.cpp)

# Unit test for util/wildcard.cpp.
snap_add_unit_test(wildcard_test SOURCES wildcard_test/wildcard_test.cpp)

# Unit test for util/dms.cpp.
snap_add_unit_test(dms_test SOURCES dms_test/dms_test.cpp)

# Unit test for util/textformat.hpp.
snap_add_unit_test(textformat_test SOURCES textformat_test/textformat_test.cpp)

# Unit test for util/writecsv.cpp.
snap_add_unit_test(writecsv_test SOURCES writecsv_test/writecsv_test.cpp)

# Unit test for the station selection criteria in network/networkl.cpp.
snap_add_unit_test(criteria_test SOURCES criteria_test/criteria_test.cpp)

# Unit test for the observation modifications in snapdata/obsmod.cpp.
snap_add_unit_test(obsmod_test SOURCES obsmod_test/obsmod_test.cpp)

# Unit test for the CommandFile class in snaplib/snap/snapglob.cpp.
snap_add_unit_test(commandfile_test SOURCES commandfile_test/commandfile_test.cpp)

# Unit test for util/errdef.cpp.
snap_add_unit_test(errdef_test SOURCES errdef_test/errdef_test.cpp)

# Unit test for the StationCode class in snaplib/network/network.h.
snap_add_unit_test(stationcode_test SOURCES stationcode_test/stationcode_test.cpp)

# Unit test for the rotmat class in snaplib/util/geodetic.h.
snap_add_unit_test(rotmat_test SOURCES rotmat_test/rotmat_test.cpp)

# Unit test for the classification functions of the station class in snaplib/network/network.h.
snap_add_unit_test(station_test SOURCES station_test/station_test.cpp)

# Unit test for the StringLimited class in snaplib/util/stringlimited.hpp.
snap_add_unit_test(stringlimited_test SOURCES stringlimited_test/stringlimited_test.cpp)

# Unit test for the RaggedLayer class in snaplib/deformation/ragged_layer.h.
snap_add_unit_test(ragged_layer_test SOURCES ragged_layer_test/ragged_layer_test.cpp)

# Unit test for the RectangleIndex class in snaplib/deformation/rectangle_index.h.
snap_add_unit_test(rectangle_index_test SOURCES rectangle_index_test/rectangle_index_test.cpp)

# Unit test for the checked scalar reads of the netCDF wrapper in snaplib/deformation/netcdf_wrapper.h. It
# writes a small netCDF file, reads it back and deletes it.
snap_add_unit_test(netcdf_wrapper_test SOURCES netcdf_wrapper_test/netcdf_wrapper_test.cpp)

# Unit test for the RaggedGridData class in snaplib/deformation/ragged_grid_data.h.
snap_add_unit_test(ragged_grid_data_test SOURCES ragged_grid_data_test/ragged_grid_data_test.cpp)

# Unit test for the validation of the TimeFunction class in snaplib/deformation/time_function.h.
snap_add_unit_test(time_function_test SOURCES time_function_test/time_function_test.cpp)

# Unit test for the GridGeometry class in snaplib/deformation/grid_geometry.h.
snap_add_unit_test(grid_geometry_test SOURCES grid_geometry_test/grid_geometry_test.cpp)

# Unit test for the Grid and GridParent classes and descendToLeaf in snaplib/deformation/grid.h.
snap_add_unit_test(grid_test SOURCES grid_test/grid_test.cpp)

# Unit test for the LineReader class that concord uses to read its input. The class is part of the concord
# program, not snaplib, so its source is compiled into the test.
snap_add_unit_test(linereader_test
    SOURCES linereader_test/linereader_test.cpp ../concord/linereader.cpp
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/../concord)

# Unit test for the date and time parsing of snap_datetime_parse in util/dateutil.cpp.
snap_add_unit_test(dateutil_test SOURCES dateutil_test/dateutil_test.cpp)

# Unit test for the ISO 8601 dates of a GGRXF file in deformation/ggrxf_date.cpp.
snap_add_unit_test(ggrxf_date_test SOURCES ggrxf_date_test/ggrxf_date_test.cpp)

# Unit test for the parts that datetimeparser (util/datetimeparser.cpp) reads a date and time into, and why
# it can not read text.
snap_add_unit_test(datetimeparser_test SOURCES datetimeparser_test/datetimeparser_test.cpp)

# Unit test for RangedInt in util/rangedint.h.
snap_add_unit_test(rangedint_test SOURCES rangedint_test/rangedint_test.cpp)

# The setup test that gives each unit test the directory its temporary files go in, and the unit_tests target
# that builds them all
snap_add_unit_test_setup()
