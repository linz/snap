// Standalone unit test for ScalarGridData (deformation/scalar_grid_data.h): the validation of its node values source,
// the axis count, and interpolate(), which weights the four nodes of a cell bilinearly and then applies the scale
// factor once. A cell that has a node with no value gives no values at all.
//
// The nodes are those of IndexedScalarValues (indexed_scalar_values.h): node (i, j) has the value 100 * a + 10 * i + j
// on axis a, so that the value at a cell shows which nodes were read, and the nodes have values for the axes that the
// grid does not have, which interpolate() must not read. The values of those nodes are all in one plane: they rise by
// the same amount from one node to the next along i, and by the same amount along j. A weighting that ignores the
// fourth node of a cell, and extends the plane through the other three, gives the same values for them as a bilinear
// one, so NonPlanarScalarValues has four nodes that are not in one plane.

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "deformation/grid_geometry.h"
#include "deformation/scalar_grid_data.h"
#include "indexed_scalar_values.h"
#include "test_support.h"

namespace {

using deformation::Cell;
using deformation::ScalarGridData;
using snap_test::check;
using snap_test::checkEqual;
using snap_test::checkThrows;
using snap_test::IndexedScalarValues;

using snap_test::checkValues;

using Node = IndexedScalarValues::Node;

/// \return a grid's data of IndexedScalarValues.
ScalarGridData makeData(const double scaleFactor, const std::size_t axisCount,
                        const std::vector<Node>& nodesWithNoValue = {}) {
    return ScalarGridData(scaleFactor, std::make_unique<IndexedScalarValues>(axisCount, 0.0, false, nodesWithNoValue));
}

/// A grid of one axis whose node values are not all in one plane: the four nodes of its one cell have the values that
/// are given, in the order (0, 0), (1, 0), (0, 1), (1, 1), so the value at a point of the cell depends on all four of
/// them. Four values that are in one plane are rejected.
class NonPlanarScalarValues : public deformation::ScalarNodeValueSource {
 public:
    explicit NonPlanarScalarValues(const std::array<double, 4>& corners)  ///< the values of the four nodes of the cell
        : _corners(corners) {
        if (corners[0] + corners[3] == corners[1] + corners[2]) {
            throw std::invalid_argument("NonPlanarScalarValues: the four values are in one plane");
        }
    }

    [[nodiscard]] std::size_t axisCount() const override {
        return 1;
    }

    [[nodiscard]] std::optional<std::array<double, 3>> nodeValues(const std::uint32_t i,
                                                                  const std::uint32_t j) const override {
        if (i > 1 || j > 1) {
            return std::nullopt;
        }
        return std::array<double, 3>{_corners[i + 2 * j], 0.0, 0.0};
    }

    [[nodiscard]] bool hasEndDay() const override {
        return false;
    }

