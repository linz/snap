#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace deformation {

// One grid cell a query point resolved into: the base corner (bi, bj) and
// the fractional position within it (fi, fj) - what a grid's own node-data
// type (ScalarGridData::interpolate(), RaggedGridData::evaluate()) needs to do
// its bilinear weighting. fi/fj are each in [0, 1) for a query point in the
// grid's own interior, but can range as far as [-cellTolerance, 1+cellTolerance]
// for a point resolved via GridGeometry's own tolerance band near a
// boundary (see GridGeometry::locate()). bi/bj are clamped to valid node
// indices, which is what makes a point exactly on the grid's last node
// evaluate to weight 1.0 there and 0.0 on the opposite corner; fi/fj are
// deliberately left unclamped themselves, which is what allows that
// slightly-past-[0, 1) range within the tolerance band. Returned only by
// GridGeometry::locate() (and, bundled with the grid it belongs to, by
// a Layer's own resolution methods) once a query point has been confirmed to
// fall within a grid's own extent - never constructed by hand.
struct Cell {
    std::uint32_t bi;
    std::uint32_t bj;
    double fi;
    double fj;
};

// One affine-indexed grid's own geometry: node (i,j) <-> world (c0,c1), and
// the containment/locate logic behind grid resolution - shared by both
// GGRXF grid types (spec: "Grid resolution within a layer" resolves any
// grid, scalar_with_time_function or ragged_segments, by the same
// containment+priority rule, dispatching to grid-type-specific evaluation
// only afterward; see also LINZDEF's own `calc_grid_linear`, an
// axis-aligned special case of the same general affine transform).
class GridGeometry {
 public:
    // Throws std::invalid_argument if iNodeCount/jNodeCount is < 2 - same
    // "fail hard on a violated external-boundary invariant" reasoning as
    // TimeFunction's own constructor.
    GridGeometry(
        const std::array<double, 6>&
            affineCoeffs,  // node (i,j) -> (c0,c1): c0 = coeffs[0] + coeffs[1]*i + coeffs[2]*j, c1 = coeffs[3] + coeffs[4]*i + coeffs[5]*j. Which of c0/c1 is longitude/latitude is a caller concern (GGRXF: determined by the file's own interpolationCrsWkt) - GridGeometry itself is agnostic. Taken by const reference and copied, not by value: a plain 6-double aggregate has no move benefit over a copy, so by-value would only cost an extra copy for no gain
        std::uint32_t iNodeCount,  // grid extent along the first axis; must be >= 2
        std::uint32_t jNodeCount,  // grid extent along the second axis; must be >= 2
        double
            cellTolerance);  // boundary slop for locate(), in grid-index units - 0.0 reproduces LINZDEF's exact hard-edge behaviour; GGRXF's own spec value is 0.00001

    // Locates (c0, c1) within this grid: inverts affineCoeffs to a
    // fractional node index, and - only if that index falls within
    // [-cellTolerance, iNodeCount - 1 + cellTolerance] on each axis (the
    // closed containment check from "Grid spatial indexing attributes") -
    // returns the clamped base corner and bilinear weights as a Cell.
    // Returns std::nullopt if (c0, c1) is outside this grid's own extent,
    // even with tolerance.
    [[nodiscard]] std::optional<Cell> locate(
        double c0,  // coordinate on this grid's first axis (same units/order as affineCoeffs' own c0)
        double c1   // coordinate on this grid's second axis
    ) const;

    // True if `other`'s own extent - all 4 of its corner nodes, mapped
    // through its own affine transform to world coordinates - falls within
    // this grid's own extent (per locate(), so this grid's own
    // cellTolerance applies). An affine transform maps a rectangle to a
    // parallelogram, always convex, so checking the 4 corners is enough to
    // confirm the whole of `other` is contained, not just its corners.
    // Used by GridParent::addChild() to validate the "a child's extent must be a
    // subset of its parent's" invariant nesting depends on.
    [[nodiscard]] bool containsExtentOf(const GridGeometry& other) const;

    // Grid extent along the first/second axis (see the constructor).
    [[nodiscard]] std::uint32_t iNodeCount() const;
    [[nodiscard]] std::uint32_t jNodeCount() const;

    // This grid's own affine coefficients and boundary tolerance (see the
    // constructor) - exposed for a caller that needs to reconstruct this
    // grid's own lattice position directly (e.g. RaggedLayer's own
    // root-grid index), rather than only test containment via locate().
    [[nodiscard]] const std::array<double, 6>& affineCoeffs() const;
    [[nodiscard]] double cellTolerance() const;

 private:
    std::array<double, 6> affineCoeffs_;
    std::uint32_t iNodeCount_;  // grid extent along the first axis
    std::uint32_t jNodeCount_;  // grid extent along the second axis
    double cellTolerance_;      // boundary slop for locate(), in grid-index units
};

}  // namespace deformation
