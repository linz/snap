// Standalone unit test for GridGeometry (deformation/grid_geometry.h): the validation of the node counts, locate() for
// an axis-aligned, a swapped-axis, a rotated and a sheared grid, the clamping at the last node, the tolerance band at
// the edges, containsExtentOf(), and the accessors. The coordinates are chosen so that every expected value is exact in
// binary floating point, so the checks compare with ==. Failures are reported to stdout and the process exit code is
// the pass/fail signal (0 = all passed), as for the other test-only tools in src/test/.

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

#include "deformation/grid_geometry.h"
#include "test_support.h"

namespace {

using deformation::Cell;
using deformation::GridGeometry;
using snap_test::check;

/// Checks that locate() gave a cell with exactly the base corner and the fractional position.
void checkCell(const std::optional<Cell>& cell, const std::uint32_t bi, const std::uint32_t bj, const double fi,
               const double fj, const std::string& messageOnError) {
    if (!cell) {
        check(false, messageOnError + ": the point was not located");
        return;
    }
    check(cell->bi == bi && cell->bj == bj && cell->fi == fi && cell->fj == fj,
          messageOnError + ": got (" + std::to_string(cell->bi) + ", " + std::to_string(cell->bj) + ", " +
              std::to_string(cell->fi) + ", " + std::to_string(cell->fj) + "), expected (" + std::to_string(bi) + ", " +
              std::to_string(bj) + ", " + std::to_string(fi) + ", " + std::to_string(fj) + ")");
}

/// \return whether making a GridGeometry with the node counts throws an std::invalid_argument.
bool constructionThrows(const std::uint32_t iNodeCount, const std::uint32_t jNodeCount) {
    try {
        const GridGeometry geometry({0.0, 1.0, 0.0, 0.0, 0.0, 1.0}, iNodeCount, jNodeCount, 0.0);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

// Node (i, j) is at c0 = 10 + 2 i and c1 = 20 + 4 j, so the 5 by 4 nodes cover c0 in [10, 18] and c1 in [20, 32].
const std::array<double, 6> axisAlignedCoeffs = {10.0, 2.0, 0.0, 20.0, 0.0, 4.0};

void check_node_counts() {
    check(constructionThrows(1, 5), "an iNodeCount of 1");
    check(constructionThrows(5, 1), "a jNodeCount of 1");
    check(constructionThrows(0, 0), "node counts of 0");
    check(constructionThrows(1, 1), "node counts of 1");
    check(!constructionThrows(2, 2), "node counts of 2, the smallest grid");
}

void check_locate_on_an_axis_aligned_grid() {
    const GridGeometry geometry(axisAlignedCoeffs, 5, 4, 0.0);
    checkCell(geometry.locate(13.0, 26.0), 1, 1, 0.5, 0.5, "the middle of a cell");
    checkCell(geometry.locate(10.0, 20.0), 0, 0, 0.0, 0.0, "the first node");
    checkCell(geometry.locate(14.0, 28.0), 2, 2, 0.0, 0.0, "a node in the interior");
}

void check_locate_at_the_last_node() {
    const GridGeometry geometry(axisAlignedCoeffs, 5, 4, 0.0);
    // The base corner is clamped to the last cell, so the last node is the far corner of that cell, with weight 1.
    checkCell(geometry.locate(18.0, 32.0), 3, 2, 1.0, 1.0, "the last node");
    checkCell(geometry.locate(18.0, 20.0), 3, 0, 1.0, 0.0, "the last node along i only");
    checkCell(geometry.locate(10.0, 32.0), 0, 2, 0.0, 1.0, "the last node along j only");
}

void check_locate_outside_each_side() {
    const GridGeometry geometry(axisAlignedCoeffs, 5, 4, 0.0);
    check(!geometry.locate(9.5, 26.0), "below the first c0");
    check(!geometry.locate(18.5, 26.0), "above the last c0");
    check(!geometry.locate(13.0, 19.5), "below the first c1");
    check(!geometry.locate(13.0, 32.5), "above the last c1");
}

void check_locate_on_a_swapped_axis_grid() {
    // c0 = j and c1 = i, so the first axis of the grid runs along c1.
    const GridGeometry geometry({0.0, 0.0, 1.0, 0.0, 1.0, 0.0}, 3, 5, 0.0);
    checkCell(geometry.locate(2.5, 1.5), 1, 2, 0.5, 0.5, "a point on a swapped-axis grid");
    check(!geometry.locate(5.0, 1.0), "outside a swapped-axis grid along c0, which is its j axis");
    check(!geometry.locate(1.0, 3.0), "outside a swapped-axis grid along c1, which is its i axis");
}

void check_locate_on_a_rotated_grid() {
    // c0 = i + j and c1 = j - i, with node (0, 0) at the origin (0, 0). A step of 1 in i moves the point by (+1, -1)
    // in (c0, c1) and a step of 1 in j moves it by (+1, +1), so the grid is rotated 45 degrees away from the standard
    // XY alignment (and scaled by the square root of 2): neither of its axes is along an axis of the coordinates, and
    // its axes are still perpendicular.
    const GridGeometry geometry({0.0, 1.0, 1.0, 0.0, -1.0, 1.0}, 4, 4, 0.0);
    checkCell(geometry.locate(3.0, 1.0), 1, 2, 0.0, 0.0, "a node of a rotated grid");
    checkCell(geometry.locate(3.5, 1.5), 1, 2, 0.0, 0.5, "between two nodes of a rotated grid");
    check(!geometry.locate(0.0, 3.0), "outside a rotated grid, where i would be negative");
}

void check_locate_on_a_sheared_grid() {
    // c0 = i + 0.5 j and c1 = j, with node (0, 0) at the origin (0, 0). A step of 1 in i moves the point by (+1, 0) in
    // (c0, c1) and a step of 1 in j moves it by (+0.5, +1): the j axis leans over while the i axis stays along c0, so
    // the axes are not perpendicular.
    const GridGeometry geometry({0.0, 1.0, 0.5, 0.0, 0.0, 1.0}, 4, 4, 0.0);
    checkCell(geometry.locate(2.5, 1.0), 2, 1, 0.0, 0.0, "a node of a sheared grid");
    checkCell(geometry.locate(3.0, 1.5), 2, 1, 0.25, 0.5, "between two nodes of a sheared grid");
    checkCell(geometry.locate(4.5, 3.0), 2, 2, 1.0, 1.0, "the last node of a sheared grid");
    // At c1 = 2 the row of nodes with j = 2 starts at c0 = 1, so c0 = 0 is left of it.
    check(!geometry.locate(0.0, 2.0), "outside a sheared grid, where i would be negative");
}

void check_locate_in_the_tolerance_band() {
    // A tolerance of 0.25 of a cell on each side.
    const GridGeometry geometry(axisAlignedCoeffs, 5, 4, 0.25);
    checkCell(geometry.locate(9.75, 20.0), 0, 0, -0.125, 0.0, "just below the first i, inside the tolerance");
    checkCell(geometry.locate(18.5, 32.0), 3, 2, 1.25, 1.0, "exactly at the upper i tolerance limit");
    checkCell(geometry.locate(10.0, 19.5), 0, 0, 0.0, -0.125, "just below the first j, inside the tolerance");
    checkCell(geometry.locate(18.0, 33.0), 3, 2, 1.0, 1.25, "exactly at the upper j tolerance limit");
    check(!geometry.locate(9.0, 26.0), "below the first i, beyond the tolerance");
    check(!geometry.locate(18.75, 26.0), "above the last i, beyond the tolerance");
    check(!geometry.locate(13.0, 18.0), "below the first j, beyond the tolerance");
    check(!geometry.locate(13.0, 34.0), "above the last j, beyond the tolerance");
}

void check_locate_with_no_tolerance() {
    const GridGeometry geometry(axisAlignedCoeffs, 5, 4, 0.0);
    check(geometry.locate(10.0, 20.0).has_value(), "the first node, with no tolerance");
    check(!geometry.locate(9.75, 20.0), "a quarter of a cell below the first i, with no tolerance");
    check(!geometry.locate(18.25, 32.0), "a quarter of a cell above the last i, with no tolerance");
}

void check_contains_extent_of() {
    const GridGeometry parent(axisAlignedCoeffs, 5, 4, 0.0);

    // c0 in [12, 14] and c1 in [24, 28]
    check(parent.containsExtentOf(GridGeometry({12.0, 1.0, 0.0, 24.0, 0.0, 2.0}, 3, 3, 0.0)),
          "a grid inside the parent");
    check(parent.containsExtentOf(parent), "a grid with the extent of the parent itself");

    // c0 in [16, 19], past the parent's last c0 of 18
    check(!parent.containsExtentOf(GridGeometry({16.0, 1.0, 0.0, 24.0, 0.0, 2.0}, 4, 3, 0.0)),
          "a grid that extends past the last c0");
    // c0 in [8, 10], before the parent's first c0 of 10
    check(!parent.containsExtentOf(GridGeometry({8.0, 1.0, 0.0, 24.0, 0.0, 2.0}, 3, 3, 0.0)),
          "a grid that extends before the first c0");
    // c1 in [24, 34], past the parent's last c1 of 32
    check(!parent.containsExtentOf(GridGeometry({12.0, 1.0, 0.0, 24.0, 0.0, 2.0}, 3, 6, 0.0)),
          "a grid that extends past the last c1");
    // c1 in [18, 22], before the parent's first c1 of 20
    check(!parent.containsExtentOf(GridGeometry({12.0, 1.0, 0.0, 18.0, 0.0, 2.0}, 3, 3, 0.0)),
          "a grid that extends before the first c1");

    // Containment is one way: the parent is not inside a grid that is inside the parent.
    const GridGeometry child({12.0, 1.0, 0.0, 24.0, 0.0, 2.0}, 3, 3, 0.0);
    check(!child.containsExtentOf(parent), "the parent inside its own child");
}

void check_contains_extent_of_uses_the_tolerance() {
    // c0 in [16.5, 18.5], half a cell past the parent's last c0 of 18: 0.25 of a parent cell.
    const GridGeometry edge({16.5, 1.0, 0.0, 24.0, 0.0, 2.0}, 3, 3, 0.0);
    check(GridGeometry(axisAlignedCoeffs, 5, 4, 0.25).containsExtentOf(edge),
          "a grid that ends exactly at the parent's tolerance limit");
    check(!GridGeometry(axisAlignedCoeffs, 5, 4, 0.0).containsExtentOf(edge),
          "the same grid, for a parent with no tolerance");
}

void check_accessors() {
    const GridGeometry geometry({1.0, 2.0, 3.0, 4.0, 5.0, 6.0}, 7, 8, 0.5);
    check(geometry.iNodeCount() == 7, "iNodeCount");
    check(geometry.jNodeCount() == 8, "jNodeCount");
    check(geometry.cellTolerance() == 0.5, "cellTolerance");
    check(geometry.affineCoeffs() == std::array<double, 6>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0}), "affineCoeffs");
}

}  // namespace

int main() {
    check_node_counts();
    check_locate_on_an_axis_aligned_grid();
    check_locate_at_the_last_node();
    check_locate_outside_each_side();
    check_locate_on_a_swapped_axis_grid();
    check_locate_on_a_rotated_grid();
    check_locate_on_a_sheared_grid();
    check_locate_in_the_tolerance_band();
    check_locate_with_no_tolerance();
    check_contains_extent_of();
    check_contains_extent_of_uses_the_tolerance();
    check_accessors();

    return snap_test::finish("grid_geometry_test");
}
