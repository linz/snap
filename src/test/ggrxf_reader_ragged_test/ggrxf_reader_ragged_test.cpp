// Standalone unit test for the reading of the ragged_segments layers of a GGRXF file, by parseGgrxfFile()
// (deformation/ggrxf_reader.h). It reads the files that src/test/ggrxf_files/write_ggrxf_files.py writes, whose
// directory is the first argument of the test, and checks the values that the layers of each file give, or the
// exception that reading the file throws. Failures are reported to stdout and the process exit code is the pass/fail
// signal (0 = all passed), as for the other test-only tools in src/test/.
//
// The grid of most of the files has 3 by 3 nodes, at 1 degree intervals from latitude 10 and longitude 20, and every
// node of it has the reference day 0 and the same segments. On the east axis there is one segment, which starts at the
// raw value 1000 and rises by the raw scale factor 365. On the north axis there are two, a constant at the raw value
// 2000 from day 0, and a constant at the raw value 3000 from day 100. There are none on the up axis. A raw start value
// is scaled by 0.01, and a raw scale factor is scaled by 0.01 and divided by 365.2425, to be a rate a day. So, a number
// of days after the reference date of the layer, which is 2000-01-01, the values are
//
//     east  = 10 + 365 * 0.01 / 365.2425 * days
//     north = 20 before day 100, and 30 from then on
//     up    = 0

#include <array>
#include <cstddef>
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

/// The rate a day of the east values of most of the files, from the raw scale factor 365.
constexpr double kEastRatePerDay = 365 * 0.01 / 365.2425;

/// The directory of the files that the Python script wrote.
std::string filesDirectory;

/// \return the path of a file that the Python script wrote.
std::string pathOf(const std::string& name) {
    return (std::filesystem::path(filesDirectory) / name).string();
}

/// \return the day number that is a number of days after 2000-01-01, the reference date of the layers of the files.
double dayNumber(const double daysAfterReferenceDate) {
    return ggrxfDateToDayNumber("2000-01-01") + daysAfterReferenceDate;
}

/// \return the values of the grid of most of the files, on a number of days after the reference date, which has to be
/// at least 0.
std::array<double, 3> valuesOnDay(const double daysAfterReferenceDate) {
    return {10.0 + kEastRatePerDay * daysAfterReferenceDate, daysAfterReferenceDate < 100.0 ? 20.0 : 30.0, 0.0};
}

/// Checks that reading the file throws a std::runtime_error whose message has the text.
void checkRejected(const std::string& name, const std::string& text, const std::string& messageOnError) {
    checkThrows<std::runtime_error>([&name] { return parseGgrxfFile(pathOf(name)); }, text, messageOnError);
}

/// Checks the values that a layer of the file gives at a point on a day.
void checkValuesOf(const std::string& name, const std::size_t layerIndex, const double latitude, const double longitude,
                   const double day, const std::array<double, 3>& expected, const std::string& messageOnError) {
    const GgrxfFile file = parseGgrxfFile(pathOf(name));
    checkValues(file.layers.at(layerIndex)->evaluate(latitude, longitude, day), expected, messageOnError);
}

/// Checks the values that the first layer of the file gives at a point on a day.
void checkValuesOf(const std::string& name, const double latitude, const double longitude, const double day,
                   const std::array<double, 3>& expected, const std::string& messageOnError) {
    checkValuesOf(name, 0, latitude, longitude, day, expected, messageOnError);
}

void check_a_file_with_a_ragged_layer() {
    const GgrxfFile file = parseGgrxfFile(pathOf("ragged_basic.nc"));
    check(file.layers.size() == 1, "a file with one layer has one layer");
    const deformation::Layer& layer = *file.layers[0];
    checkValues(layer.evaluate(10.5, 20.5, dayNumber(50.0)), valuesOnDay(50.0), "a point in the grid, on day 50");
    checkValues(layer.evaluate(10.5, 20.5, dayNumber(0.0)), valuesOnDay(0.0), "a point in the grid, on the first day");
    checkValues(layer.evaluate(11.75, 21.75, dayNumber(400.0)), valuesOnDay(400.0),
                "a point in the last cell of the grid, a year after the second north segment starts");
    check(!layer.evaluate(50.0, 50.0, dayNumber(50.0)).has_value(), "a point outside the grid");
}

void check_the_segment_that_applies_on_a_day() {
    const std::string name = "ragged_basic.nc";
    // The second north segment starts on day 100, and until then the first applies.
    checkValuesOf(name, 10.5, 20.5, dayNumber(99.0), valuesOnDay(99.0), "the day before the second north segment");
    checkValuesOf(name, 10.5, 20.5, dayNumber(100.0), valuesOnDay(100.0), "the day the second north segment starts");
    checkValuesOf(name, 10.5, 20.5, dayNumber(100.0), {10.0 + kEastRatePerDay * 100.0, 30.0, 0.0},
                  "the day the second north segment starts, as numbers");
    // Before the first segment of an axis starts, which is day 0 for them all, its value is 0.
    checkValuesOf(name, 10.5, 20.5, dayNumber(-10.0), {0.0, 0.0, 0.0}, "ten days before any segment starts");
}

