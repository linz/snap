#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "grid.h"
#include "layer.h"
#include "ragged_grid_data.h"

namespace deformation {

// One GGRXF-style layer covering the `ragged_segments` grid type: a set of
// root grids, one of which is picked per query point - the first one in the
// list whose extent contains the point, same first-come-first-served rule
// ScalarLayer's own root scan uses. Unlike ScalarLayer, there is no
// grid-wide time function to share - each ragged grid's own nodes carry
// their own independent segment chains (see RaggedGridData) - so this only
// ever needs a single reference date, not a summed set of TimeFunctions.
//
// Real files (the InSAR quadtree-tiling producer this format was designed
// for) put potentially thousands of root grids directly under one
// RaggedLayer, as a flat, non-overlapping partition. Resolving which one
// covers a query point by testing every root grid in turn (as ScalarLayer
// does for its own, typically small, root list) doesn't scale to that - see
// resolveRootGrid() below for how this layer resolves its own root grids
// instead.
class RaggedLayer : public Layer
{
public:
    explicit RaggedLayer( double referenceDateDays );   // this layer's own reference date, in the same epoch as evaluate()'s own queryDateDays and every TimeFunction::Params field - every RaggedSegment's own startDay (see ragged_grid_data.h) is relative to this, matching the real GGRXF spec's own per-layer reference_date attribute for `ragged_segments` layers

    // Adds a root grid to this layer. Ownership transfers to this Layer.
    // Real root grids are a flat, non-overlapping partition, added in any
    // order - unlike ScalarLayer's own addRootGrid(), list order plays no
    // role in resolveRootGrid() below. The first root grid added
    // establishes this layer's own axisCount(); every later one must match
    // it - same reasoning as GridParent::addChild()'s own validation. It also
    // establishes this layer's own shared lattice reference (origin/step,
    // taken from the grid's own affineCoeffs, assumed axis-aligned - real
    // ragged root grids always are, see GridGeometry) - every later grid's
    // own step must match it, since every root grid under one real
    // RaggedLayer shares one native lattice step.
    void addRootGrid( std::unique_ptr<Grid<RaggedGridData>> grid );   // the grid to add; throws std::invalid_argument if grid is null, if a root grid was added before and grid->axisCount() != axisCount(), if the grid's own affine transform isn't axis-aligned, or if its own lattice step doesn't match the first root grid's own

    // The number of displacement axes every grid under this layer shares -
    // established by the first root grid added (see addRootGrid()).
    // Returns std::nullopt if no root grid has been added yet.
    std::optional<std::size_t> axisCount() const;

    // Evaluates this layer's own contribution at (c0, c1, queryDateDays):
    // picks a single grid via resolveRootGrid() below, then evaluates it
    // at queryDateDays - referenceDateDays_ (converted to this layer's own
    // relative day count) rather than summing time functions, since a
    // ragged grid's own value computation already accounts for its own
    // per-node timing (see RaggedGridData::evaluate()). Returns
    // std::nullopt if no root grid (or none of its descendants) contains
    // the point.
    std::optional<std::array<double, 3>> evaluate( double c0, double c1, double queryDateDays ) const override;

private:
    // One root grid's own lattice position, relative to this layer's own
    // shared reference established by the first root grid added:
    // sizeSteps is iNodeCount() - 1 (a power-of-two multiple of the
    // smallest sizeSteps present, for a real quadtree-tiled file), and
    // (row0, col0) is this grid's own origin, in units of one native
    // lattice step.
    struct RootKey
    {
        std::int64_t row0;
        std::int64_t col0;
        std::int64_t sizeSteps;

        bool operator==( const RootKey &other ) const;
    };

    struct RootKeyHash
    {
        std::size_t operator()( const RootKey &key ) const;
    };

    // Registers `grid` into rootIndex_/distinctSizeSteps_ - establishing
    // this layer's own shared lattice reference if this is the first root
    // grid added, or validating against it otherwise (see addRootGrid()).
    void indexRootGrid( const Grid<RaggedGridData> &grid );

    // Resolves this layer's own root grids at (c0, c1) - a bucket lookup,
    // not a tree: rootIndex_ is one hash map, but folding sizeSteps into
    // its own key means it behaves as several independent *uniform*
    // bucket grids overlaid on the same space, one per distinct sizeSteps
    // actually present (few, for a real file), rather than one bucket size
    // forced to fit every tile in it (see this layer's own class comment
    // for why that doesn't work). A huge root grid is found via its own
    // coarse "bucket grid" in one lookup, a tiny one via its own fine one.
    //
    // Sizes are tried one at a time, and at each size the exact
    // (non-tolerant) primary candidate - found by plain floor-division, no
    // hash entries scanned "nearby" - is tried first. Only when that exact
    // attempt fails (its bucket is empty, or occupied but the grid there
    // genuinely doesn't contain the point) does a tolerance-widened
    // same-size neighbour check happen, and only right there, at that
    // size - never speculatively at a size not yet tried, never
    // re-applied to a size that already succeeded exactly. This is the
    // same "resolve exactly first, only reach for tolerance at the actual
    // point of failure" principle a hierarchical descent would need too,
    // just applied per size here rather than per tree level: root grids
    // have no parent/child relationship *with each other* (that's what
    // makes them roots), so there's no tree to exploit at this specific,
    // root-level selection step - this says nothing about descent
    // generally, which still happens exactly as before, into whichever
    // children a resolved root grid actually has (see evaluate() below).
    //
    // A query point sitting exactly on a shared boundary between two
    // different-size tiles can find either one, depending only on which
    // size happens to be tried first - never both, and never resolved by
    // an explicit priority rule (Grid never stores a gridPriority; nesting
    // and list order already realise priority elsewhere - see Grid's own
    // class comment). This is fine because real adjacent tiles agree in
    // value at a shared node regardless of which one answers - there is
    // no genuine ambiguity to arbitrate, only a choice between two equally
    // correct answers.
    std::optional<Resolution<RaggedGridData>> resolveRootGrid( double c0, double c1 ) const;

    double referenceDateDays_;
    std::vector<std::unique_ptr<Grid<RaggedGridData>>> rootGrids_;   // this layer's own root grids, in the order they were added; owns them
    // Non-owning, read-only lookup alongside rootGrids_ (the real owner) -
    // see resolveRootGrid(). reference_wrapper rather than a raw pointer:
    // every entry is always inserted already pointing at a real grid (see
    // indexRootGrid()), so there is no "not yet indexed" state this needs
    // to represent (absence is a missing *key*, checked via rootIndex_.
    // find() == end(), never a null *value* under an existing one) -
    // reference_wrapper makes "never null" a compile-time guarantee here
    // too, the same reasoning as Resolution::grid (see grid.h).
    std::unordered_map<RootKey, std::reference_wrapper<const Grid<RaggedGridData>>, RootKeyHash> rootIndex_;
    std::vector<std::int64_t> distinctSizeSteps_;   // every distinct sizeSteps value present in rootIndex_, in the order first seen
    bool haveReference_ = false;
    double refC0Origin_ = 0.0;
    double refC0Step_ = 0.0;
    double refC1Origin_ = 0.0;
    double refC1Step_ = 0.0;
    double cellTolerance_ = 0.0;   // this layer's own shared boundary tolerance, in the same raw lattice-index units as rawRow/rawCol in resolveRootGrid() - established alongside the lattice reference above, from the first root grid added
};

} // namespace deformation
