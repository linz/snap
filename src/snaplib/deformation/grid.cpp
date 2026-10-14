#include "deformation/grid.h"

#include <stdexcept>
#include <string>

#include "deformation/scalar_grid_data.h"
#include "deformation/ragged_grid_data.h"

namespace deformation {

template <typename NodeData>
Grid<NodeData>::Grid(const GridGeometry& geometry, NodeData data) : geometry_(geometry), data_(std::move(data)) {}

template <typename NodeData>
const GridGeometry& Grid<NodeData>::geometry() const {
    return geometry_;
}

template <typename NodeData>
const NodeData& Grid<NodeData>::data() const {
    return data_;
}

template <typename NodeData>
std::optional<Cell> Grid<NodeData>::locate(double c0, double c1) const {
    return geometry_.locate(c0, c1);
}

template <typename NodeData>
std::size_t Grid<NodeData>::axisCount() const {
    return data_.axisCount();
}

template <typename NodeData>
GridParent<NodeData>::GridParent(const GridGeometry& geometry, NodeData data)
    : Grid<NodeData>(geometry, std::move(data)) {}

template <typename NodeData>
void GridParent<NodeData>::addChild(std::unique_ptr<Grid<NodeData>> child) {
    if (!child) {
        throw std::invalid_argument("GridParent::addChild: child must not be null");
    }
    if (child->axisCount() != this->axisCount()) {
        throw std::invalid_argument("GridParent::addChild: child axisCount (" + std::to_string(child->axisCount()) +
                                    ") does not match parent's own (" + std::to_string(this->axisCount()) + ")");
    }
    if (!this->geometry().containsExtentOf(child->geometry())) {
        throw std::invalid_argument("GridParent::addChild: child extent is not contained within parent's own extent");
    }
    children_.push_back(std::move(child));
}

template <typename NodeData>
const std::vector<std::unique_ptr<Grid<NodeData>>>& GridParent<NodeData>::children() const {
    return children_;
}

template <typename NodeData>
std::optional<Resolution<NodeData>> GridParent<NodeData>::resolveChild(double c0, double c1) const {
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        const auto& child = *it;
        if (const auto cell = child->locate(c0, c1)) {
            return Resolution<NodeData>{*child, *cell};
        }
    }
    return std::nullopt;
}

// Explicit instantiation - Grid/GridParent are only ever used with these two
// node-data types (ScalarGridData for scalar_with_time_function,
// RaggedGridData for ragged_segments), so the definitions above live here
// rather than in the header, matching this module's own declaration/
// definition split.
template class Grid<ScalarGridData>;
template class Grid<RaggedGridData>;
template class GridParent<ScalarGridData>;
template class GridParent<RaggedGridData>;

}  // namespace deformation
