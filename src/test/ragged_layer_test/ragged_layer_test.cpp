// Standalone unit test for RaggedLayer (deformation/ragged_layer.h), which finds the root grid that a point is in.
// Failures are reported to stdout and the process exit code is the pass/fail signal (0 = all passed), as for the other
// test-only tools in src/test/.
//
// The grids are built in code. Node (i, j) of a grid has the value offset + 10 * i + j on its one axis, at every day,
// where offset is different for each grid. Bilinear interpolation of that is exact, so the value at a point shows
// which grid answered, and which node indices the point was found by, and so which way round the grid's own axes run.
//
// The grids that a layer is given are also checked: none, a null grid, grids of different axis counts, and a second
// set of grids.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "deformation/grid.h"
#include "deformation/grid_geometry.h"
#include "deformation/ragged_grid_data.h"
#include "deformation/ragged_layer.h"
#include "test_support.h"

namespace {

using deformation::Grid;
using deformation::GridGeometry;
using deformation::RaggedGridData;
using deformation::RaggedLayer;
using deformation::RaggedNodeValueSource;
using deformation::RaggedSegment;
using deformation::RaggedSegmentRange;

using GridPointer = std::unique_ptr<Grid<RaggedGridData>>;

/// The boundary tolerance of a grid, which is the one that the GGRXF specification gives.
constexpr double kCellTolerance = 0.00001;

/// How far a value may be from the one expected.
constexpr double kValueTolerance = 1e-9;

using snap_test::check;
using snap_test::checkThrows;

/// The values of a grid: node (i, j) has the value offset + 10 * i + j on each of its axes, from day 0 on, and does not
/// change.
class NodeIndexValues : public RaggedNodeValueSource {
 public:
    NodeIndexValues(const std::uint32_t iNodeCount,  ///< how many nodes the grid has along i
                    const std::uint32_t jNodeCount,  ///< how many nodes the grid has along j
                    const double offset,             ///< added to the value of every node, to tell grids apart
                    const std::size_t axisCount)     ///< how many axes the grid has, which all have the same values
        : _jNodeCount(jNodeCount), _axisCount(axisCount) {
        for (std::uint32_t i = 0; i < iNodeCount; ++i) {
            for (std::uint32_t j = 0; j < jNodeCount; ++j) {
                _segments.push_back(RaggedSegment{0.0, offset + 10.0 * i + j, 0.0});
            }
        }
    }

    [[nodiscard]] std::size_t axisCount() const override {
        return _axisCount;
    }

    [[nodiscard]] bool hasEndDay(const std::uint32_t, const std::uint32_t) const override {
        return false;
    }

    [[nodiscard]] RaggedSegmentRange segments(const std::uint32_t i, const std::uint32_t j,
                                              const std::size_t) const override {
        const RaggedSegment* segment = &_segments[i * _jNodeCount + j];
        return RaggedSegmentRange(segment, segment + 1);
    }

