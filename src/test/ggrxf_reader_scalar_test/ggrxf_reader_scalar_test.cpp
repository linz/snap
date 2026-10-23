// Standalone unit test for the reading of the scalar_with_time_function layers of a GGRXF file, by parseGgrxfFile()
// (deformation/ggrxf_reader.h). It reads the files that src/test/ggrxf_files/write_ggrxf_files.py writes, whose
// directory is the first argument of the test, and checks the values that the layers of each file give, or the
// exception that reading the file throws. Failures are reported to stdout and the process exit code is the pass/fail
// signal (0 = all passed), as for the other test-only tools in src/test/.
//
// The grid of most of the files has 3 by 3 nodes, at 1 degree intervals from latitude 10 and longitude 20. The raw
// value of node (i, j) on axis a is 100 * (i + 1) + 10 * (j + 1) + (a + 1), which is scaled by 0.5, and then by the
// time function of the layer, which is 0 before 2000-01-01 and 1 from then on. At (10.5, 20.5), the middle of the cell
// of the first node, the raw values are 166, 167 and 168, which are 83, 83.5 and 84 once scaled.

#include <array>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "deformation/ggrxf_date.h"
#include "deformation/ggrxf_reader.h"
#include "deformation/layer.h"
#include "indexed_scalar_values.h"
#include "test_support.h"

