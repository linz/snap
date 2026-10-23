// Standalone unit test for ScalarLayer (deformation/scalar_layer.h): the checks that addRootGrid() makes, the choice of
// the grid that a point is in (the first root grid that covers it, and then the children of that grid), and the scaling
// of the interpolated values by the sum of the layer's time functions at the query date.
//
// The grids are axis-aligned squares of n by n nodes, with node (i, j) at c0 = x0 + i and c1 = y0 + j, so that the
// extent of a grid is easy to read from where it starts and its size. The nodes are those of IndexedScalarValues
// (indexed_scalar_values.h): node (i, j) of a grid has the value offset + 100 * a + 10 * i + j on axis a, so that the
// value that a layer gives shows which grid and which nodes answered. The expected values are worked out by hand.

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "deformation/grid.h"
#include "deformation/grid_geometry.h"
#include "deformation/scalar_grid_data.h"
#include "deformation/scalar_layer.h"
#include "deformation/time_function.h"
#include "indexed_scalar_values.h"
#include "test_support.h"

namespace {

using deformation::Grid;
using deformation::GridGeometry;
using deformation::GridParent;
using deformation::ScalarGridData;
using deformation::ScalarLayer;
using deformation::TimeFunction;
using deformation::TimeFunctionShape;
using snap_test::check;
using snap_test::checkThrows;
using snap_test::checkValues;
using snap_test::IndexedScalarValues;

using ScalarGrid = Grid<ScalarGridData>;
using ScalarParent = GridParent<ScalarGridData>;
using Node = IndexedScalarValues::Node;

/// \return the data of a grid, of IndexedScalarValues, with a scale factor of 1.
ScalarGridData makeData(const std::size_t axisCount, const double offset, const std::vector<Node>& nodesWithNoValue) {
    return ScalarGridData(1.0, std::make_unique<IndexedScalarValues>(axisCount, offset, false, nodesWithNoValue));
}

/// \return the geometry of a grid of count by count nodes that starts at (x0, y0), with no boundary tolerance.
GridGeometry makeGeometry(const double x0, const double y0, const std::uint32_t count) {
    return GridGeometry({x0, 1.0, 0.0, y0, 0.0, 1.0}, count, count, 0.0);
}

/// \return a grid with no children.
std::unique_ptr<ScalarGrid> makeGrid(const double x0, const double y0, const std::uint32_t count,
                                     const std::size_t axisCount = 3, const double offset = 0.0,
                                     const std::vector<Node>& nodesWithNoValue = {}) {
    return std::make_unique<ScalarGrid>(makeGeometry(x0, y0, count), makeData(axisCount, offset, nodesWithNoValue));
}

/// \return a grid that can have children, which has none yet.
std::unique_ptr<ScalarParent> makeParent(const double x0, const double y0, const std::uint32_t count,
                                         const std::size_t axisCount = 3, const double offset = 0.0) {
    return std::make_unique<ScalarParent>(makeGeometry(x0, y0, count), makeData(axisCount, offset, {}));
}

/// \return a time function that is 0 before the day, and the scale factor from the day on.
TimeFunction stepFunction(const double eventDay, const double scaleFactor) {
    TimeFunction::Params params;
    params.shape = TimeFunctionShape::Step;
    params.scaleFactor = scaleFactor;
    params.eventDateDays = eventDay;
    return TimeFunction(params);
}

/// \return a time function that is the scale factor times the number of days since day 0.
TimeFunction linearFunction(const double scaleFactor) {
    TimeFunction::Params params;
    params.shape = TimeFunctionShape::Linear;
    params.scaleFactor = scaleFactor;
    params.functionReferenceDateDays = 0.0;
    return TimeFunction(params);
}

/// The day that the layers are evaluated on, which is after the day that every step function steps on.
constexpr double kDay = 10.0;

void check_axis_count() {
    ScalarLayer layer;
    check(!layer.axisCount().has_value(), "a layer with no root grid has no axis count");

    layer.addRootGrid(makeGrid(0.0, 0.0, 5, 2));
    check(layer.axisCount() == 2, "the axis count of a layer is that of its first root grid");

    layer.addRootGrid(makeGrid(20.0, 20.0, 3, 2));
    check(layer.axisCount() == 2, "the axis count of a layer is the same after another root grid with that count");
}

void check_root_grids_are_validated() {
    ScalarLayer layer;
    layer.addRootGrid(makeGrid(0.0, 0.0, 5, 2));
    layer.addTimeFunction(stepFunction(0.0, 1.0));

    checkThrows<std::invalid_argument>([&layer] { layer.addRootGrid(nullptr); }, "must not be null",
                                       "a null root grid");
    checkThrows<std::invalid_argument>([&layer] { layer.addRootGrid(makeGrid(10.0, 10.0, 3, 3)); }, "axisCount",
                                       "a root grid with a different axis count");
    check(layer.axisCount() == 2, "a rejected root grid does not change the axis count");
    check(!layer.evaluate(11.0, 11.0, kDay).has_value(), "a rejected root grid is not added");
}

void check_a_point_that_no_grid_covers_gives_no_values() {
    ScalarLayer empty;
    empty.addTimeFunction(stepFunction(0.0, 1.0));
    check(!empty.evaluate(1.0, 1.0, kDay).has_value(), "a layer with no root grid");

    ScalarLayer layer;
    layer.addRootGrid(makeGrid(0.0, 0.0, 5));  // covers [0, 4] by [0, 4]
    layer.addTimeFunction(stepFunction(0.0, 1.0));
    checkValues(layer.evaluate(4.0, 4.0, kDay), {44.0, 144.0, 244.0}, "the corner of a root grid, which it covers");
    check(!layer.evaluate(4.5, 1.0, kDay).has_value(), "a point beyond the last c0 of the only root grid");
    check(!layer.evaluate(9.0, 9.0, kDay).has_value(), "a point far from the only root grid");
}

void check_value_is_interpolated_and_scaled() {
    ScalarLayer layer;
    layer.addRootGrid(makeGrid(0.0, 0.0, 5));
    layer.addTimeFunction(stepFunction(0.0, 2.0));
    // Node (i, j) has the value 10 i + j, which is 17.5 at (1.5, 2.5), and 100 more on each later axis, all doubled.
    checkValues(layer.evaluate(1.5, 2.5, kDay), {35.0, 235.0, 435.0}, "a point in a root grid");
}

void check_values_beyond_the_axis_count_are_zero() {
    ScalarLayer layer;
    layer.addRootGrid(makeGrid(0.0, 0.0, 5, 2));
    layer.addTimeFunction(stepFunction(0.0, 2.0));
    checkValues(layer.evaluate(1.5, 2.5, kDay), {35.0, 235.0, 0.0}, "a layer of two axes");
}

void check_time_functions_are_summed_at_the_query_date() {
    ScalarLayer layer;
    layer.addRootGrid(makeGrid(0.0, 0.0, 5));
    layer.addTimeFunction(linearFunction(2.0));      // 2 a day
    layer.addTimeFunction(stepFunction(10.0, 5.0));  // 5 from day 10 on
    // The value of the nodes at (1.5, 2.5) is 17.5, 117.5 and 217.5, and what scales them is the sum of the functions.
    checkValues(layer.evaluate(1.5, 2.5, 4.0), {140.0, 940.0, 1740.0}, "day 4: 8 from the line, 0 from the step");
    checkValues(layer.evaluate(1.5, 2.5, 10.0), {437.5, 2937.5, 5437.5}, "day 10: 20 and 5, the step is made that day");
    checkValues(layer.evaluate(1.5, 2.5, 12.0), {507.5, 3407.5, 6307.5}, "day 12: 24 and 5");
}

void check_a_layer_with_no_time_function_gives_zeros() {
    // The values are scaled by the sum of the time functions, which is 0 when there are none. The layer still covers
    // the point, so it gives 0 for every axis, not no values.
    ScalarLayer layer;
    layer.addRootGrid(makeGrid(0.0, 0.0, 5));
    checkValues(layer.evaluate(1.5, 2.5, kDay), {0.0, 0.0, 0.0}, "a layer with a root grid and no time function");
}

void check_the_first_root_grid_that_covers_the_point_answers() {
    // A covers [0, 4] by [0, 4], and B, which has values 1000 higher, covers [2, 6] by [2, 6].
    ScalarLayer aThenB;
    aThenB.addRootGrid(makeGrid(0.0, 0.0, 5, 3, 0.0));
    aThenB.addRootGrid(makeGrid(2.0, 2.0, 5, 3, 1000.0));
    aThenB.addTimeFunction(stepFunction(0.0, 1.0));
    checkValues(aThenB.evaluate(3.0, 3.0, kDay), {33.0, 133.0, 233.0}, "a point in both, with A added first");
    checkValues(aThenB.evaluate(1.0, 1.0, kDay), {11.0, 111.0, 211.0}, "a point in A only");
    checkValues(aThenB.evaluate(5.0, 5.0, kDay), {1033.0, 1133.0, 1233.0}, "a point in B only");

    ScalarLayer bThenA;
    bThenA.addRootGrid(makeGrid(2.0, 2.0, 5, 3, 1000.0));
    bThenA.addRootGrid(makeGrid(0.0, 0.0, 5, 3, 0.0));
    bThenA.addTimeFunction(stepFunction(0.0, 1.0));
    checkValues(bThenA.evaluate(3.0, 3.0, kDay), {1011.0, 1111.0, 1211.0}, "a point in both, with B added first");
}

void check_root_grids_that_do_not_overlap_each_answer_for_their_own_area() {
    ScalarLayer layer;
    layer.addRootGrid(makeGrid(0.0, 0.0, 3, 3, 0.0));      // covers [0, 2] by [0, 2]
    layer.addRootGrid(makeGrid(10.0, 10.0, 3, 3, 500.0));  // covers [10, 12] by [10, 12]
    layer.addTimeFunction(stepFunction(0.0, 1.0));
    checkValues(layer.evaluate(1.0, 1.5, kDay), {11.5, 111.5, 211.5}, "a point in the first root grid");
    checkValues(layer.evaluate(11.0, 10.5, kDay), {510.5, 610.5, 710.5}, "a point in the second root grid");
    check(!layer.evaluate(5.0, 5.0, kDay).has_value(), "a point between them");
}

void check_a_child_grid_answers_where_it_covers_the_point() {
    // The root covers [0, 10] by [0, 10], and its child, which has values 1000 higher, covers [4, 6] by [4, 6].
    auto root = makeParent(0.0, 0.0, 11);
    root->addChild(makeGrid(4.0, 4.0, 3, 3, 1000.0));
    ScalarLayer layer;
    layer.addRootGrid(std::move(root));
    layer.addTimeFunction(stepFunction(0.0, 3.0));
    // The same time functions scale the values of a child as they scale those of a root grid.
    checkValues(layer.evaluate(5.0, 5.0, kDay), {3033.0, 3333.0, 3633.0}, "a point in the child");
    checkValues(layer.evaluate(4.5, 5.5, kDay), {3019.5, 3319.5, 3619.5}, "a point between nodes of the child");
    checkValues(layer.evaluate(1.0, 1.0, kDay), {33.0, 333.0, 633.0}, "a point in the root grid only");
}

void check_a_node_with_no_value_gives_no_values() {
    ScalarLayer layer;
    layer.addRootGrid(makeGrid(0.0, 0.0, 5, 3, 0.0, {{1, 1}}));
    layer.addTimeFunction(stepFunction(0.0, 1.0));
    check(!layer.evaluate(1.5, 1.5, kDay).has_value(), "a point in a cell that has the node with no value");
    check(!layer.evaluate(0.5, 0.5, kDay).has_value(), "a point in another cell that has the node with no value");
    checkValues(layer.evaluate(3.5, 3.5, kDay), {38.5, 138.5, 238.5}, "a point in a cell that has all its nodes");
}

void check_a_child_with_no_value_does_not_fall_back_to_its_parent() {
    // The child covers [4, 6] by [4, 6], and its corner node at (6, 6) has no value. Only the cell with its base at the
    // node (1, 1) of the child has that node.
    auto root = makeParent(0.0, 0.0, 11);
    root->addChild(makeGrid(4.0, 4.0, 3, 3, 1000.0, {{2, 2}}));
    ScalarLayer layer;
    layer.addRootGrid(std::move(root));
    layer.addTimeFunction(stepFunction(0.0, 1.0));
    check(!layer.evaluate(5.5, 5.5, kDay).has_value(),
          "a point in a cell of the child that has the node with no value, though the root grid has values there");
    checkValues(layer.evaluate(4.5, 4.5, kDay), {1005.5, 1105.5, 1205.5}, "a point in a cell of the child with values");
}

}  // namespace

int main() {
    check_axis_count();
    check_root_grids_are_validated();
    check_a_point_that_no_grid_covers_gives_no_values();
    check_value_is_interpolated_and_scaled();
    check_values_beyond_the_axis_count_are_zero();
    check_time_functions_are_summed_at_the_query_date();
    check_a_layer_with_no_time_function_gives_zeros();
    check_the_first_root_grid_that_covers_the_point_answers();
    check_root_grids_that_do_not_overlap_each_answer_for_their_own_area();
    check_a_child_grid_answers_where_it_covers_the_point();
    check_a_node_with_no_value_gives_no_values();
    check_a_child_with_no_value_does_not_fall_back_to_its_parent();

    return snap_test::finish("scalar_layer_test");
}
