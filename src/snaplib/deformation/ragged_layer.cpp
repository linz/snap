#include "deformation/ragged_layer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include "deformation/grid.h"
#include "deformation/grid_geometry.h"
#include "deformation/ragged_grid_data.h"
#include "deformation/rectangle_index.h"

namespace deformation {

namespace {

/// Makes the rectangle that bounds a grid in (c0, c1) coordinates, for RaggedLayer::rootIndex_. It is the smallest
/// rectangle, with its edges along c0 and c1, that holds the four corner nodes of the grid, and each side of it is then
/// widened by as much as GridGeometry::locate() allows a point beyond the grid, which is the grid's cellTolerance in
/// node indexes. A node index moves c0 by affineCoeffs[1] along i and by affineCoeffs[2] along j (and c1 by [4] and
/// [5]), so the widening along c0 is cellTolerance * (|affineCoeffs[1]| + |affineCoeffs[2]|), and likewise along c1.
/// In exact arithmetic every point that locate() accepts is therefore inside the rectangle, for any affine transform,
/// and the rectangle is the smallest one with that property. A point within rounding error of the edge could fall on
/// either side of it, so the grid might not be tried for a point that is a rounding error beyond its tolerance, and a
/// later grid in file order would answer instead. That only changes whether such a point counts as inside, which the
/// tolerance already leaves to judgement. A grid whose edges are along c0 and c1 fills its rectangle, and any other
/// grid fills only part of it.
/// \return the rectangle, with the id given.
IndexedRectangle boundingRectangleOf(const GridGeometry& geometry,  ///< the grid's geometry
                                     const std::size_t id) {        ///< the id for the rectangle
    const std::array<double, 6>& affineCoeffs = geometry.affineCoeffs();
    const double lastI = static_cast<double>(geometry.iNodeCount() - 1);
    const double lastJ = static_cast<double>(geometry.jNodeCount() - 1);

    // c0 and c1 of the corner nodes (0, 0), (lastI, 0), (0, lastJ) and (lastI, lastJ).
    const std::array<double, 4> corners0 = {affineCoeffs[0], affineCoeffs[0] + affineCoeffs[1] * lastI,
                                            affineCoeffs[0] + affineCoeffs[2] * lastJ,
                                            affineCoeffs[0] + affineCoeffs[1] * lastI + affineCoeffs[2] * lastJ};
    const std::array<double, 4> corners1 = {affineCoeffs[3], affineCoeffs[3] + affineCoeffs[4] * lastI,
                                            affineCoeffs[3] + affineCoeffs[5] * lastJ,
                                            affineCoeffs[3] + affineCoeffs[4] * lastI + affineCoeffs[5] * lastJ};
    const auto [lowest0, highest0] = std::minmax_element(corners0.begin(), corners0.end());
    const auto [lowest1, highest1] = std::minmax_element(corners1.begin(), corners1.end());

    const double tolerance = geometry.cellTolerance();
    return IndexedRectangle(*lowest0, *highest0, *lowest1, *highest1, id,
                            tolerance * (std::abs(affineCoeffs[1]) + std::abs(affineCoeffs[2])),
                            tolerance * (std::abs(affineCoeffs[4]) + std::abs(affineCoeffs[5])));
}

}  // namespace

RaggedLayer::RaggedLayer(double referenceDateDays) : referenceDateDays_(referenceDateDays) {}

void RaggedLayer::addRootGrids(std::vector<std::unique_ptr<Grid<RaggedGridData>>> grids) {
    if (!rootGrids_.empty()) {
        throw std::logic_error("RaggedLayer::addRootGrids: this layer already has root grids");
    }
    if (grids.empty()) {
        return;
    }

    // Everything is checked and built before anything in this layer changes, so a grid that is rejected leaves the
    // layer as it was.
    std::vector<IndexedRectangle> rectangles;
    rectangles.reserve(grids.size());
    for (std::size_t fileIndex = 0; fileIndex < grids.size(); ++fileIndex) {
        const auto& grid = grids[fileIndex];
        if (!grid) {
            throw std::invalid_argument("RaggedLayer::addRootGrids: a grid must not be null");
        }
        if (grid->axisCount() != grids.front()->axisCount()) {
            throw std::invalid_argument("RaggedLayer::addRootGrids: grid axisCount (" +
                                        std::to_string(grid->axisCount()) + ") does not match the first grid's own (" +
                                        std::to_string(grids.front()->axisCount()) + ")");
        }
        rectangles.push_back(boundingRectangleOf(grid->geometry(), fileIndex));
    }
    rootIndex_.emplace(std::move(rectangles));
    rootGrids_ = std::move(grids);
}

std::optional<std::size_t> RaggedLayer::axisCount() const {
    if (rootGrids_.empty()) {
        return std::nullopt;
    }
    return rootGrids_.front()->axisCount();
}

std::optional<Resolution<RaggedGridData>> RaggedLayer::resolveRootGrid(double c0, double c1) const {
    if (!rootIndex_) {
        return std::nullopt;
    }
    const std::optional<std::size_t> fileIndex = rootIndex_->firstContaining(
        c0, c1, [this, c0, c1](const std::size_t id) { return rootGrids_[id]->locate(c0, c1).has_value(); });
    if (!fileIndex) {
        return std::nullopt;
    }
    // The cell is found again for the grid that was chosen, because the search tests grids that it does not choose.
    const std::optional<Cell> cell = rootGrids_[*fileIndex]->locate(c0, c1);
    if (!cell) {
        return std::nullopt;
    }
    return Resolution<RaggedGridData>{*rootGrids_[*fileIndex], *cell};
}

std::optional<std::array<double, 3>> RaggedLayer::evaluate(double c0, double c1, double queryDateDays) const {
    auto resolution = resolveRootGrid(c0, c1);
    if (!resolution) {
        return std::nullopt;
    }
    resolution = descendToLeaf(*resolution, c0, c1);

    const double queryDay = queryDateDays - referenceDateDays_;
    return resolution->grid.get().data().evaluate(resolution->cell, queryDay);
}

}  // namespace deformation
