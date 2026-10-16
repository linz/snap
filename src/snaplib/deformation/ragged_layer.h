#pragma once

#include <memory>
#include <optional>
#include <vector>
#include <cstddef>
#include "grid.h"
#include "layer.h"
#include "ragged_grid_data.h"
#include "rectangle_index.h"

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
// RaggedLayer, as a flat, non-overlapping partition. The format does not
// require that, nor that the grids are square, or of one size or step, or
// aligned to each other. Resolving which one covers a query point by testing
// every root grid in turn (as ScalarLayer does for its own, typically small,
// root list) doesn't scale to thousands of grids, so this layer finds them
// with a RectangleIndex instead - see resolveRootGrid() below.
class RaggedLayer : public Layer {
 public:
    explicit RaggedLayer(
        double
            referenceDateDays);  // this layer's own reference date, in the same epoch as evaluate()'s own queryDateDays and every TimeFunction::Params field - every RaggedSegment's own startDay (see ragged_grid_data.h) is relative to this, matching the real GGRXF spec's own per-layer reference_date attribute for `ragged_segments` layers

    // Sets this layer's root grids, which must be given in the order that the
    // file has them. Ownership transfers to this Layer. Where root grids
    // overlap, the first one in that order that contains a query point answers
    // it, the same first-come-first-served rule ScalarLayer's own root scan
    // uses - real root grids are usually a flat, non-overlapping partition,
    // so this seldom decides anything. This can be called once only.
    //
    // The first grid's axisCount() becomes this layer's own, and every other
    // grid must match it - same reasoning as GridParent::addChild()'s own
    // validation. Nothing else is required of the grids: they can be of any
    // shape, size, step or affine transform, can overlap, and need not be
    // aligned to each other.
    void addRootGrids(
        std::vector<std::unique_ptr<Grid<RaggedGridData>>>
            grids);  // the root grids, in file order; throws std::invalid_argument if one is null or if one's axisCount() differs from the first's, and std::logic_error if this layer already has root grids

    // The number of displacement axes every grid under this layer shares -
    // established by the first root grid (see addRootGrids()). Returns
    // std::nullopt if no root grid has been added yet.
    [[nodiscard]] std::optional<std::size_t> axisCount() const;

    // Evaluates this layer's own contribution at (c0, c1, queryDateDays):
    // picks a single grid via resolveRootGrid() below, then evaluates it
    // at queryDateDays - referenceDateDays_ (converted to this layer's own
    // relative day count) rather than summing time functions, since a
    // ragged grid's own value computation already accounts for its own
    // per-node timing (see RaggedGridData::evaluate()). Returns
    // std::nullopt if no root grid (or none of its descendants) contains
    // the point.
    [[nodiscard]] std::optional<std::array<double, 3>> evaluate(double c0, double c1, double queryDateDays) const override;

 private:
    // Finds the root grid that (c0, c1) is in: the first one in file order
    // that contains the point, so overlapping root grids, and a point on an
    // edge that two root grids share, are resolved by file order, as
    // ScalarLayer's own root scan resolves them. rootIndex_ finds the
    // candidates, which are the root grids whose bounding rectangle contains
    // the point. Each candidate is then tested with Grid::locate(), the exact
    // test, since a root grid whose edges are not along c0 and c1 fills only
    // part of its bounding rectangle. Returns std::nullopt if no root grid
    // contains the point.
    //
    // Root grids have no parent/child relationship *with each other* (that's
    // what makes them roots), so there's no tree to exploit at this specific,
    // root-level selection step. Descent into the children of the root grid
    // that is returned happens afterwards, in evaluate() below.
    [[nodiscard]] std::optional<Resolution<RaggedGridData>> resolveRootGrid(double c0, double c1) const;

    double referenceDateDays_;
    std::vector<std::unique_ptr<Grid<RaggedGridData>>>
        rootGrids_;  // this layer's own root grids, in file order; owns them

    // The bounding rectangle of each root grid in rootGrids_, widened by the
    // grid's own cellTolerance, with the grid's position in rootGrids_ as the
    // rectangle's id. Empty until addRootGrids() is called.
    std::optional<RectangleIndex> rootIndex_;
};

}  // namespace deformation
