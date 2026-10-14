#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "scalar_grid_data.h"

namespace deformation {

// The netCDF-backed ScalarNodeValueSource: reads one scalar_with_time_function
// grid's own raw `displacement` array into memory once, at construction,
// rather than re-reading the file per query - real query volumes make
// per-query netCDF calls too slow, and a grid's own raw values never change
// after the file is opened.
class NetcdfScalarNodeValueSource : public ScalarNodeValueSource {
 public:
    // displacement is this grid's own raw `displacement` array, already
    // read out of the file in full, row-major over (i, j, axis) - node
    // (i, j)'s own raw value for file axis a is displacement[(i *
    // jNodeCount + j) * rawAxisCount + a], matching the file's own storage
    // order exactly so no reshaping is needed here. canonicalAxisIndices
    // maps each of those raw file axes to its real displacement component -
    // canonicalAxisIndices[a] is 0 (east), 1 (north), or 2 (up), i.e. raw
    // file axis a's own value belongs at result index canonicalAxisIndices[a]
    // in nodeValues()'s own returned array - since a layer's own
    // `gridParameters` can list fewer than 3 parameters, or a different
    // order, than raw file-axis position alone would suggest. A real
    // single-parameter "up only" layer gives canonicalAxisIndices = {2}
    // (raw axis 0's one value must land at canonical index 2, not 0); a
    // hypothetical gridParameters = ["displacementUp", "displacementEast"]
    // would give {2, 0} (raw axis 0 at canonical index 2, raw axis 1 at
    // canonical index 0). Must be the same length as displacement's own
    // raw axis count (rawAxisCount,
    // implied by canonicalAxisIndices.size()). missingValue, where present,
    // is the raw stored value that marks a node's own value as undefined
    // for every axis alike (this format declares missing_value once per
    // grid, not per axis). hasEndDay is this grid's own grid-wide
    // has_end_day flag, already resolved to a bool by the caller (absent in
    // the file means false) - see ScalarGridData's own has_end_day
    // handling for why a true value here makes ScalarGridData's
    // constructor throw.
    NetcdfScalarNodeValueSource(std::uint32_t jNodeCount, std::vector<std::size_t> canonicalAxisIndices,
                                std::vector<std::int32_t> displacement, std::optional<std::int32_t> missingValue,
                                bool hasEndDay);

    // Always 3 - every canonical component (east/north/up) this source
    // doesn't carry a raw file axis for simply reads as 0.0 (a real,
    // meaningful "this layer contributes nothing on that axis", not
    // "unknown") - see canonicalAxisIndices above.
    [[nodiscard]] std::size_t axisCount() const override;
    [[nodiscard]] std::optional<std::array<double, 3>> nodeValues(std::uint32_t i, std::uint32_t j) const override;
    [[nodiscard]] bool hasEndDay() const override;

 private:
    std::uint32_t jNodeCount_;                       // needed to convert (i, j) into displacement_'s own flat offset
    std::vector<std::size_t> canonicalAxisIndices_;  // raw file axis -> canonical component index (0/1/2)
    std::vector<std::int32_t> displacement_;         // raw, unscaled - row-major over (i, j, raw file axis)
    std::optional<std::int32_t> missingValue_;
    bool hasEndDay_;
};

}  // namespace deformation
