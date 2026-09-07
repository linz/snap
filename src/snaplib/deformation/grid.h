#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace deformation {

// Supplies the raw, unscaled values stored at one GridData's nodes. One
// instance is bound to a single GridData's own node storage - a
// netCDF-backed implementation reads GGRXF's `displacement` variable, and a
// future LINZDEF-binary-backed one could read LINZDEF's own row-cached grid
// data, without GridData's own indexing/interpolation code needing to know
// which.
class NodeValueSource
{
public:
    virtual ~NodeValueSource() = default;

    // The number of displacement axes stored at each node (GGRXF's
    // `displacementCount`, LINZDEF's `ngrdval`) - at most 3, the real bound
    // both formats share. Only the first axisCount() entries nodeValues()
    // returns are meaningful; the rest are unspecified.
    virtual std::size_t axisCount() const = 0;

    // Returns the raw, unscaled values stored at node (i, j) (only the
    // first axisCount() entries are meaningful), or std::nullopt if that
    // node is marked as missing/undefined data.
    virtual std::optional<std::array<double, 3>> nodeValues(
        std::uint32_t i,   // node index along the grid's first axis, 0 <= i < iNodeCount
        std::uint32_t j    // node index along the grid's second axis, 0 <= j < jNodeCount
    ) const = 0;
};

// One grid cell a query point resolved into: the base corner (bi, bj) and
// the fractional position within it (fi, fj) - what GridData::
// interpolate(const Cell&) needs to do its bilinear weighting. fi/fj are
// each in [0, 1) for a query point in the grid's own interior, but can
// range as far as [-cellTolerance, 1+cellTolerance] for a point resolved
// via GridData's own tolerance band near a boundary (see GridData::
// locate()). bi/bj are clamped to valid node indices, which is what makes a
// point exactly on the grid's last node evaluate to weight 1.0 there and
// 0.0 on the opposite corner; fi/fj are deliberately left unclamped
// themselves, which is what allows that slightly-past-[0, 1) range within
// the tolerance band. Returned only by GridData::locate()/Grid::locate()
// (and, bundled with the grid it belongs to, by Grid's resolution methods
// below) once a query point has been confirmed to fall within a grid's own
// extent - never constructed by hand.
struct Cell
{
    std::uint32_t bi;
    std::uint32_t bj;
    double fi;
    double fj;
};

// One affine-indexed grid's own geometry and node data, generalising both
// GGRXF's own affine grid (spec: "Grid spatial indexing attributes") and
// LINZDEF's axis-aligned grid (dbl4_utl_grid.cpp's calc_grid_linear) as the
// same shape - LINZDEF's case is representable as a diagonal-only affine
// transform.
//
// GridData implements bilinear interpolation only - GGRXF's other
// interpolation methods and its `ragged_segments` grid type aren't covered
// here. GridData has no notion of hierarchy (no children, no priority) -
// that's Grid's job, below; GridData only ever answers "what's the value
// here", given a point already known to be inside it.
class GridData
{
public:
    // Throws std::invalid_argument if values is null or if iNodeCount/
    // jNodeCount is < 2 - same "fail hard on a violated external-boundary
    // invariant" reasoning as TimeFunction's own constructor.
    GridData( std::array<double, 6> affineCoeffs,          // node (i,j) -> (c0,c1): c0 = coeffs[0] + coeffs[1]*i + coeffs[2]*j, c1 = coeffs[3] + coeffs[4]*i + coeffs[5]*j. Which of c0/c1 is longitude/latitude is a caller concern (GGRXF: determined by the file's own interpolationCrsWkt) - GridData itself is agnostic
              std::uint32_t iNodeCount,                    // grid extent along the first axis; must be >= 2
              std::uint32_t jNodeCount,                    // grid extent along the second axis; must be >= 2
              double cellTolerance,                        // boundary slop for locate(), in grid-index units - 0.0 reproduces LINZDEF's exact hard-edge behaviour; GGRXF's own spec value is 0.00001
              double scaleFactor,                          // real-valued unit one raw stored value represents; applied once, after the weighted sum in interpolate() (matching dbl4_utl_grid.cpp's calc_grid_linear: sum raw values weighted, scale once)
              std::unique_ptr<NodeValueSource> values );   // this grid's own raw node data; must not be null

    // Locates (c0, c1) within this grid: inverts affineCoeffs to a
    // fractional node index, and - only if that index falls within
    // [-cellTolerance, iNodeCount - 1 + cellTolerance] on each axis (the
    // closed containment check from "Grid spatial indexing attributes") -
    // returns the clamped base corner and bilinear weights as a Cell.
    // Returns std::nullopt if (c0, c1) is outside this grid's own extent,
    // even with tolerance.
    std::optional<Cell> locate(
        double c0,   // coordinate on this grid's first axis (same units/order as affineCoeffs' own c0)
        double c1    // coordinate on this grid's second axis
    ) const;

    // 4-corner bilinear interpolation over `cell` (from a prior locate()
    // call on this same grid), scaled by scaleFactor - only the first
    // axisCount() entries of the result are meaningful; entries beyond that
    // are always exactly 0.0. Returns std::nullopt if any of the 4 corner
    // nodes is missing data.
    std::optional<std::array<double, 3>> interpolate( const Cell &cell ) const;

    // The number of displacement axes this grid's own NodeValueSource
    // returns per node (and hence per interpolate() call).
    std::size_t axisCount() const;

