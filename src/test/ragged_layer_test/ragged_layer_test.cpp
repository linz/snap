// Standalone unit test for RaggedLayer (deformation/ragged_layer.h), which finds the root grid that a point is in.
// Failures are reported to stdout and the process exit code is the pass/fail signal (0 = all passed), as for the other
// test-only tools in src/test/.
//
// The grids are built in code. Every grid has the same values: node (i, j) is 10 * i + j on its one axis, at every day.
// Bilinear interpolation of that is exact, so the value at a point shows which node indices the point was found by,
// and so which way round the grid's own axes run.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
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

namespace {

using deformation::Grid;
using deformation::GridGeometry;
using deformation::RaggedGridData;
using deformation::RaggedLayer;
using deformation::RaggedNodeValueSource;
using deformation::RaggedSegment;
using deformation::RaggedSegmentRange;

/// The boundary tolerance of a grid, which is the one that the GGRXF specification gives.
constexpr double kCellTolerance = 0.00001;

/// How far a value may be from the one expected.
constexpr double kValueTolerance = 1e-9;

int failures = 0;

void check(const bool condition, const std::string& description) {
    if (condition) {
        return;
    }
    ++failures;
    std::cout << "FAIL: " << description << "\n";
}

/// The values of a grid: node (i, j) has the value 10 * i + j on its one axis, from day 0 on, and does not change.
class NodeIndexValues : public RaggedNodeValueSource {
 public:
    NodeIndexValues(const std::uint32_t iNodeCount,  ///< how many nodes the grid has along i
                    const std::uint32_t jNodeCount)  ///< how many nodes the grid has along j
        : _jNodeCount(jNodeCount) {
        for (std::uint32_t i = 0; i < iNodeCount; ++i) {
            for (std::uint32_t j = 0; j < jNodeCount; ++j) {
                _segments.push_back(RaggedSegment{0.0, 10.0 * i + j, 0.0});
            }
        }
    }

    [[nodiscard]] std::size_t axisCount() const override {
        return 1;
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
    std::vector<RaggedSegment> _segments;
};

/// \return a grid with those coefficients and node counts, that has the values of NodeIndexValues.
std::unique_ptr<Grid<RaggedGridData>> makeGrid(
    const std::array<double, 6>& affineCoeffs,  ///< the grid's node (i, j) to (c0, c1) transform
    const std::uint32_t iNodeCount,             ///< how many nodes the grid has along i
    const std::uint32_t jNodeCount) {           ///< how many nodes the grid has along j
    const GridGeometry geometry(affineCoeffs, iNodeCount, jNodeCount, kCellTolerance);
    return std::make_unique<Grid<RaggedGridData>>(
        geometry, RaggedGridData(std::make_unique<NodeIndexValues>(iNodeCount, jNodeCount)));
}

/// \return a square grid of three nodes along each side, with those coefficients.
std::unique_ptr<Grid<RaggedGridData>> makeSmallGrid(const std::array<double, 6>& affineCoeffs) {
    return makeGrid(affineCoeffs, 3, 3);
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

/// \return whether adding the grid to the layer throws an std::invalid_argument.
bool addThrows(RaggedLayer& layer,                            ///< the layer to add it to
               std::unique_ptr<Grid<RaggedGridData>> grid) {  ///< the grid to add
    try {
        layer.addRootGrid(std::move(grid));
    } catch (const std::invalid_argument&) {
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
    RaggedLayer layer(0.0);
    layer.addRootGrid(makeSmallGrid(kAlongCoordinates));
    checkValue(layer, 0.5, 1.5, 6.5, "c0 along i: a point in the grid");
    checkNoValue(layer, 5.0, 5.0, "c0 along i: a point outside the grid");
}

void check_grid_across_coordinates() {
    RaggedLayer layer(0.0);
    layer.addRootGrid(makeSmallGrid(kAcrossCoordinates));
    checkValue(layer, 0.5, 1.5, 15.5, "c0 along j: a point in the grid");
    checkNoValue(layer, 5.0, 5.0, "c0 along j: a point outside the grid");
}

void check_layer_with_both_orientations() {
    RaggedLayer layer(0.0);
    layer.addRootGrid(makeSmallGrid(kAlongCoordinates));

    // c0 from 2 to 4 and c1 from 0 to 2, next to the first grid, with c0 along j.
    const std::array<double, 6> acrossFromTwo = {2, 0, 1, 0, 1, 0};
    layer.addRootGrid(makeSmallGrid(acrossFromTwo));

    checkValue(layer, 0.5, 1.5, 6.5, "both orientations: a point in the grid with c0 along i");
    checkValue(layer, 2.5, 1.5, 15.5, "both orientations: a point in the grid with c0 along j");
    checkNoValue(layer, 5.0, 1.5, "both orientations: a point past both grids");
}

void check_rejected_grids() {
    {
        RaggedLayer layer(0.0);
        const std::array<double, 6> slanted = {0, 1, 0.5, 0, 0.5, 1};
        check(addThrows(layer, makeSmallGrid(slanted)), "a grid that is not axis-aligned is rejected");
    }
    {
        RaggedLayer layer(0.0);
        check(addThrows(layer, makeGrid(kAlongCoordinates, 3, 4)), "a grid that is not square is rejected");
    }
    {
        RaggedLayer layer(0.0);
        layer.addRootGrid(makeSmallGrid(kAlongCoordinates));
        const std::array<double, 6> acrossWithOtherStep = {0, 0, 2, 0, 2, 0};
        check(addThrows(layer, makeSmallGrid(acrossWithOtherStep)),
              "a grid whose step differs from the first grid's, along the other index, is rejected");
    }
}

}  // namespace

int main() {
    check_grid_along_coordinates();
    check_grid_across_coordinates();
    check_layer_with_both_orientations();
    check_rejected_grids();

    if (failures > 0) {
        std::cout << failures << " ragged_layer_test check(s) failed\n";
        return 1;
    }
    std::cout << "ragged_layer_test: all checks passed\n";
    return 0;
}
