// Standalone unit test for NetcdfScalarNodeValueSource (deformation/netcdf_scalar_node_value_source.h), which serves
// the raw displacement values of a grid, read from a file, as the nodes of a ScalarGridData: the position of a node's
// values in the flat array, the canonical axis (east, north or up) that each raw axis belongs to, the nodes that have
// the missing value, and the exact conversion of the raw integers to doubles. The class holds vectors that the reader
// has already read, so no netCDF file is involved.
//
// The grids have 2 by 3 nodes. The raw value of node (i, j) on raw axis a is 1000 * (i + 1) + 100 * (j + 1) + (a + 1),
// so that it shows which node and which axis it is from, and it is never 0, which is what an axis that the grid does
// not have reads as.

#include <boost/numeric/conversion/cast.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "deformation/netcdf_scalar_node_value_source.h"
#include "indexed_scalar_values.h"
#include "test_support.h"

namespace {

using boost::numeric_cast;
using deformation::NetcdfScalarNodeValueSource;
using snap_test::check;
using snap_test::checkEqual;
using snap_test::checkValues;

constexpr std::uint32_t kINodeCount = 2;
constexpr std::uint32_t kJNodeCount = 3;

/// The raw value that node (i, j) has on raw axis a.
std::int32_t rawValue(const std::uint32_t i, const std::uint32_t j, const std::size_t a) {
    return numeric_cast<std::int32_t>(1000 * (i + 1) + 100 * (j + 1) + (a + 1));
}

/// \return the raw displacement of the grid, row-major over (i, j, raw axis), as the file stores it.
std::vector<std::int32_t> makeDisplacement(const std::size_t rawAxisCount) {
    std::vector<std::int32_t> displacement;
    for (std::uint32_t i = 0; i < kINodeCount; ++i) {
        for (std::uint32_t j = 0; j < kJNodeCount; ++j) {
            for (std::size_t a = 0; a < rawAxisCount; ++a) {
                displacement.push_back(rawValue(i, j, a));
            }
        }
    }
    return displacement;
}

/// \return a source of the displacement, whose raw axes belong to the canonical axes in the list.
NetcdfScalarNodeValueSource makeSource(const std::vector<std::size_t>& canonicalAxisIndices,
                                       const std::optional<std::int32_t> missingValue = std::nullopt,
                                       const bool hasEndDay = false) {
    return NetcdfScalarNodeValueSource(kJNodeCount, canonicalAxisIndices, makeDisplacement(canonicalAxisIndices.size()),
                                       missingValue, hasEndDay);
}

void check_axis_count_is_always_three() {
    checkEqual(makeSource({0, 1, 2}).axisCount(), std::size_t{3}, "a source of all three axes");
    checkEqual(makeSource({0, 1}).axisCount(), std::size_t{3}, "a source of two axes");
    checkEqual(makeSource({2}).axisCount(), std::size_t{3}, "a source of one axis");
}

void check_nodes_are_found_in_the_flat_array() {
    const NetcdfScalarNodeValueSource source = makeSource({0, 1});
    checkValues(source.nodeValues(0, 0), {1101.0, 1102.0, 0.0}, "node (0, 0)");
    checkValues(source.nodeValues(0, 2), {1301.0, 1302.0, 0.0}, "the last node of the first row");
    checkValues(source.nodeValues(1, 0), {2101.0, 2102.0, 0.0}, "the first node of the second row");
    checkValues(source.nodeValues(1, 1), {2201.0, 2202.0, 0.0}, "a node in the middle of the second row");
    checkValues(source.nodeValues(1, 2), {2301.0, 2302.0, 0.0}, "the last node");
}

void check_raw_axes_go_to_their_canonical_axes() {
    checkValues(makeSource({0, 1, 2}).nodeValues(0, 1), {1201.0, 1202.0, 1203.0}, "east, north and up in file order");
    checkValues(makeSource({2}).nodeValues(1, 1), {0.0, 0.0, 2201.0}, "a layer of up only");
    // The first raw axis is up, and the second is east.
    checkValues(makeSource({2, 0}).nodeValues(1, 1), {2202.0, 0.0, 2201.0}, "up, then east");
}

void check_missing_value() {
    constexpr std::int32_t kMissing = std::numeric_limits<std::int32_t>::max();

    // The second raw axis of node (1, 1), and the first of node (0, 2), have the missing value.
    std::vector<std::int32_t> displacement = makeDisplacement(2);
    displacement[(1 * kJNodeCount + 1) * 2 + 1] = kMissing;
    displacement[(0 * kJNodeCount + 2) * 2 + 0] = kMissing;

    const NetcdfScalarNodeValueSource declared(kJNodeCount, {0, 1}, displacement, kMissing, false);
    check(!declared.nodeValues(1, 1).has_value(), "no values for a node that has it on its second axis only");
    check(!declared.nodeValues(0, 2).has_value(), "no values for a node that has it on its first axis only");
    checkValues(declared.nodeValues(0, 0), {1101.0, 1102.0, 0.0}, "a node before them");
    checkValues(declared.nodeValues(1, 2), {2301.0, 2302.0, 0.0}, "a node after them");

    // With no missing value declared, the same number is an ordinary value.
    const NetcdfScalarNodeValueSource notDeclared(kJNodeCount, {0, 1}, displacement, std::nullopt, false);
    checkValues(notDeclared.nodeValues(1, 1), {2201.0, kMissing, 0.0},
                "a node that has the number when there is no missing value");
}

void check_raw_integers_are_exact() {
    constexpr std::int32_t kMaximum = std::numeric_limits<std::int32_t>::max();
    constexpr std::int32_t kMinimum = std::numeric_limits<std::int32_t>::min();
    const NetcdfScalarNodeValueSource source(4, {0}, {kMaximum, kMinimum, -5, 0}, std::nullopt, false);
    checkValues(source.nodeValues(0, 0), {2147483647.0, 0.0, 0.0}, "the largest value");
    checkValues(source.nodeValues(0, 1), {-2147483648.0, 0.0, 0.0}, "the smallest value");
    checkValues(source.nodeValues(0, 2), {-5.0, 0.0, 0.0}, "a negative value");
    checkValues(source.nodeValues(0, 3), {0.0, 0.0, 0.0}, "a value of 0");
}

void check_has_end_day_is_what_it_was_given() {
    check(!makeSource({0}, std::nullopt, false).hasEndDay(), "a grid with no end day");
    check(makeSource({0}, std::nullopt, true).hasEndDay(), "a grid with an end day");
}

}  // namespace

int main() {
    check_axis_count_is_always_three();
    check_nodes_are_found_in_the_flat_array();
    check_raw_axes_go_to_their_canonical_axes();
    check_missing_value();
    check_raw_integers_are_exact();
    check_has_end_day_is_what_it_was_given();

    return snap_test::finish("netcdf_scalar_node_value_source_test");
}