void check_a_layer_of_two_axes() {
    // The file has no segment_count_u, so the layer has an east and a north axis, and the third value is 0.
    checkValuesOf("ragged_two_axes.nc", 10.5, 20.5, dayNumber(150.0), valuesOnDay(150.0), "a layer of east and north");
}

void check_the_reference_day_and_the_day_delta_add() {
    // The reference day of every node is 0 in ragged_basic.nc, so the day that a segment starts on is its day delta
    // only. The layer evaluates relative to its own reference date, so the date of the query is what moves.
    checkValuesOf("ragged_basic.nc", 10.5, 20.5, ggrxfDateToDayNumber("2000-04-10"), valuesOnDay(100.0),
                  "2000-04-10, which is 100 days (31 + 29 + 31 + 9) after 2000-01-01");
}

void check_the_start_of_a_segment_is_the_node_reference_day_plus_the_day_delta() {
    // The east axis of every node of ragged_node_reference_days.nc has one constant segment of the value 10, with the
    // day delta 100. The reference days of the four nodes of the first cell are 0 for node (0, 0), 10 for (0, 1), 20
    // for (1, 0) and 30 for (1, 1), so their segments start on the days 100, 110, 120 and 130. At (10.2, 20.4) the
    // weights of those nodes are 0.48, 0.32, 0.12 and 0.08, which are all different, so the value on a day is 10 times
    // the weights of the nodes that have started, and shows which of them have.
    const std::string name = "ragged_node_reference_days.nc";
    checkValuesOf(name, 10.2, 20.4, dayNumber(99.0), {0.0, 0.0, 0.0}, "the day before any segment starts");
    checkValuesOf(name, 10.2, 20.4, dayNumber(100.0), {4.8, 0.0, 0.0}, "the day the segment of node (0, 0) starts");
    checkValuesOf(name, 10.2, 20.4, dayNumber(110.0), {8.0, 0.0, 0.0}, "the day the segment of node (0, 1) starts");
    checkValuesOf(name, 10.2, 20.4, dayNumber(120.0), {9.2, 0.0, 0.0}, "the day the segment of node (1, 0) starts");
    checkValuesOf(name, 10.2, 20.4, dayNumber(130.0), {10.0, 0.0, 0.0}, "the day the segment of node (1, 1) starts");
}

void check_coefficients_in_the_order_of_longitude_first() {
    // The grid and its values are those of ragged_basic.nc, but its file declares the longitude axis first and has its
    // coefficients in that order. The layers give (latitude, longitude) either way.
    checkValuesOf("ragged_longitude_first.nc", 10.5, 20.5, dayNumber(50.0), valuesOnDay(50.0), "a point in the grid");
}

void check_a_file_with_both_types_of_layer() {
    // The scalar layer and the ragged layer of the file have the same grid, and each gives values at it.
    const GgrxfFile file = parseGgrxfFile(pathOf("mixed_layers.nc"));
    check(file.layers.size() == 2, "a file with a scalar and a ragged layer has two layers");
    for (const auto& layer : file.layers) {
        check(layer->evaluate(10.5, 20.5, dayNumber(400.0)).has_value(), "each layer of a mixed file gives values");
    }
}

void check_layers_missing_the_segment_counts() {
    checkRejected("ragged_no_segment_count_e.nc", "missing segment_count_e/n", "a layer with no segment_count_e");
    checkRejected("ragged_no_segment_count_n.nc", "missing segment_count_e/n", "a layer with no segment_count_n");
}

void check_a_function_type_that_is_not_implemented() {
    checkRejected("ragged_function_type.nc", "function_type_e code 1 is not implemented",
                  "a segment that is not linear");
}

void check_end_days() {
    // The middle node of the grid has an end day, and is a corner of all four cells of a grid of 3 by 3 nodes, so every
    // point of the grid is in a cell that has a node with an end day. That is not implemented, and is found when the
    // layer is evaluated, not when the file is read.
    const GgrxfFile file = parseGgrxfFile(pathOf("ragged_end_day.nc"));
    checkThrows<std::runtime_error>([&file] { return file.layers.at(0)->evaluate(10.5, 20.5, dayNumber(50.0)); },
                                    "own_end_day is not implemented", "a cell that has a node with an end day");
    checkThrows<std::runtime_error>([&file] { return file.layers.at(0)->evaluate(11.5, 21.5, dayNumber(50.0)); },
                                    "own_end_day is not implemented", "the last cell, which has it as a corner too");
    check(!file.layers.at(0)->evaluate(50.0, 50.0, dayNumber(50.0)).has_value(),
          "a point outside the grid, where no node is read");
    checkValuesOf("ragged_end_day_none.nc", 10.5, 20.5, dayNumber(50.0), valuesOnDay(50.0),
                  "a layer that has end days, which are all 0");
}

