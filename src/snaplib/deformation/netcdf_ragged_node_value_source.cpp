#include "deformation/netcdf_ragged_node_value_source.h"
#include <utility>
#include "deformation/ragged_grid_data.h"

namespace deformation {

NetcdfRaggedNodeValueSource::NetcdfRaggedNodeValueSource(std::uint32_t jNodeCount, std::size_t axisCount,
                                                         std::vector<bool> hasEndDayPerNode,
                                                         std::vector<std::vector<std::size_t>> segmentOffsetsByAxis,
                                                         std::vector<std::vector<RaggedSegment>> segmentsByAxis)
    : jNodeCount_(jNodeCount),
      axisCount_(axisCount),
      hasEndDayPerNode_(std::move(hasEndDayPerNode)),
      segmentOffsetsByAxis_(std::move(segmentOffsetsByAxis)),
      segmentsByAxis_(std::move(segmentsByAxis)) {}

std::size_t NetcdfRaggedNodeValueSource::axisCount() const {
    return axisCount_;
}

bool NetcdfRaggedNodeValueSource::hasEndDay(std::uint32_t i, std::uint32_t j) const {
    if (hasEndDayPerNode_.empty()) {
        return false;
    }
    return hasEndDayPerNode_[nodeIndex(i, j)];
}

RaggedSegmentRange NetcdfRaggedNodeValueSource::segments(std::uint32_t i, std::uint32_t j, std::size_t axis) const {
    const std::size_t node = nodeIndex(i, j);
    const auto& offsets = segmentOffsetsByAxis_[axis];
    const RaggedSegment* base = segmentsByAxis_[axis].data();
    return RaggedSegmentRange(base + offsets[node], base + offsets[node + 1]);
}

std::size_t NetcdfRaggedNodeValueSource::nodeIndex(std::uint32_t i, std::uint32_t j) const {
    return static_cast<std::size_t>(i) * jNodeCount_ + j;
}

}  // namespace deformation
