// Standalone unit test for NetcdfRaggedNodeValueSource (deformation/netcdf_ragged_node_value_source.h), which serves
// the segments of every node of a grid, read from a file, to a RaggedGridData: finding the segments of node (i, j) on
// an axis in the flat arrays that the file stores them in, the nodes that have no segment, and the per-node end day
// flags. The class holds vectors that the reader has already read, so no netCDF file is involved.
//
// The grids have 2 by 3 nodes, and two axes. Node (i, j) is the node at the flat index 3 * i + j, and the counts of
// segments of the nodes in flat order are 2, 0, 1, 3, 0, 1 on the first axis and 1, 1, 0, 0, 2, 1 on the second.

#include <boost/numeric/conversion/cast.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "deformation/netcdf_ragged_node_value_source.h"
#include "deformation/ragged_grid_data.h"
#include "test_support.h"

namespace {

using boost::numeric_cast;
using deformation::NetcdfRaggedNodeValueSource;
using deformation::RaggedSegment;
using deformation::RaggedSegmentRange;
using snap_test::check;
using snap_test::checkEqual;

constexpr std::uint32_t kJNodeCount = 3;

/// The k-th segment of the node with the flat index node, on the axis. Its numbers show which segment it is. The
/// numbers are 32 bit, which a double holds exactly, so they convert to double without a cast.
RaggedSegment makeSegment(const std::uint32_t axis, const std::uint32_t node, const std::uint32_t k) {
    const double day = 1000.0 * axis + 10.0 * node + k;
    return RaggedSegment{day, -day, 0.5 * k};
}

/// Adds the segments of one axis, given how many segments each node has, in flat order, to the CSR offsets and the flat
/// array of segments that the file stores them in.
void addAxis(const std::uint32_t axis,
             const std::vector<std::uint32_t>& countOfEachNode,
             std::vector<std::vector<std::size_t>>& offsetsByAxis,
             std::vector<std::vector<RaggedSegment>>& segmentsByAxis) {
    std::vector<std::size_t> offsets = {0};
    std::vector<RaggedSegment> segments;
    for (std::uint32_t node = 0; node < countOfEachNode.size(); ++node) {
        for (std::uint32_t k = 0; k < countOfEachNode[node]; ++k) {
            segments.push_back(makeSegment(axis, node, k));
        }
        offsets.push_back(segments.size());
    }
    offsetsByAxis.push_back(offsets);
    segmentsByAxis.push_back(segments);
}

/// \return a source of the 2 by 3 grid, whose nodes have the flags that are given, in flat order, or none for a file
/// that has no flags.
NetcdfRaggedNodeValueSource makeSource(const std::vector<bool>& hasEndDayPerNode = {}) {
    std::vector<std::vector<std::size_t>> offsetsByAxis;
    std::vector<std::vector<RaggedSegment>> segmentsByAxis;
    addAxis(0, {2, 0, 1, 3, 0, 1}, offsetsByAxis, segmentsByAxis);
    addAxis(1, {1, 1, 0, 0, 2, 1}, offsetsByAxis, segmentsByAxis);
    return NetcdfRaggedNodeValueSource(kJNodeCount, 2, hasEndDayPerNode, offsetsByAxis, segmentsByAxis);
}

/// \return how many segments the range has.
std::size_t countOf(const RaggedSegmentRange& range) {
    return numeric_cast<std::size_t>(range.end() - range.begin());
}

/// Checks that the segment is the expected one.
void checkSegment(const RaggedSegment& actual, const RaggedSegment& expected, const std::string& messageOnError) {
    checkEqual(actual.startDay, expected.startDay, 0.0, messageOnError + ": the start day");
    checkEqual(actual.startValue, expected.startValue, 0.0, messageOnError + ": the start value");
    checkEqual(actual.ratePerDay, expected.ratePerDay, 0.0, messageOnError + ": the rate");
}

/// The name of a node and an axis, for a failure message.
std::string nameOf(const std::uint32_t i, const std::uint32_t j, const std::uint32_t axis) {
    return "node (" + std::to_string(i) + ", " + std::to_string(j) + ") on axis " + std::to_string(axis);
}

/// Checks that the node has the expected number of segments, and that they are the ones that makeSegment() gives for
/// the flat index of the node.
void checkSegmentsOfNode(const NetcdfRaggedNodeValueSource& source,
                         const std::uint32_t i,
                         const std::uint32_t j,
                         const std::uint32_t axis,
                         const std::uint32_t flatIndex,
                         const std::size_t expectedCount) {
    const RaggedSegmentRange range = source.segments(i, j, axis);
    checkEqual(countOf(range), expectedCount, "the number of segments of " + nameOf(i, j, axis));
    std::size_t k = 0;
    for (const RaggedSegment* segment = range.begin(); segment != range.end(); ++segment, ++k) {
        checkSegment(*segment, makeSegment(axis, flatIndex, k),
                     "segment " + std::to_string(k) + " of " + nameOf(i, j, axis));
    }
}

void check_axis_count() {
    checkEqual(makeSource().axisCount(), std::size_t{2}, "the axis count of the source");
}

void check_every_node_has_its_own_number_of_segments() {
    // The number of segments of node (i, j) on an axis, written out by node and not by flat index.
    struct Count {
        std::uint32_t i;
        std::uint32_t j;
        std::uint32_t axis;
        std::size_t count;
    };
    const std::vector<Count> counts = {{0, 0, 0, 2}, {0, 1, 0, 0}, {0, 2, 0, 1}, {1, 0, 0, 3},
                                       {1, 1, 0, 0}, {1, 2, 0, 1}, {0, 0, 1, 1}, {0, 1, 1, 1},
                                       {0, 2, 1, 0}, {1, 0, 1, 0}, {1, 1, 1, 2}, {1, 2, 1, 1}};
    const NetcdfRaggedNodeValueSource source = makeSource();
    for (const Count& entry : counts) {
        checkEqual(countOf(source.segments(entry.i, entry.j, entry.axis)), entry.count,
                   "the number of segments of " + nameOf(entry.i, entry.j, entry.axis));
    }
}

void check_segments_are_those_of_the_node() {
    const NetcdfRaggedNodeValueSource source = makeSource();
    checkSegmentsOfNode(source, 0, 0, 0, 0, 2);
    checkSegmentsOfNode(source, 1, 0, 0, 3, 3);
    checkSegmentsOfNode(source, 1, 2, 0, 5, 1);
    checkSegmentsOfNode(source, 0, 1, 1, 1, 1);
    checkSegmentsOfNode(source, 1, 1, 1, 4, 2);
    checkSegmentsOfNode(source, 1, 2, 1, 5, 1);
}

void check_a_node_with_no_segment_has_an_empty_range() {
    const NetcdfRaggedNodeValueSource source = makeSource();
    const RaggedSegmentRange none = source.segments(0, 1, 0);
    check(none.begin() == none.end(), "node (0, 1) has no segment on the first axis");
    const RaggedSegmentRange alsoNone = source.segments(1, 0, 1);
    check(alsoNone.begin() == alsoNone.end(), "node (1, 0) has no segment on the second axis");
}

void check_end_day_flags() {
    const NetcdfRaggedNodeValueSource without = makeSource();
    for (std::uint32_t i = 0; i < 2; ++i) {
        for (std::uint32_t j = 0; j < kJNodeCount; ++j) {
            check(!without.hasEndDay(i, j),
                  "no end day for node (" + std::to_string(i) + ", " + std::to_string(j) + "), the file has no flags");
        }
    }

    // The flags in flat order: nodes (0, 1) and (1, 2) have an end day.
    const NetcdfRaggedNodeValueSource with = makeSource({false, true, false, false, false, true});
    check(!with.hasEndDay(0, 0), "no end day for node (0, 0)");
    check(with.hasEndDay(0, 1), "an end day for node (0, 1)");
    check(!with.hasEndDay(0, 2), "no end day for node (0, 2)");
    check(!with.hasEndDay(1, 0), "no end day for node (1, 0)");
    check(!with.hasEndDay(1, 1), "no end day for node (1, 1)");
    check(with.hasEndDay(1, 2), "an end day for node (1, 2)");
}

}  // namespace

int main() {
    check_axis_count();
    check_every_node_has_its_own_number_of_segments();
    check_segments_are_those_of_the_node();
    check_a_node_with_no_segment_has_an_empty_range();
    check_end_day_flags();

    return snap_test::finish("netcdf_ragged_node_value_source_test");
}
