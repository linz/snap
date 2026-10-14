#include "deformation/netcdf_scalar_node_value_source.h"
#include <utility>
#include <array>

namespace deformation {

NetcdfScalarNodeValueSource::NetcdfScalarNodeValueSource(std::uint32_t jNodeCount,
                                                         std::vector<std::size_t> canonicalAxisIndices,
                                                         std::vector<std::int32_t> displacement,
                                                         std::optional<std::int32_t> missingValue, bool hasEndDay)
    : jNodeCount_(jNodeCount),
      canonicalAxisIndices_(std::move(canonicalAxisIndices)),
      displacement_(std::move(displacement)),
      missingValue_(missingValue),
      hasEndDay_(hasEndDay) {}

std::size_t NetcdfScalarNodeValueSource::axisCount() const {
    return 3;
}

// Returns std::nullopt as soon as any raw file axis's own value equals
// missingValue_ - this format marks a node undefined for every axis at
// once, so there is nothing to salvage from the other, still-defined axes
// at that same node.
std::optional<std::array<double, 3>> NetcdfScalarNodeValueSource::nodeValues(std::uint32_t i, std::uint32_t j) const {
    const std::size_t rawAxisCount = canonicalAxisIndices_.size();
    const std::size_t base = (static_cast<std::size_t>(i) * jNodeCount_ + j) * rawAxisCount;
    std::array<double, 3> result{};  // value-initialized: every canonical component this source doesn't carry stays 0.0
    for (std::size_t rawAxis = 0; rawAxis < rawAxisCount; ++rawAxis) {
        const std::int32_t raw = displacement_[base + rawAxis];
        if (missingValue_ && raw == *missingValue_) {
            return std::nullopt;
        }
        result[canonicalAxisIndices_[rawAxis]] = static_cast<double>(raw);
    }
    return result;
}

bool NetcdfScalarNodeValueSource::hasEndDay() const {
    return hasEndDay_;
}

}  // namespace deformation
