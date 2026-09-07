#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "gridgeometry.h"

namespace deformation {

// One linear-shaped segment of a single node's own time series (GGRXF's
// only implemented ragged segment shape - see RaggedNodeValueSource): from
// startDay onward, this node's own value is startValue + ratePerDay *
// (queryDay - startDay), until superseded by whichever later segment (if
// any) in the same chain has the next-highest startDay. startDay is already
// the file's own reference_day + day_delta, combined by whichever front end
// builds this from the file - the core never needs the two separately.
struct RaggedSegment
{
    std::int64_t startDay;
    double startValue;
    double ratePerDay;
};

// Supplies the raw per-node segment data for one ragged_segments grid's own
// node storage - the ragged sibling to ScalarNodeValueSource. One instance
// is bound to a single grid's own node storage.
class RaggedNodeValueSource
{
public:
    virtual ~RaggedNodeValueSource() = default;

    // The number of displacement axes stored at each node - see
    // ScalarNodeValueSource::axisCount().
    virtual std::size_t axisCount() const = 0;

    // True if node (i, j) has a real own end day (GGRXF's own_end_day/
    // has_end_day terminal-node supersession, shared across every axis at
    // that node) - not implemented; RaggedGridData::evaluate() throws
    // std::runtime_error if this is ever true, matching the real Python
    // reference reader's own NotImplementedError-at-query-time behaviour,
    // rather than silently ignoring it.
    virtual bool hasEndDay( std::uint32_t i, std::uint32_t j ) const = 0;

    // Node (i, j)'s own segment chain for displacement axis `axis` (0 <=
    // axis < axisCount()), in chronological startDay order. Empty if this
    // node has no active segment there at all - a genuine gap (e.g. the
    // edge of a quadtree-tiled InSAR patch), not an error; see
    // RaggedGridData::evaluate() for how that resolves.
    virtual const std::vector<RaggedSegment> &segments(
        std::uint32_t i, std::uint32_t j, std::size_t axis ) const = 0;
};

// One ragged_segments grid's own per-node value computation - the sibling
// to ScalarGridData for the other GGRXF grid type. Shares GridGeometry
// (grid resolution - locate()/containsExtentOf() - is identical for both
// grid types, see GridGeometry's own class comment) but computes a node's
// own value in the opposite order to ScalarGridData::interpolate(): each of
// the 4 corners is evaluated at the query date independently first, then
// the 4 already-time-evaluated results are bilinearly interpolated, since
// each node here carries its own independent time series rather than
// sharing one grid-wide time function.
class RaggedGridData
{
public:
    // Throws std::invalid_argument if values is null.
    explicit RaggedGridData( std::unique_ptr<RaggedNodeValueSource> values );

    // 4-corner-independent-time-evaluation then bilinear interpolation over
    // `cell` (from a prior GridGeometry::locate() call), at `queryDayCount`
    // (a day count in the same epoch as every RaggedSegment's own
    // startDay). Unlike ScalarGridData::interpolate(), this never returns
    // std::nullopt - a node with no active segment at all contributes a
    // real 0.0, not a missing value (see RaggedNodeValueSource::segments()).
    // Throws std::runtime_error if any of the 4 corner nodes has a real
    // hasEndDay() - see RaggedNodeValueSource::hasEndDay().
    std::array<double, 3> evaluate( const Cell &cell, std::int64_t queryDayCount ) const;

    // The number of displacement axes this grid's own RaggedNodeValueSource
    // returns per node (and hence per evaluate() call).
    std::size_t axisCount() const;

private:
    // One corner node's own value at queryDayCount, for one axis: the last
    // segment whose own startDay is at or before queryDayCount (0.0 if none
    // is, including an empty segment chain).
    double evaluateNode( std::uint32_t i, std::uint32_t j, std::size_t axis, std::int64_t queryDayCount ) const;

    std::unique_ptr<RaggedNodeValueSource> values_;
};

} // namespace deformation
