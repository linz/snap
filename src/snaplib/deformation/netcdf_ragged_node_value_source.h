#pragma once

#include <cstdint>
#include <vector>
#include <cstddef>
#include "ragged_grid_data.h"

namespace deformation {

// The netCDF-backed RaggedNodeValueSource: every node's own per-axis
// segment chain is built into memory once, at construction, rather than
// reconstructed per query - real query volumes make per-query netCDF calls
// too slow regardless.
//
// Stores every axis's own segments in one flat, pre-sized buffer per axis
// (segmentsByAxis_[axis]), node by node back to back, with a parallel CSR
// offset array (segmentOffsetsByAxis_[axis]) marking where each node's own
// slice starts - the same shape the file itself already stores this data
// in (segment_count_{e,n,u} + flat segments_{e,n,u}). segments() returns a
// RaggedSegmentRange pointing directly into the relevant slice, rather
// than each node owning a separate std::vector<RaggedSegment>: a real
// InSAR file needs on the order of a million small allocations, one per
// node per axis with at least one real segment, and a separate
// std::vector<RaggedSegment> per node at that volume causes real
// heap-fragmentation. One pre-sized allocation per axis per grid, instead
// of one per node, avoids that - measured to meaningfully cut live memory
// and allocation count, though most of a real file's own total peak memory
// comes from elsewhere entirely (netCDF-C/HDF5's own per-group overhead
// opening/reading attributes across every root grid, not from anything
// this class stores), so this alone does not fix a real file's overall
// peak memory.
class NetcdfRaggedNodeValueSource : public RaggedNodeValueSource {
 public:
    // segmentOffsetsByAxis[axis] is this grid's own per-axis CSR offsets
    // (length nodeCount + 1, matching the file's own segment_count_-derived
    // offsets exactly - see parseRaggedGridContent()): node (i, j)'s own
    // segments occupy segmentsByAxis[axis][segmentOffsetsByAxis[axis][node]
    // .. segmentOffsetsByAxis[axis][node + 1]), where node is (i, j)'s own
    // flat index (see nodeIndex() below). segmentsByAxis[axis] holds every
    // node's own segments for that axis, in node order, back to back -
    // already fully resolved (RaggedSegment::startDay already combines the
    // file's own reference_day/day_delta, per RaggedSegment's own
    // documentation). hasEndDayPerNode is this grid's own per-node
    // has_end_day flags, flattened the same way (i * jNodeCount + j); an
    // empty vector means the file had no has_end_day variable at all, i.e.
    // every node is false.
    NetcdfRaggedNodeValueSource(std::uint32_t jNodeCount, std::size_t axisCount, std::vector<bool> hasEndDayPerNode,
                                std::vector<std::vector<std::size_t>> segmentOffsetsByAxis,
                                std::vector<std::vector<RaggedSegment>> segmentsByAxis);

    [[nodiscard]] std::size_t axisCount() const override;
    [[nodiscard]] bool hasEndDay(std::uint32_t i, std::uint32_t j) const override;
    [[nodiscard]] RaggedSegmentRange segments(std::uint32_t i, std::uint32_t j, std::size_t axis) const override;

 private:
    // Node (i, j)'s own flat offset into hasEndDayPerNode_/each axis's own
    // entry in segmentOffsetsByAxis_ - matching the file's own node_index
    // convention (row-major over (i, j), not stored as a 2D array).
    [[nodiscard]] std::size_t nodeIndex(std::uint32_t i, std::uint32_t j) const;

    std::uint32_t jNodeCount_;
    std::size_t axisCount_;
    std::vector<bool> hasEndDayPerNode_;  // empty means "no node has an end day"
    std::vector<std::vector<std::size_t>>
        segmentOffsetsByAxis_;  // [axis][node], plus [axis][nodeCount] as the end sentinel
    std::vector<std::vector<RaggedSegment>> segmentsByAxis_;  // [axis] -> every node's own segments, node order, flat
};

}  // namespace deformation
