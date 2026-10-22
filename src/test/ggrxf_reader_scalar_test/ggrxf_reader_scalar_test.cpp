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

#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

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

/// The directory of the files that the Python script wrote.
std::string filesDirectory;

/// \return the path of a file that the Python script wrote.
std::string pathOf(const std::string& name) {
    return (std::filesystem::path(filesDirectory) / name).string();
}

/// The days that the layers are evaluated on, which are on either side of the step of the layers of most of the files.
const double kBeforeStep = ggrxfDateToDayNumber("1999-12-31");
const double kAfterStep = ggrxfDateToDayNumber("2001-01-01");

void check_a_file_with_a_scalar_layer() {
    const GgrxfFile file = parseGgrxfFile(pathOf("scalar_basic.nc"));
    check(file.layers.size() == 1, "a file with one layer has one layer");
    check(file.modelVersion == "1.2.3", "the model version of a file that has one");
    const deformation::Layer& layer = *file.layers[0];
    checkValues(layer.evaluate(10.5, 20.5, kAfterStep), {83.0, 83.5, 84.0}, "a point in the grid, after the step");
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
    const GgrxfFile file = parseGgrxfFile(pathOf("scalar_longitude_first.nc"));
    // The grid and its values are those of scalar_basic.nc, but its file declares the longitude axis first and has its
    // coefficients in that order. The layers give (latitude, longitude) either way.
    checkValues(file.layers[0]->evaluate(10.5, 20.5, kAfterStep), {83.0, 83.5, 84.0}, "a point in the grid");
}

void check_files_that_cannot_be_read() {
    checkThrows<std::runtime_error>([] { return parseGgrxfFile(pathOf("absent.nc")); }, "nc_open",
                                    "a file that is not there");
    checkThrows<std::runtime_error>([] { return parseGgrxfFile(pathOf("no_wkt.nc")); }, "interpolationCrsWkt",
                                    "a file with no interpolation CRS");
    checkThrows<std::runtime_error>([] { return parseGgrxfFile(pathOf("wkt_no_longitude.nc")); },
                                    "geodetic longitude", "an interpolation CRS with no longitude axis");
    checkThrows<std::runtime_error>([] { return parseGgrxfFile(pathOf("wkt_no_latitude.nc")); }, "geodetic latitude",
                                    "an interpolation CRS with no latitude axis");
}

void check_layers_that_cannot_be_read() {
    checkThrows<std::runtime_error>([] { return parseGgrxfFile(pathOf("layer_bicubic.nc")); }, "bicubic",
                                    "a layer that interpolates bicubically");
    checkThrows<std::runtime_error>([] { return parseGgrxfFile(pathOf("layer_unknown_grid_type.nc")); },
                                    "sparse_points", "a layer of a type of grid that is not implemented");
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
    } catch (const std::exception& error) {
        check(false, std::string("unexpected exception: ") + error.what());
    }

    return snap_test::finish("ggrxf_reader_scalar_test");
}