namespace {

using deformation::ggrxfDateToDayNumber;
using deformation::GgrxfFile;
using deformation::parseGgrxfFile;
using snap_test::check;
using snap_test::checkThrows;
using snap_test::checkValues;

/// The values at the point (10.5, 20.5) of the first layer of most of the files, from a time function that is 1.
const std::array<double, 3> kValuesAtTheFirstCell = {83.0, 83.5, 84.0};

/// The directory of the files that the Python script wrote.
std::string filesDirectory;

/// \return the path of a file that the Python script wrote.
std::string pathOf(const std::string& name) {
    return (std::filesystem::path(filesDirectory) / name).string();
}

/// The days that the layers are evaluated on, which are on either side of the step of the layers of most of the files.
const double kBeforeStep = ggrxfDateToDayNumber("1999-12-31");
const double kAfterStep = ggrxfDateToDayNumber("2001-01-01");

/// Checks that reading the file throws a std::runtime_error whose message has the text.
void checkRejected(const std::string& name, const std::string& text, const std::string& messageOnError) {
    checkThrows<std::runtime_error>([&name] { return parseGgrxfFile(pathOf(name)); }, text, messageOnError);
}

/// Checks the values that the first layer of the file gives at a point on a day.
void checkValuesOf(const std::string& name, const double latitude, const double longitude, const double day,
                   const std::array<double, 3>& expected, const std::string& messageOnError) {
    const GgrxfFile file = parseGgrxfFile(pathOf(name));
    checkValues(file.layers.at(0)->evaluate(latitude, longitude, day), expected, messageOnError);
}

/// Checks that the first layer of the file gives no values at a point on a day.
void checkNoValuesOf(const std::string& name, const double latitude, const double longitude, const double day,
                     const std::string& messageOnError) {
    const GgrxfFile file = parseGgrxfFile(pathOf(name));
    check(!file.layers.at(0)->evaluate(latitude, longitude, day).has_value(), messageOnError);
}

void check_a_file_with_a_scalar_layer() {
    const GgrxfFile file = parseGgrxfFile(pathOf("scalar_basic.nc"));
    check(file.layers.size() == 1, "a file with one layer has one layer");
    check(file.modelVersion == "1.2.3", "the model version of a file that has one");
    const deformation::Layer& layer = *file.layers[0];
    checkValues(layer.evaluate(10.5, 20.5, kAfterStep), kValuesAtTheFirstCell, "a point in the grid, after the step");
    checkValues(layer.evaluate(10.5, 20.5, kBeforeStep), {0.0, 0.0, 0.0}, "a point in the grid, before the step");
    check(!layer.evaluate(50.0, 50.0, kAfterStep).has_value(), "a point outside the grid");
}

void check_the_model_version_is_optional() {
    check(!parseGgrxfFile(pathOf("scalar_no_model_version.nc")).modelVersion.has_value(),
          "a file that has no model version");
}

void check_a_file_with_no_layer() {
    check(parseGgrxfFile(pathOf("no_layers.nc")).layers.empty(), "a file with no layer has no layers");
}

void check_a_file_with_both_types_of_layer() {
    check(parseGgrxfFile(pathOf("mixed_layers.nc")).layers.size() == 2, "a file with a scalar and a ragged layer");
}

void check_coefficients_in_the_order_of_longitude_first() {
    // The grid and its values are those of scalar_basic.nc, but its file declares the longitude axis first and has its
    // coefficients in that order. The layers give (latitude, longitude) either way.
    checkValuesOf("scalar_longitude_first.nc", 10.5, 20.5, kAfterStep, kValuesAtTheFirstCell, "a point in the grid");
}

void check_files_that_cannot_be_read() {
    checkRejected("absent.nc", "nc_open", "a file that is not there");
    checkRejected("no_wkt.nc", "interpolationCrsWkt", "a file with no interpolation CRS");
    checkRejected("wkt_no_longitude.nc", "geodetic longitude", "an interpolation CRS with no longitude axis");
    checkRejected("wkt_no_latitude.nc", "geodetic latitude", "an interpolation CRS with no latitude axis");
}

void check_layers_that_cannot_be_read() {
    checkRejected("layer_bicubic.nc", "bicubic", "a layer that interpolates bicubically");
    checkRejected("layer_unknown_grid_type.nc", "sparse_points", "a layer of a type of grid that is not implemented");
}

void check_time_functions() {
    // The layer has a linear function of 0.25 a day from 2000-01-01, a step of 2 on 2001-01-01, and a ramp of 3 from
    // 2002-01-01 to 2002-01-05. On 2002-01-03, which is 733 days (366 + 365 + 2) after 2000-01-01, they are 183.25, 2
    // and 1.5, which is 186.75, and 186.75 times 83, 83.5 and 84 is the values that are expected.
    checkValuesOf("scalar_time_functions.nc", 10.5, 20.5, ggrxfDateToDayNumber("2002-01-03"),
                  {15500.25, 15593.625, 15687.0}, "all three time functions, part of the way along the ramp");
    // The linear function is -0.25, one day before it, and unbounded, and the step and the ramp have not started.
    checkValuesOf("scalar_time_functions.nc", 10.5, 20.5, kBeforeStep, {-20.75, -20.875, -21.0},
                  "the linear function before its date, when the others have not started");
    // On 2003-01-01 the linear function is 274 (1096 days), and the step and the ramp are 2 and 3.
    checkValuesOf("scalar_time_functions.nc", 10.5, 20.5, ggrxfDateToDayNumber("2003-01-01"),
                  {23157.0, 23296.5, 23436.0}, "all three time functions, after the end of the ramp");
    checkRejected("scalar_time_function_quadratic.nc", "quadratic", "a time function that is not implemented");
}

void check_grid_parameters() {
    // One raw axis, which is up, so its value goes to the third of the three values, and the others are 0.
    checkValuesOf("scalar_parameters_up.nc", 10.5, 20.5, kAfterStep, {0.0, 0.0, 83.0}, "a layer of up only");
    // The first raw axis is up and the second is east, so up is 83 on the third value, and east is 83.5 on the first.
    checkValuesOf("scalar_parameters_up_east.nc", 10.5, 20.5, kAfterStep, {83.5, 0.0, 83.0},
                  "a layer of up, then east");
    checkRejected("scalar_parameters_unknown.nc", "displacementWest", "a parameter that is not a displacement");
    checkRejected("scalar_parameters_count_mismatch.nc", "gridParameters lists 2",
                  "gridParameters of two for a displacement of three axes");
}

void check_missing_value_and_end_day() {
    // The node (2, 2) has the missing value, which is in the cell of (11.5, 21.5) only.
    checkValuesOf("scalar_missing_value.nc", 10.5, 20.5, kAfterStep, kValuesAtTheFirstCell,
                  "a cell whose nodes all have values");
    checkNoValuesOf("scalar_missing_value.nc", 11.5, 21.5, kAfterStep, "a cell with a node that has the missing value");
    checkRejected("scalar_end_day.nc", "not implemented", "a grid that has an end day");
}

void check_grids_with_a_wrong_shape() {
    checkRejected("scalar_affine_coeffs_length.nc", "has 5 elements", "affineCoeffs of 5 numbers");
    checkRejected("scalar_displacement_dimensions.nc", "2 dimensions", "a displacement of 2 dimensions");
    checkRejected("scalar_displacement_nodes.nc", "2 x 3 nodes",
                  "a displacement whose nodes are not iNodeCount by jNodeCount");
    checkRejected("scalar_displacement_axes.nc", "has 2 axes", "a displacement of 2 axes for a layer of 3");
}

void check_root_grids_are_in_the_order_of_the_numbers_in_their_names() {
    // The grids grid_2 and grid_10 cover the same points, and the values of grid_10, which was written first, are 1000
    // more. By number grid_2 is first, and the values are those of grid_2.
    checkValuesOf("scalar_file_order.nc", 10.5, 20.5, kAfterStep, kValuesAtTheFirstCell,
                  "grid_2, which is before grid_10 by number, and after it by text and in the file");
    checkRejected("scalar_group_name.nc", "naming convention", "a root grid whose name is not grid_<N>");
    // The text of the exception of a name that is grid_ and then text that is not a number is the library's own.
    checkThrows<std::invalid_argument>([] { return parseGgrxfFile(pathOf("scalar_group_number.nc")); }, "",
                                       "a root grid whose name is grid_ and then text that is not a number");
}

void check_nested_grids() {
    // The grid of 5 by 5 nodes covers latitude 10 to 14 and longitude 20 to 24. Inside it, a (priority 1, values 1000
    // more) covers 11 to 13 by 21 to 23, and b (priority 2, values 2000 more) covers 12 to 14 by 22 to 24. The file has
    // b before a, so the order of the grids in the file is not the order of their priorities, and the value where they
    // overlap is b's only if the reader orders them by priority.
    const std::string name = "scalar_nested.nc";
    checkValuesOf(name, 10.5, 20.5, kAfterStep, kValuesAtTheFirstCell, "a point in the grid only");
    checkValuesOf(name, 11.5, 21.5, kAfterStep, {583.0, 583.5, 584.0}, "a point in a as well");
    checkValuesOf(name, 13.5, 23.5, kAfterStep, {1138.0, 1138.5, 1139.0}, "a point in b as well");
    checkValuesOf(name, 12.5, 22.5, kAfterStep, {1083.0, 1083.5, 1084.0}, "a point in a and b, where b has priority");
}

void check_nests_with_wrong_priorities() {
    checkRejected("scalar_nest_root_no_priority.nc", "no gridPriority", "a nest root with no priority");
    checkRejected("scalar_nest_root_priority_1.nc", "expected 0", "a nest root whose priority is not 0");
    checkRejected("scalar_nest_leaf_has_priority.nc", "carries gridPriority",
                  "a grid with no child grids that has one");
    checkRejected("scalar_nest_child_no_priority.nc", "required for every descendant", "a child with no priority");
    checkRejected("scalar_nest_child_priority_0.nc", "gridPriority 0", "a child whose priority is 0");
    checkRejected("scalar_nest_child_priority_range.nc", "gridPriority 3", "a child whose priority is out of range");
    checkRejected("scalar_nest_child_priority_twice.nc", "already used", "two children that have the same priority");
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: ggrxf_reader_scalar_test <directory of the GGRXF files>\n";
        return 2;
    }
    filesDirectory = argv[1];

    try {
        check_a_file_with_a_scalar_layer();
        check_the_model_version_is_optional();
        check_a_file_with_no_layer();
        check_a_file_with_both_types_of_layer();
        check_coefficients_in_the_order_of_longitude_first();
        check_files_that_cannot_be_read();
        check_layers_that_cannot_be_read();
        check_time_functions();
        check_grid_parameters();
        check_missing_value_and_end_day();
        check_grids_with_a_wrong_shape();
        check_root_grids_are_in_the_order_of_the_numbers_in_their_names();
        check_nested_grids();
        check_nests_with_wrong_priorities();
    } catch (const std::exception& error) {
        check(false, std::string("unexpected exception: ") + error.what());
    }

    return snap_test::finish("ggrxf_reader_scalar_test");
}