    // True if `other`'s own extent - all 4 of its corner nodes, mapped
    // through its own affine transform to world coordinates - falls within
    // this grid's own extent (per locate(), so this grid's own
    // cellTolerance applies). An affine transform maps a rectangle to a
    // parallelogram, always convex, so checking the 4 corners is enough to
    // confirm the whole of `other` is contained, not just its corners.
    // Used by Grid::addChild() to validate the "a child's extent must be a
    // subset of its parent's" invariant nesting depends on.
    bool containsExtentOf( const GridData &other ) const;

private:
    std::array<double, 6> affineCoeffs_;
    std::uint32_t iNodeCount_;               // grid extent along the first axis
    std::uint32_t jNodeCount_;               // grid extent along the second axis
    double cellTolerance_;                   // boundary slop for locate(), in grid-index units
    double scaleFactor_;                     // applied once, after interpolate()'s weighted sum
    std::unique_ptr<NodeValueSource> values_;   // this grid's own raw node data
};

class Grid;

// One grid a query point resolved to, together with the cell within it -
// what Grid's resolution methods below return, so a caller can pass the
// cell straight to GridData::interpolate(const Cell&)/Grid::interpolate()
// without relocating it.
struct Resolution
{
    const Grid *grid;
    Cell cell;
};

// One node in a grid hierarchy: one GridData, plus whichever child Grids
// describe the same physical contribution at finer resolution (GGRXF spec:
// "A grid may itself contain further grid subgroups nested inside it").
// Each child's own extent must be entirely contained within this grid's
// own (enforced by addChild(), see below) - nesting is how spatial
// *resolution* is expressed; it is a separate concept from spatial
// *coverage*, below.
//
// Picking a grid follows one rule, applied identically whether choosing
// among a Layer's own root grids or one Grid's own children(): the first
// entry in the list whose extent contains the query point wins - first
// come, first served. Entries may cover the same area (in which case list
// order matters) or different areas (in which case it doesn't) - either
// way, list order alone decides; see selectAmongSiblings() below. GGRXF's
// own gridPriority attribute, where a file has one, is simply translated
// into that order by whoever builds the hierarchy (a GGRXF parser; LINZDEF
// has no equivalent attribute at all) - Grid itself never stores or reads
// a priority number.
//
// Once a grid is picked by coverage, resolveChild() keeps descending into
// its own children - each one finer than the last - stopping as soon as
// none of the current grid's own children cover the query point. The
// current grid's own value is then used. Grid has no parent pointer: this
// only ever walks top-down and never needs to go back up.
class Grid
{
public:
    explicit Grid( GridData data );   // this grid's own geometry and node data; children are added afterward via addChild()

    const GridData &data() const;

    // Forwards to data().locate()/interpolate()/axisCount() - see GridData.
    // Kept here too so a caller holding a Grid (as selectAmongSiblings()/
    // resolveChild() callers do) never needs to go via data() just to
    // query it.
    std::optional<Cell> locate( double c0, double c1 ) const;
    std::optional<std::array<double, 3>> interpolate( const Cell &cell ) const;
    std::size_t axisCount() const;

    // Adds a child grid. Ownership transfers to this Grid. If this grid
    // has multiple children that could both cover the same point, add
    // them in the order they should be tried - first come, first served
    // (see the class comment above); children covering different areas
    // can be added in any order.
    //
    // Two invariants are checked and enforced here: every child's own
    // axisCount() must equal this grid's own (GGRXF's gridParameters is
    // declared once per layer, so every grid nested anywhere under it
    // necessarily agrees), and the child's own extent must be entirely
    // contained within this grid's own (see GridData::containsExtentOf()).
    // A file that violates either is malformed (or this is a parser bug),
    // so this throws rather than silently accepting bad data - same
    // reasoning as TimeFunction's own constructor validation.
    void addChild( std::unique_ptr<Grid> child );   // the child grid to add; throws std::invalid_argument if child is null, child->axisCount() != this->axisCount(), or child's own extent is not contained within this grid's own

    // This grid's own child grids, in the order they were added (see
    // addChild()). Never contains null entries.
    const std::vector<std::unique_ptr<Grid>> &children() const;

    // True if this grid has no children.
    bool isLeaf() const;

    // Picks among this grid's own children() - equivalent to
    // selectAmongSiblings(children(), c0, c1), for the common case of
    // choosing one of a specific grid's own children rather than a
    // Layer's root grids (which no single Grid owns, so must go through
    // the static selectAmongSiblings() form directly).
    std::optional<Resolution> resolveChild( double c0, double c1 ) const;

    // Picks a single grid among `siblings` that contains (c0, c1) - the
    // first-come-first-served rule from the class comment above, used
    // identically for a Layer's own root grids and for any Grid's own
    // children(). Returns std::nullopt if none of `siblings` contains the
    // point. This only ever inspects `siblings` itself, never anything
    // deeper - choosing further into the picked sibling's own children is
    // the caller's job (see resolveChild()).
    static std::optional<Resolution> selectAmongSiblings(
        const std::vector<std::unique_ptr<Grid>> &siblings,   // a Layer's own root grids, or one Grid's own children()
        double c0,                                             // coordinate on the siblings' shared first axis
        double c1                                              // coordinate on the siblings' shared second axis
    );

private:
    GridData data_;
    std::vector<std::unique_ptr<Grid>> children_;   // this grid's own child grids, in the order they were added
};

} // namespace deformation
