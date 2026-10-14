#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

#include "grid_geometry.h"

namespace deformation {

// Supplies the raw, unscaled values stored at one ScalarGridData's nodes. One
// instance is bound to a single ScalarGridData's own node storage - a
// netCDF-backed implementation feeds `ScalarGridData`, without `ScalarGridData`'s own
// math ever touching netCDF; a future LINZDEF-binary-backed one could read
// LINZDEF's own row-cached grid data, without ScalarGridData's own indexing/
// interpolation code needing to know which.
class ScalarNodeValueSource {
 public:
    virtual ~ScalarNodeValueSource() = default;

    // The number of displacement axes stored at each node (GGRXF's
    // `displacementCount`, LINZDEF's `ngrdval`) - at most 3, the real bound
    // both formats share. Only the first axisCount() entries nodeValues()
    // returns are meaningful; the rest are unspecified.
    [[nodiscard]] virtual std::size_t axisCount() const = 0;

    // Returns the raw, unscaled values stored at node (i, j) (only the
    // first axisCount() entries are meaningful), or std::nullopt if that
    // node is marked as missing/undefined data.
    [[nodiscard]] virtual std::optional<std::array<double, 3>> nodeValues(
        std::uint32_t i,  // node index along the grid's first axis, 0 <= i < iNodeCount
        std::uint32_t j   // node index along the grid's second axis, 0 <= j < jNodeCount
    ) const = 0;

    // True if this whole grid declares GGRXF's own grid-wide has_end_day -
    // a single flag for the whole grid, unlike
    // RaggedNodeValueSource::hasEndDay()'s per-node one. Signals that this
    // grid's own contribution should stop being extrapolated, and hold
    // fixed, from own_end_date onward (early termination or temporal
    // supersession - see the GGRXF spec's own "Terminal-node evaluation").
    // That behaviour itself is not implemented here; ScalarGridData's
    // constructor throws std::runtime_error if this is ever true. No real
    // file this project's own deformation model is built from sets it -
    // the equivalent effect (an existing ramp component cut off where a
    // newer dataset takes over) is achieved instead by rescaling and
    // truncating that ramp's own end_date directly.
    [[nodiscard]] virtual bool hasEndDay() const = 0;
};

// One scalar_with_time_function grid's own per-node value computation - the
// sibling to RaggedGridData for the other GGRXF grid type. Has no notion of
// its own geometry or hierarchy (that's GridGeometry's and Grid's job,
// respectively) - ScalarGridData only ever answers "what's the value here",
// given a Cell already resolved against this grid's own geometry.
class ScalarGridData {
 public:
    // Throws std::invalid_argument if values is null.
    ScalarGridData(
        double
            scaleFactor,  // real-valued unit one raw stored value represents; applied once, after the weighted sum in interpolate() (matching dbl4_utl_grid.cpp's calc_grid_linear: sum raw values weighted, scale once)
        std::unique_ptr<ScalarNodeValueSource> values);  // this grid's own raw node data; must not be null

    // 4-corner bilinear interpolation over `cell` (from a prior
    // GridGeometry::locate() call), scaled by scaleFactor - only the first
    // axisCount() entries of the result are meaningful; entries beyond that
    // are always exactly 0.0. Returns std::nullopt if any of the 4 corner
    // nodes is missing data.
    [[nodiscard]] std::optional<std::array<double, 3>> interpolate(const Cell& cell) const;

    // The number of displacement axes this grid's own ScalarNodeValueSource
    // returns per node (and hence per interpolate() call).
    [[nodiscard]] std::size_t axisCount() const;

 private:
    double scaleFactor_;                             // applied once, after interpolate()'s weighted sum
    std::unique_ptr<ScalarNodeValueSource> values_;  // this grid's own raw node data
};

}  // namespace deformation