 private:
    std::uint32_t _jNodeCount;
    std::size_t _axisCount;
    std::vector<RaggedSegment> _segments;
};

/// \return a grid with those coefficients and node counts, that has the values of NodeIndexValues.
GridPointer makeGrid(const std::array<double, 6>& affineCoeffs,  ///< the grid's node (i, j) to (c0, c1) transform
                     const std::uint32_t iNodeCount,             ///< how many nodes the grid has along i
                     const std::uint32_t jNodeCount,             ///< how many nodes the grid has along j
                     const double offset = 0.0,                  ///< added to the value of every node
                     const std::size_t axisCount = 1) {          ///< how many axes the grid has
    const GridGeometry geometry(affineCoeffs, iNodeCount, jNodeCount, kCellTolerance);
    return std::make_unique<Grid<RaggedGridData>>(
        geometry, RaggedGridData(std::make_unique<NodeIndexValues>(iNodeCount, jNodeCount, offset, axisCount)));
}

/// \return a square grid of three nodes along each side, with those coefficients.
GridPointer makeSmallGrid(const std::array<double, 6>& affineCoeffs, const double offset = 0.0) {
    return makeGrid(affineCoeffs, 3, 3, offset);
}

/// \return the grids as a list, in the order that they are given, which is the order that a file has them in.
template <typename... Grids>
std::vector<GridPointer> inFileOrder(Grids&&... grids) {
    std::vector<GridPointer> list;
    (list.push_back(std::forward<Grids>(grids)), ...);
    return list;
}

/// \return a layer of those grids, in file order.
RaggedLayer makeLayer(std::vector<GridPointer> grids) {
    RaggedLayer layer(0.0);
    layer.addRootGrids(std::move(grids));
    return layer;
}

/// Checks that the layer gives a value at a point, and what it is.
void checkValue(const RaggedLayer& layer,        ///< the layer to evaluate
                const double c0,                 ///< the first coordinate of the point
                const double c1,                 ///< the second coordinate of the point
                const double expected,           ///< the value that it must give
                const std::string& description)  ///< what the check is of
{
    const auto result = layer.evaluate(c0, c1, 0.0);
    check(result.has_value() && std::abs((*result)[0] - expected) <= kValueTolerance,
          description + ": expected " + std::to_string(expected) + ", got " +
              (result ? std::to_string((*result)[0]) : std::string("no value")));
}

/// Checks that the layer gives no value at a point, because none of its grids contains it.
void checkNoValue(const RaggedLayer& layer,        ///< the layer to evaluate
                  const double c0,                 ///< the first coordinate of the point
                  const double c1,                 ///< the second coordinate of the point
                  const std::string& description)  ///< what the check is of
{
    check(!layer.evaluate(c0, c1, 0.0).has_value(), description + ": expected no value");
}

/// \return whether giving the layer the grids throws an exception of that type.
template <typename Exception>
bool addThrows(RaggedLayer& layer,              ///< the layer to give them to
               std::vector<GridPointer> grids)  ///< the grids
{
    try {
        layer.addRootGrids(std::move(grids));
    } catch (const Exception&) {
        return true;
    }
    return false;
}

// c0 changes along i and c1 along j. Node (i, j) is at (c0, c1) = (i, j), so the point (0.5, 1.5) is at i of 0.5 and j
// of 1.5, which gives 10 * 0.5 + 1.5.
constexpr std::array<double, 6> kAlongCoordinates = {0, 1, 0, 0, 0, 1};

// c0 changes along j and c1 along i, which is what a file with its longitude first gives once its coefficients are in
// (latitude, longitude) order. Node (i, j) is at (c0, c1) = (j, i), so the point (0.5, 1.5) is at i of 1.5 and j of
// 0.5, which gives 10 * 1.5 + 0.5.
constexpr std::array<double, 6> kAcrossCoordinates = {0, 0, 1, 0, 1, 0};

void check_grid_along_coordinates() {
    const RaggedLayer layer = makeLayer(inFileOrder(makeSmallGrid(kAlongCoordinates)));
    checkValue(layer, 0.5, 1.5, 6.5, "c0 along i: a point in the grid");
    checkNoValue(layer, 5.0, 5.0, "c0 along i: a point outside the grid");
}

void check_grid_across_coordinates() {
    const RaggedLayer layer = makeLayer(inFileOrder(makeSmallGrid(kAcrossCoordinates)));
    checkValue(layer, 0.5, 1.5, 15.5, "c0 along j: a point in the grid");
    checkNoValue(layer, 5.0, 5.0, "c0 along j: a point outside the grid");
}

void check_layer_with_both_orientations() {
    // c0 from 2 to 4 and c1 from 0 to 2, next to the first grid, with c0 along j.
    const std::array<double, 6> acrossFromTwo = {2, 0, 1, 0, 1, 0};
    const RaggedLayer layer = makeLayer(inFileOrder(makeSmallGrid(kAlongCoordinates), makeSmallGrid(acrossFromTwo)));

    checkValue(layer, 0.5, 1.5, 6.5, "both orientations: a point in the grid with c0 along i");
    checkValue(layer, 2.5, 1.5, 15.5, "both orientations: a point in the grid with c0 along j");
    checkNoValue(layer, 5.0, 1.5, "both orientations: a point past both grids");
}

// Where root grids overlap, the first one in file order that contains the point answers it.
void check_overlapping_root_grids_resolve_by_file_order() {
    // A grid of two nodes along each side covers c0 and c1 from 0 to 1, and one of three nodes covers 0 to 2.
    const auto small = [] { return makeGrid(kAlongCoordinates, 2, 2, 100.0); };
    const auto large = [] { return makeGrid(kAlongCoordinates, 3, 3, 200.0); };

    const RaggedLayer smallFirst = makeLayer(inFileOrder(small(), large()));
    checkValue(smallFirst, 0.5, 0.5, 105.5, "small grid first: a point in both grids is answered by the small grid");
    checkValue(smallFirst, 1.5, 1.5, 216.5, "small grid first: a point in the large grid only");

    const RaggedLayer largeFirst = makeLayer(inFileOrder(large(), small()));
    checkValue(largeFirst, 0.5, 0.5, 205.5, "large grid first: a point in both grids is answered by the large grid");
    checkValue(largeFirst, 1.5, 1.5, 216.5, "large grid first: a point in the large grid only");
}

// A point on an edge that two grids share is in both of them, within the boundary tolerance, so the first in file order
// answers it.
void check_shared_edge_resolves_by_file_order() {
    // c0 from 0 to 2 and from 2 to 4, with c1 from 0 to 2 for both.
    const auto left = [] { return makeSmallGrid(kAlongCoordinates, 100.0); };
    const auto right = [] { return makeSmallGrid({2, 1, 0, 0, 0, 1}, 200.0); };

    const RaggedLayer leftFirst = makeLayer(inFileOrder(left(), right()));
    checkValue(leftFirst, 2.0, 1.0, 121.0, "left grid first: a point on the shared edge is answered by the left grid");
    checkValue(leftFirst, 3.0, 1.0, 211.0, "left grid first: a point in the right grid only");

    const RaggedLayer rightFirst = makeLayer(inFileOrder(right(), left()));
    checkValue(rightFirst, 2.0, 1.0, 201.0,
               "right grid first: a point on the shared edge is answered by the right grid");
    checkValue(rightFirst, 1.0, 1.0, 111.0, "right grid first: a point in the left grid only");
}

// File order decides even when grids of different sizes are mixed up in it, so it is not the same as trying the sizes in
// the order that they first appear.
void check_mixed_sizes_resolve_by_file_order() {
    // Three grids in file order: a large one, a small one and another large one. The small grid lies inside the second
    // large one, from 10 to 11 where that one covers 10 to 12, and the first large one covers 0 to 2.
    const RaggedLayer layer =
        makeLayer(inFileOrder(makeSmallGrid(kAlongCoordinates, 100.0), makeGrid({10, 1, 0, 0, 0, 1}, 2, 2, 200.0),
                              makeSmallGrid({10, 1, 0, 0, 0, 1}, 300.0)));
    checkValue(layer, 10.5, 0.5, 205.5, "mixed sizes: a point in the small grid and the second large grid");
    checkValue(layer, 11.5, 1.5, 316.5, "mixed sizes: a point in the second large grid only");
    checkValue(layer, 0.5, 1.5, 106.5, "mixed sizes: a point in the first large grid");
}

// The lattice reference is the largest grid, however the grids are ordered, so that a small grid and a large grid that
// do not overlap are both found.
void check_grids_of_different_sizes_are_found_in_any_order() {
    // c0 from 6 to 7 for the small grid, and from 8 to 12 with c1 from 0 to 4 for the large one. If the small grid
    // were the reference, with origin 6, the large grid would be 2 steps from it, which is not a multiple of its size
    // of 4 steps, and it would never be found.
    const auto small = [] { return makeGrid({6, 1, 0, 0, 0, 1}, 2, 2, 100.0); };
    const auto large = [] { return makeGrid({8, 1, 0, 0, 0, 1}, 5, 5, 200.0); };

    for (const bool smallFirst : {true, false}) {
        const std::string order = smallFirst ? "small grid first: " : "large grid first: ";
        const RaggedLayer layer =
            smallFirst ? makeLayer(inFileOrder(small(), large())) : makeLayer(inFileOrder(large(), small()));
        checkValue(layer, 6.5, 0.5, 105.5, order + "a point in the small grid");
        checkValue(layer, 9.5, 1.5, 216.5, order + "a point in the large grid");
        checkNoValue(layer, 7.5, 0.5, order + "a point between them");
    }
}

// A grid need not be square: this one has 3 nodes along i and 4 along j, so it covers c0 from 0 to 2 and c1 from 0 to 3.
void check_rectangular_grid() {
    const RaggedLayer layer = makeLayer(inFileOrder(makeGrid(kAlongCoordinates, 3, 4)));
    checkValue(layer, 0.5, 2.5, 7.5, "a rectangular grid: a point in the grid");
    checkNoValue(layer, 0.5, 3.5, "a rectangular grid: a point beyond its longer side");
    checkNoValue(layer, 2.5, 0.5, "a rectangular grid: a point beyond its shorter side");
}

// A grid whose edges are not along c0 and c1 fills only part of its bounding rectangle. In this one, c0 = i + 0.5 * j and
// c1 = 0.5 * i + j, so its corners are (0, 0), (2, 1), (1, 2) and (3, 3), and its bounding rectangle is 0 to 3 in both.
void check_slanted_grid() {
    const std::array<double, 6> slanted = {0, 1, 0.5, 0, 0.5, 1};
    const RaggedLayer layer = makeLayer(inFileOrder(makeSmallGrid(slanted)));
    checkValue(layer, 1.5, 1.5, 11.0, "a slanted grid: the point at i = 1, j = 1");
    checkValue(layer, 1.25, 1.75, 6.5, "a slanted grid: the point at i = 0.5, j = 1.5");
    checkNoValue(layer, 2.9, 0.1, "a slanted grid: a point in its bounding rectangle but not in the grid");
}

// Root grids need not share a step.
void check_grids_with_different_steps() {
    // c0 and c1 from 0 to 2 with a step of 1, and from 10 to 11 with a step of 0.5.
    const RaggedLayer layer =
        makeLayer(inFileOrder(makeSmallGrid(kAlongCoordinates, 100.0), makeSmallGrid({10, 0.5, 0, 10, 0, 0.5}, 200.0)));
    checkValue(layer, 0.5, 1.5, 106.5, "different steps: a point in the grid with a step of 1");
    checkValue(layer, 10.25, 10.75, 206.5, "different steps: a point in the grid with a step of 0.5");
}

// Root grids need not sit on a lattice of their own size or step.
void check_grids_not_on_a_common_lattice() {
    // The same size and step, so the second starts at 3, which is 1.5 times its size from the first, and the third at
    // 7.3, which is not a whole number of steps.
    const RaggedLayer layer =
        makeLayer(inFileOrder(makeSmallGrid(kAlongCoordinates, 100.0), makeSmallGrid({3, 1, 0, 0, 0, 1}, 200.0),
                              makeSmallGrid({7.3, 1, 0, 0, 0, 1}, 300.0)));
    checkValue(layer, 0.5, 1.5, 106.5, "no common lattice: a point in the first grid");
    checkValue(layer, 4.5, 1.5, 216.5, "no common lattice: a point in the grid 1.5 sizes along");
    checkValue(layer, 8.0, 1.0, 308.0, "no common lattice: a point in the grid at 7.3");
}

// Two grids with the same edges are both allowed, and the first in file order answers.
void check_grids_with_the_same_edges_resolve_by_file_order() {
    const auto first = [] { return makeSmallGrid(kAlongCoordinates, 100.0); };
    const auto second = [] { return makeSmallGrid(kAlongCoordinates, 200.0); };
    checkValue(makeLayer(inFileOrder(first(), second())), 0.5, 1.5, 106.5, "same edges: the first grid is first");
    checkValue(makeLayer(inFileOrder(second(), first())), 0.5, 1.5, 206.5, "same edges: the second grid is first");
}

void check_rejected_grids() {
    {
        RaggedLayer layer(0.0);
        check(addThrows<std::invalid_argument>(layer, inFileOrder(makeSmallGrid(kAlongCoordinates), GridPointer())),
              "a null grid is rejected");
        check(!layer.axisCount().has_value(), "a layer whose grids are rejected is left empty");
    }
    {
        RaggedLayer layer(0.0);
        layer.addRootGrids(inFileOrder(makeSmallGrid(kAlongCoordinates)));
        check(addThrows<std::logic_error>(layer, inFileOrder(makeSmallGrid(kAlongCoordinates))),
              "grids can be given to a layer once only");
    }
    {
        // The grids of a layer all have the same axis count.
        RaggedLayer layer(0.0);
        checkThrows<std::invalid_argument>(
            [&layer] {
                layer.addRootGrids(inFileOrder(makeGrid(kAlongCoordinates, 3, 3, 0.0, 1),
                                               makeGrid(kAlongCoordinates, 3, 3, 0.0, 2)));
            },
            "axisCount (2) does not match the first grid's own (1)", "grids with different axis counts");
        check(!layer.axisCount().has_value(), "a layer whose grids have different axis counts is left empty");
    }
}

void check_axis_count_and_layers_with_no_grid() {
    RaggedLayer neverGiven(0.0);
    check(!neverGiven.axisCount().has_value(), "a layer that was given no grids has no axis count");
    checkNoValue(neverGiven, 0.5, 0.5, "a layer that was given no grids");

    RaggedLayer emptyList(0.0);
    emptyList.addRootGrids(std::vector<GridPointer>());
    check(!emptyList.axisCount().has_value(), "a layer that was given an empty list of grids has no axis count");
    checkNoValue(emptyList, 0.5, 0.5, "a layer that was given an empty list of grids");

    const RaggedLayer oneAxis = makeLayer(inFileOrder(makeGrid(kAlongCoordinates, 3, 3, 0.0, 1)));
    check(oneAxis.axisCount() == 1, "the axis count of a layer of grids of one axis");
    const RaggedLayer twoAxes = makeLayer(inFileOrder(makeGrid(kAlongCoordinates, 3, 3, 0.0, 2),
                                                      makeGrid(kAcrossCoordinates, 3, 3, 0.0, 2)));
    check(twoAxes.axisCount() == 2, "the axis count of a layer of grids of two axes");
}

}  // namespace

int main() {
    check_grid_along_coordinates();
    check_grid_across_coordinates();
    check_layer_with_both_orientations();
    check_overlapping_root_grids_resolve_by_file_order();
    check_shared_edge_resolves_by_file_order();
    check_mixed_sizes_resolve_by_file_order();
    check_grids_of_different_sizes_are_found_in_any_order();
    check_rectangular_grid();
    check_slanted_grid();
    check_grids_with_different_steps();
    check_grids_not_on_a_common_lattice();
    check_grids_with_the_same_edges_resolve_by_file_order();
    check_rejected_grids();
    check_axis_count_and_layers_with_no_grid();

    return snap_test::finish("ragged_layer_test");
}