void check_nested_grids() {
    // The grid of 5 by 5 nodes covers latitude 10 to 14 and longitude 20 to 24. It has the raw start value 100 on the
    // east axis, which is 1, and nothing on the others. Inside it, a (priority 1) covers 11 to 13 by 21 to 23 and b
    // (priority 2) covers 12 to 14 by 22 to 24. They have the start values 11000 and 22000, which are 110 and 220 on
    // the east axis, and the north segments of ragged_basic.nc, which are 20 before day 100. The file has b before a,
    // so the order of the grids in the file is not the order of their priorities, and the value where they overlap is
    // b's only if the reader orders them by priority. A separate root grid has the segments of ragged_basic.nc, from
    // (30, 40).
    const std::string name = "ragged_nested.nc";
    const double day = dayNumber(50.0);
    checkValuesOf(name, 10.5, 20.5, day, {1.0, 0.0, 0.0}, "a point in the grid only");
    checkValuesOf(name, 11.5, 21.5, day, {110.0, 20.0, 0.0}, "a point in a as well");
    checkValuesOf(name, 13.5, 23.5, day, {220.0, 20.0, 0.0}, "a point in b as well");
    checkValuesOf(name, 12.5, 22.5, day, {220.0, 20.0, 0.0}, "a point in a and b, where b has priority");
    checkValuesOf(name, 30.5, 40.5, day, valuesOnDay(50.0), "a point in the separate root grid");
}

void check_nests_with_wrong_priorities() {
    checkRejected("ragged_nest_root_priority_1.nc", "expected 0", "a nest root whose priority is not 0");
    checkRejected("ragged_nest_child_priority_0.nc", "gridPriority 0", "a child whose priority is 0");
    checkRejected("ragged_nest_child_priority_range.nc", "gridPriority 3", "a child whose priority is out of range");
    checkRejected("ragged_nest_child_priority_twice.nc", "already used", "two children that have the same priority");
}

void check_grids_that_no_root_reaches() {
    // Grids 1 and 2 have each other as parents, so no root grid reaches them.
    checkRejected("ragged_nest_unreached.nc", "2 of 3 grids that are neither a root grid nor nested under one",
                  "grids that are the parents of each other");
}

void check_arrays_of_the_wrong_length() {
    // Each file has one grid, of 3 by 3 nodes, so 9 nodes, and 9 segments on the east axis.
    checkRejected("ragged_length_jNodeCount.nc", ".jNodeCount has 2 entries, expected 1", "jNodeCount of 2 grids");
    checkRejected("ragged_length_gridPriority.nc", ".gridPriority has 2 entries, expected 1",
                  "gridPriority of 2 grids");
    checkRejected("ragged_length_parent_grid_index.nc", ".parent_grid_index has 2 entries, expected 1",
                  "parent_grid_index of 2 grids");
    checkRejected("ragged_length_affineCoeffs.nc", ".affineCoeffs has 5 entries, expected 6",
                  "affineCoeffs of 5 numbers for a grid");
    checkRejected("ragged_length_reference_day.nc", ".reference_day has 8 entries, expected 9",
                  "reference_day of 8 nodes for 9");
    checkRejected("ragged_length_has_end_day.nc", ".has_end_day has 8 entries, expected 9",
                  "has_end_day of 8 nodes for 9");
    checkRejected("ragged_length_segment_count_e.nc", ".segment_count_e has 8 entries, expected 9",
                  "segment_count_e of 8 nodes for 9");
    checkRejected("ragged_length_segments_e.nc", ".segments_e has 8 entries, expected 9",
                  "segments_e of 8 segments for 9");
    checkRejected("ragged_length_function_type_e.nc", ".function_type_e has 8 entries, expected 9",
                  "function_type_e of 8 segments for 9");
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: ggrxf_reader_ragged_test <directory of the GGRXF files>\n";
        return 2;
    }
    filesDirectory = argv[1];

    try {
        check_a_file_with_a_ragged_layer();
        check_the_segment_that_applies_on_a_day();
        check_the_start_of_a_segment_is_the_node_reference_day_plus_the_day_delta();
        check_a_layer_of_two_axes();
        check_the_reference_day_and_the_day_delta_add();
        check_coefficients_in_the_order_of_longitude_first();
        check_a_file_with_both_types_of_layer();
        check_layers_missing_the_segment_counts();
        check_a_function_type_that_is_not_implemented();
        check_end_days();
        check_nested_grids();
        check_nests_with_wrong_priorities();
        check_grids_that_no_root_reaches();
        check_arrays_of_the_wrong_length();
    } catch (const std::exception& error) {
        check(false, std::string("unexpected exception: ") + error.what());
    }

    return snap_test::finish("ggrxf_reader_ragged_test");
}
