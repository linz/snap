#include "deformation/rectangle_index.h"

#include <boost/numeric/conversion/cast.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>

namespace deformation {

namespace {

/// Sorts rectangles using Sort-Tile-Recursive packing, which is well suited to fast spatial queries over those
/// rectangles.
///
/// This is a helper function for the RectangleIndex constructor, which uses it to sort the rectangles that it is
/// given, and then each level of nodes that it makes over them.
///
/// centre0 and centre1 can be lambdas, function objects or functions. Each must be callable as
///
///     double centre0(std::size_t index)
///
/// where index is the number of one of the rectangles, from 0 to count - 1, and the result is the centre of that
/// rectangle along the first coordinate (centre0) or along the second coordinate (centre1). For example, the
/// RectangleIndex constructor passes
///
///     [&rectangles](std::size_t index) { return 0.5 * (rectangles[index].min0 + rectangles[index].max0); }
///
/// as centre0, which is the centre of rectangles[index] along c0.
///
/// count is how many rectangles there are, numbered from 0 to count - 1. fanout is how many rectangles the tree holds
/// in each node on its bottom level (defines the "adaptivity" of the grid), and so the most children that any node has.
///
/// The steps are:
///   1. groupCount = ceil(count / fanout) is how many runs of fanout rectangles there will be. Lay them out as a grid
///      that is about strips = ceil(sqrt(groupCount)) runs across and as many down.
///   2. Sort the rectangles by the centre of their first coordinate, and cut that order into strips of
///      strips * fanout rectangles each. Within a strip, the first coordinate is now about the same for every
///      rectangle.
///   3. Sort each strip by the centre of the second coordinate.
///   4. The strips, one after another, are the result.
/// For 2175 rectangles and a fanout of 16, that is 136 runs, 12 strips and 192 rectangles in each strip (12 runs), with
/// the last strip holding the remaining 63.
///
/// It sorts the numbers and does not move the rectangles, so that it works for rectangles that cannot be assigned to.
/// \return the numbers 0 to count - 1, in the order that the rectangles were sorted into.
template <typename Centre0, typename Centre1>
std::vector<std::size_t> packedOrder(const std::size_t count,   ///< how many rectangles there are
                                     const std::size_t fanout,  ///< how many rectangles each tile holds (see the comment above)
                                     Centre0 centre0,           ///< centre0(index): that rectangle's centre on the first coordinate
                                     Centre1 centre1) {         ///< centre1(index): that rectangle's centre on the second
    std::vector<std::size_t> order(count);
    std::iota(order.begin(), order.end(), std::size_t{0});

    const std::size_t groupCount = (count + fanout - 1) / fanout;
    const std::size_t strips = boost::numeric_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(groupCount))));
    const std::size_t stripSize = strips * fanout;

    std::sort(order.begin(), order.end(), [&centre0](const std::size_t first, const std::size_t second) {
        return centre0(first) < centre0(second);
    });
    for (std::size_t start = 0; start < count; start += stripSize) {
        const std::size_t end = std::min(start + stripSize, count);
        std::sort(order.begin() + boost::numeric_cast<std::ptrdiff_t>(start),
                  order.begin() + boost::numeric_cast<std::ptrdiff_t>(end),
                  [&centre1](const std::size_t first, const std::size_t second) {
                      return centre1(first) < centre1(second);
                  });
    }
    return order;
}

}  // namespace

IndexedRectangle::IndexedRectangle(const double min0, const double max0, const double min1, const double max1,
                                   const std::size_t id, const double tolerance0, const double tolerance1)
    : min0(min0 - tolerance0), max0(max0 + tolerance0), min1(min1 - tolerance1), max1(max1 + tolerance1), id(id) {
    // The tests use the arguments, not the members (this->X), which have the tolerances added. A NaN fails every comparison, so it
    // is rejected here too.
    const bool valid = min0 <= max0 && min1 <= max1 && tolerance0 >= 0.0 && tolerance1 >= 0.0;  // false if any is NaN
    if (!valid) {
        throw std::invalid_argument(
            "IndexedRectangle: needs min0 <= max0, min1 <= max1 and non-negative tolerances (min0=" +
            std::to_string(min0) + ", max0=" + std::to_string(max0) + ", min1=" + std::to_string(min1) +
            ", max1=" + std::to_string(max1) + ", tolerance0=" + std::to_string(tolerance0) +
            ", tolerance1=" + std::to_string(tolerance1) + ")");
    }
}

RectangleIndex::RectangleIndex(std::vector<IndexedRectangle> rectangles) {
    if (rectangles.empty()) {
        return;
    }

    // The items are copies of the rectangles, in the order that packedOrder() gives.
    const std::vector<std::size_t> itemOrder =
        packedOrder(rectangles.size(), kFanout,
                    [&rectangles](const std::size_t i) { return 0.5 * (rectangles[i].min0 + rectangles[i].max0); },
                    [&rectangles](const std::size_t i) { return 0.5 * (rectangles[i].min1 + rectangles[i].max1); });
    _items.reserve(rectangles.size());
    for (const std::size_t index : itemOrder) {
        _items.push_back(rectangles[index]);
    }

    // Makes the node that has the count children from position first on, in a list of items or of nodes. Both have the
    // rectangle and the id that the node needs.
    const auto makeNode = [](const auto& children, const std::size_t first, const std::size_t count) {
        double min0 = children[first].min0;
        double max0 = children[first].max0;
        double min1 = children[first].min1;
        double max1 = children[first].max1;
        std::size_t lowestId = children[first].id;
        for (std::size_t child = first + 1; child < first + count; ++child) {
            min0 = std::min(min0, children[child].min0);
            max0 = std::max(max0, children[child].max0);
            min1 = std::min(min1, children[child].min1);
            max1 = std::max(max1, children[child].max1);
            lowestId = std::min(lowestId, children[child].id);
        }
        return Node(min0, max0, min1, max1, lowestId, first, count);
    };

    // Makes the level of nodes over a list of items or of nodes: a node for each run of up to kFanout of them.
    const auto makeLevelOver = [&makeNode](const auto& children) {
        std::vector<Node> level;
        for (std::size_t first = 0; first < children.size(); first += kFanout) {
            level.push_back(makeNode(children, first, std::min(kFanout, children.size() - first)));
        }
        return level;
    };

    // The nodes of each level are sorted with packedOrder() before the level above them is made, so that the runs of
    // nodes that the level above takes together are compact. A node holds the position of its own children, so moving
    // the node does not change what it has as children.
    std::vector<Node> level = makeLevelOver(_items);
    while (level.size() > 1) {
        const std::vector<std::size_t> nodeOrder =
            packedOrder(level.size(), kFanout,
                        [&level](const std::size_t i) { return 0.5 * (level[i].min0 + level[i].max0); },
                        [&level](const std::size_t i) { return 0.5 * (level[i].min1 + level[i].max1); });
        std::vector<Node> sorted;
        sorted.reserve(level.size());
        for (const std::size_t index : nodeOrder) {
            sorted.push_back(level[index]);
        }
        std::vector<Node> above = makeLevelOver(sorted);
        _levels.push_back(std::move(sorted));
        level = std::move(above);
    }
    _levels.push_back(std::move(level));  // the last level, which is the top node alone
}

std::size_t RectangleIndex::size() const {
    return _items.size();
}

}  // namespace deformation