 private:
    std::array<double, 4> _corners;
};

void check_source_is_validated() {
    checkThrows<std::invalid_argument>([] { ScalarGridData(1.0, nullptr); }, "must not be null",
                                       "a null source of node values");
    checkThrows<std::runtime_error>([] { ScalarGridData(1.0, std::make_unique<IndexedScalarValues>(1, 0.0, true)); },
                                    "not implemented", "a source whose grid has an end day");
    check(ScalarGridData(1.0, std::make_unique<IndexedScalarValues>(1, 0.0, false)).axisCount() == 1,
          "a source whose grid has no end day is accepted");
}

void check_axis_count() {
    for (const std::size_t axisCount : {1, 2, 3}) {
        checkEqual(makeData(1.0, axisCount).axisCount(), axisCount,
                   "the axis count is that of the source of " + std::to_string(axisCount) + " axes");
    }
}

void check_interpolation_of_a_cell() {
    const ScalarGridData data = makeData(1.0, 3);
    checkValues(data.interpolate(Cell{2, 3, 0.0, 0.0}), {23.0, 123.0, 223.0}, "on a node");
    checkValues(data.interpolate(Cell{1, 1, 0.5, 0.5}), {16.5, 116.5, 216.5}, "at the centre of a cell");
    checkValues(data.interpolate(Cell{0, 2, 0.25, 0.75}), {5.25, 105.25, 205.25}, "at a point that is not central");
    checkValues(data.interpolate(Cell{2, 0, 1.0, 1.0}), {31.0, 131.0, 231.0}, "on the far corner of a cell");
}

void check_all_four_nodes_of_a_cell_are_weighted() {
    const auto dataOf = [](const std::array<double, 4>& corners) {
        return ScalarGridData(1.0, std::make_unique<NonPlanarScalarValues>(corners));
    };
    // Only the fourth node has a value, so the value is its weight 0.25 * 0.75 = 0.1875 times 16. A weighting that
    // ignores the fourth node, and extends a plane through the other three, gives 0.
    checkValues(dataOf({0.0, 0.0, 0.0, 16.0}).interpolate(Cell{0, 0, 0.25, 0.75}), {3.0, 0.0, 0.0},
                "a cell that only the fourth node has a value in");
    // The weights of (0, 0), (1, 0), (0, 1) and (1, 1) are 0.32, 0.08, 0.48 and 0.12, which are all different, so a
    // weight that is given to the wrong node changes the value. A plane through the first three nodes gives 3.
    checkValues(dataOf({1.0, 2.0, 4.0, 8.0}).interpolate(Cell{0, 0, 0.2, 0.6}), {3.36, 0.0, 0.0},
                "a cell whose four nodes have different values and weights");
    // The values of the nodes of IndexedScalarValues, which are all in one plane, are not accepted as non-planar ones.
    checkThrows<std::invalid_argument>([] { const NonPlanarScalarValues values({0.0, 10.0, 1.0, 11.0}); },
                                       "in one plane", "four values that are in one plane");
}

void check_scale_factor_is_applied_once_after_the_weighted_sum() {
    checkValues(makeData(2.0, 3).interpolate(Cell{1, 1, 0.5, 0.5}), {33.0, 233.0, 433.0}, "a scale factor of 2");
    checkValues(makeData(-0.5, 3).interpolate(Cell{1, 1, 0.5, 0.5}), {-8.25, -58.25, -108.25},
                "a negative scale factor");
    checkValues(makeData(0.0, 3).interpolate(Cell{1, 1, 0.5, 0.5}), {0.0, 0.0, 0.0}, "a scale factor of 0");
}

void check_values_beyond_the_axis_count_are_not_read() {
    checkValues(makeData(1.0, 2).interpolate(Cell{1, 1, 0.5, 0.5}), {16.5, 116.5, 0.0}, "two axes");
    checkValues(makeData(1.0, 1).interpolate(Cell{1, 1, 0.5, 0.5}), {16.5, 0.0, 0.0}, "one axis");
}

void check_fractions_outside_a_cell_extrapolate() {
    // A point in the tolerance band at the edge of a grid has a fraction outside [0, 1) in its cell, and so a negative
    // weight for the nodes on the far side.
    const ScalarGridData data = makeData(1.0, 3);
    checkValues(data.interpolate(Cell{0, 0, -0.125, 0.0}), {-1.25, 98.75, 198.75}, "a little before the first node");
    checkValues(data.interpolate(Cell{2, 0, 1.25, 0.0}), {32.5, 132.5, 232.5}, "a little past the last node");
}

void check_a_node_with_no_value_gives_no_values() {
    const Cell cell{1, 1, 0.5, 0.5};
    // The nodes of the cell are (1, 1), (2, 1), (1, 2) and (2, 2).
    for (const Node& node : std::vector<Node>{{1, 1}, {2, 1}, {1, 2}, {2, 2}}) {
        const std::string name = "node (" + std::to_string(node.first) + ", " + std::to_string(node.second) + ")";
        check(!makeData(1.0, 3, {node}).interpolate(cell).has_value(), "no values when " + name + " has no value");
    }

    // A node outside the cell does not matter.
    checkValues(makeData(1.0, 3, {{0, 0}, {3, 3}}).interpolate(cell), {16.5, 116.5, 216.5},
                "values when only nodes outside the cell have no value");

    // All four nodes of the cell have to have a value, whatever their weights are, even a weight of 0.
    check(!makeData(1.0, 3, {{2, 2}}).interpolate(Cell{1, 1, 0.0, 0.0}).has_value(),
          "no values when a node that has a weight of 0 has no value");
}

}  // namespace

int main() {
    check_source_is_validated();
    check_axis_count();
    check_interpolation_of_a_cell();
    check_all_four_nodes_of_a_cell_are_weighted();
    check_scale_factor_is_applied_once_after_the_weighted_sum();
    check_values_beyond_the_axis_count_are_not_read();
    check_fractions_outside_a_cell_extrapolate();
    check_a_node_with_no_value_gives_no_values();

    return snap_test::finish("scalar_grid_data_test");
}
