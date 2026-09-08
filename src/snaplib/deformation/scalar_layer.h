#pragma once

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "grid.h"
#include "layer.h"
#include "scalar_grid_data.h"
#include "time_function.h"

namespace deformation {

// One GGRXF-style layer covering the `scalar_with_time_function` grid type:
// a set of root grids, one of which is picked per query point (see Grid's
// own class comment for the first-come-first-served rule that applies
// identically here), sharing one set of time functions summed to scale
// whichever grid is picked. Every grid under a layer - root or descendant,
// however many roots there are - shares that one set: nesting/multiple
// roots exist to cover different areas and/or refine the same physical
// event at finer resolution, not to combine events with different timing.
// A contribution needing different timing needs a different Layer. GGRXF's
// other grid type, `ragged_segments` (independent per-node time series), is
// covered by RaggedLayer instead.
class ScalarLayer : public Layer
{
public:
    // Adds a root grid to this layer. Ownership transfers to this Layer.
    // If this layer has multiple root grids that could both cover the
    // same point, add them in the order they should be tried - evaluate()
    // picks the first one found, first come first served; root grids
    // covering different areas can be added in any order. The first root
    // grid added establishes this layer's own axisCount(); every later
    // one must match it - same reasoning as GridParent::addChild()'s own
    // validation.
    void addRootGrid( std::unique_ptr<Grid<ScalarGridData>> grid );   // the grid to add; throws std::invalid_argument if grid is null, or if a root grid was added before and grid->axisCount() != axisCount()

    // Adds one of this layer's own time functions. Every grid underneath
    // this layer is scaled by the sum of every time function added this
    // way, evaluated at the query date (GGXF's own CompoundTimeFunction
    // summation, adopted here for `scalar_with_time_function`).
    void addTimeFunction( const TimeFunction &timeFunction );   // copied - see TimeFunction's own constructor for why (no heap-owning members, no move benefit over a copy)

    // The number of displacement axes every grid under this layer shares
    // (GGRXF's own gridParameters is declared once per layer, so every grid
    // underneath it necessarily agrees) - established by the first root
    // grid added (see addRootGrid()). Returns std::nullopt if no root grid
    // has been added yet.
    std::optional<std::size_t> axisCount() const;

    // Evaluates this layer's own contribution at (c0, c1, queryDateDays):
    // picks a single grid - a plain linear scan among this layer's own root
    // grids for the first one covering the point, then descendToLeaf()
    // (grid.h) into that grid's own children, and so on until none of them
    // cover the point - interpolates the picked grid, and scales the result
    // by the sum of every time function evaluated at queryDateDays. Like
    // GridParent's own child resolution (see grid.h), this never scans the
    // whole hierarchy - only the current grid's immediate children are ever
    // inspected at each step.
    //
    // Returns the picked grid's own interpolated values (only the first
    // axisCount() entries meaningful), scaled by the summed time functions
    // - or std::nullopt if no root grid (or none of its descendants)
    // contains the point, or the picked grid has missing data there.
    std::optional<std::array<double, 3>> evaluate( double c0, double c1, double queryDateDays ) const override;

private:
    std::vector<std::unique_ptr<Grid<ScalarGridData>>> rootGrids_;   // this layer's own root grids, in the order they were added
    std::vector<TimeFunction> timeFunctions_;        // this layer's own time functions, summed at evaluate() time
};

} // namespace deformation
