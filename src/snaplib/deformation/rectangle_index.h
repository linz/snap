#pragma once

/// \file
/// Finds which of a set of rectangles in (c0, c1) coordinates contain a point, and of those the one that is first in a
/// given order. The deformation code uses this to find which root grid of a layer covers a point, where the order is the
/// order of the grids in the file.

#include <cstddef>
#include <optional>
#include <type_traits>
#include <vector>

namespace deformation {

/// A rectangle in (c0, c1) coordinates, with the place in the order that it has. The edges of the rectangle include the
/// tolerance that it was made with, so that a point within the tolerance of an edge counts as inside.
struct IndexedRectangle {
    /// \throws std::invalid_argument if min0 > max0 or min1 > max1, if a tolerance is negative, or if any number is NaN.
    IndexedRectangle(double min0,               ///< the lowest c0 that the rectangle has
                     double max0,               ///< the highest c0 that the rectangle has
                     double min1,               ///< the lowest c1 that the rectangle has
                     double max1,               ///< the highest c1 that the rectangle has
                     std::size_t id,            ///< where the rectangle comes in the order, with the lowest first
                     double tolerance0 = 0.0,   ///< how far beyond the c0 edges a point may be and still count as inside
                     double tolerance1 = 0.0);  ///< how far beyond the c1 edges a point may be and still count as inside

    const double min0;  ///< the lowest c0 that counts as inside: the c0 edge less its tolerance
    const double max0;  ///< the highest c0 that counts as inside: the c0 edge plus its tolerance
    const double min1;  ///< the lowest c1 that counts as inside: the c1 edge less its tolerance
    const double max1;  ///< the highest c1 that counts as inside: the c1 edge plus its tolerance

    /// Where the rectangle comes in the order, with the lowest first. A point that is in several rectangles is given to
    /// the one with the lowest id.
    const std::size_t id;
};

/// What RectangleIndex::firstContaining() does when it is not given an accept: it takes every rectangle that contains
/// the point.
struct AcceptEveryRectangle {
    /// \return true, whatever the id.
    [[nodiscard]] bool operator()(std::size_t /*id*/) const {
        return true;
    }
};

/// A set of rectangles that is searched by point. It is built once, from all of the rectangles, and does not change. A
/// search does not look at every rectangle: they are held in a tree of nested rectangles, and a search skips every part
/// of the tree whose rectangle does not contain the point, or that has no id lower than the best one found so far. It
/// does not depend on the rectangles being of one size, in a regular pattern or apart from each other, nor on the order
/// that they were given in.
///
/// The tree is built over the rectangles in packed order: an order that keeps small the area of the smallest rectangle
/// that holds any run of up to 16 rectangles that are next to each other in it. A node on the bottom level of the tree
/// holds one such run, so its own rectangle is small and few other rectangles overlap it. That is what lets a search
/// skip most of the tree. The order is made by sorting the rectangles by the centre of their c0 range, cutting
/// the list into strips, and sorting each strip by the centre of its c1 range (Sort-Tile-Recursive packing). It has
/// nothing to do with the ids, which are only used to choose between rectangles that contain the same point.
///
/// The packing is like laying a temporary adaptive grid over the plane. Unlike a fixed grid, its tiles are cut to hold
/// the same number of rectangles each, up to 16, so they are small where the rectangles are close together and large
/// where they are far apart. The grid is thrown away once the order is made, and each node keeps the smallest rectangle that holds
/// its own run, which is not a tile boundary. Every rectangle is in exactly one run, however large it is, so a large
/// rectangle never has to be recorded in many tiles as it would be in a fixed grid.
///
/// The rectangles are bounding rectangles: each stands for something, such as a grid, that lies inside it. They only
/// say where a point can be. What they stand for may not fill its rectangle, so a search that needs the exact answer
/// also gives the caller the final say on each rectangle that contains the point (see firstContaining()).
class RectangleIndex {
 public:
    explicit RectangleIndex(std::vector<IndexedRectangle> rectangles);  ///< the rectangles, in any order

    /// \return how many rectangles there are.
    [[nodiscard]] std::size_t size() const;

    /// Finds the rectangle that a point is in, with the lowest id, among those that the caller accepts.
    ///
    /// A rectangle is only the bounding rectangle of whatever it stands for, and it can contain points that the thing
    /// itself does not. A grid whose edges are not along c0 and c1 fills only part of its rectangle, for example. So
    /// for each rectangle that contains the point, this can ask the caller whether the point is really in what the
    /// rectangle stands for, by calling accept with the rectangle's id. If accept says no, the search goes on to the
    /// next lowest id.
    ///
    /// accept can be left out, and then every rectangle that contains the point is taken. That is right when each
    /// rectangle is exactly what it stands for, as the rectangle of a grid whose edges are along c0 and c1 is.
    ///
    /// accept can be a lambda, a function object or a function. It must be callable as
    ///
    ///     bool accept(std::size_t id)
    ///
    /// where id is the id that the rectangle was made with, and the result is true to take that rectangle or false to
    /// pass it over. The compiler checks this. For example, if the ids are positions in a vector of grids, called
    /// grids, then Grid::locate() is the exact test of whether a point is in a grid, and gives std::nullopt if it is
    /// not:
    ///
    ///     index.firstContaining(c0, c1, [&grids, c0, c1](std::size_t id) {
    ///         return grids[id]->locate(c0, c1).has_value();
    ///     });
    ///
    /// accept is called only for a rectangle that contains the point and has an id lower than any that it has already
    /// returned true for. The ids are not tried in ascending order, and accept can be called for a rectangle that is
    /// then not the one returned, so its answer must depend only on the id and on the point, and not on how many times
    /// or in what order it has been called.
    /// \return the id of the lowest accepted rectangle, or std::nullopt if no rectangle that contains the point is
    ///         accepted.
    template <typename Accept = AcceptEveryRectangle>
    [[nodiscard]] std::optional<std::size_t> firstContaining(
        double c0,                                 ///< the first coordinate of the point
        double c1,                                 ///< the second coordinate of the point
        Accept&& accept = AcceptEveryRectangle())  ///< called as bool accept(std::size_t id); true takes that rectangle
        const {
        static_assert(std::is_invocable_r_v<bool, Accept&, std::size_t>,
                      "accept must be callable as bool accept(std::size_t id)");
        std::optional<std::size_t> best;
        if (!_levels.empty()) {
            _search(_levels.size() - 1, 0, c0, c1, accept, best);
        }
        return best;
    }

 private:
    /// The most children that a node has.
    static constexpr std::size_t kFanout = 16;

    /// An IndexedRectangle that holds a group of items, or of the nodes of the level below, and has them as its
    /// children. Its id is the lowest id of any item below it. (An item is an entry of _items: one of the rectangles
    /// that the index was given.)
    struct Node : IndexedRectangle {
        Node(double min0,        ///< the lowest c0 of anything below the node
             double max0,        ///< the highest c0 of anything below the node
             double min1,        ///< the lowest c1 of anything below the node
             double max1,        ///< the highest c1 of anything below the node
             std::size_t id,     ///< the lowest id of any item below the node
             std::size_t first,  ///< the position of the first child: in _items for a node on level 0, else in the level below
             std::size_t count)  ///< how many children
            // The rectangle is exactly the one that holds the children, so there is no tolerance to add, whatever the
            // defaults of IndexedRectangle are.
            : IndexedRectangle(min0, max0, min1, max1, id, 0.0, 0.0), first(first), count(count) {}

        const std::size_t first;
        const std::size_t count;
    };

    /// Searches one node and everything below it, and updates best if it finds an accepted item with a lower id. The
    /// search skips the node, with all that is below it, if the point is outside the node, or if the node's id is not
    /// lower than best, since nothing below can then beat it. A node on level 0 is searched by testing each of its
    /// children, which are items. Any other node is searched by searching each of its children, which are nodes.
    template <typename Accept>
    void _search(std::size_t level,                 ///< the level that the node is on, with 0 the lowest
                 std::size_t index,                 ///< the node's place among the nodes of that level
                 double c0,                         ///< the first coordinate of the point
                 double c1,                         ///< the second coordinate of the point
                 Accept& accept,                    ///< see firstContaining()
                 std::optional<std::size_t>& best)  ///< the lowest id found so far, updated
        const {
        const Node& node = _levels[level][index];
        if (c0 < node.min0 || c0 > node.max0 || c1 < node.min1 || c1 > node.max1) {
            return;
        }
        if (best && node.id >= *best) {
            return;  // nothing below this node can come before what has been found
        }
        if (level == 0) {
            for (std::size_t child = node.first; child < node.first + node.count; ++child) {
                const IndexedRectangle& item = _items[child];
                const bool inside = c0 >= item.min0 && c0 <= item.max0 && c1 >= item.min1 && c1 <= item.max1;
                if (inside && (!best || item.id < *best) && accept(item.id)) {
                    best = item.id;
                }
            }
            return;
        }
        for (std::size_t child = node.first; child < node.first + node.count; ++child) {
            _search(level - 1, child, c0, c1, accept, best);
        }
    }

    /// Every item, in packed order (see the class comment), so that items that are near each other in space are next to
    /// each other. The children of a node on level 0 are a run of these.
    std::vector<IndexedRectangle> _items;

    /// The tree, as a list of its levels from the bottom up. Each level is a list of nodes.
    ///
    /// A node stands for a group of up to kFanout children that sit next to each other in a list below it. Node::first
    /// is the position of the first child in that list and Node::count is how many there are. For a node of _levels[0]
    /// the list is _items and the children are IndexedRectangles. For a node of any other level the list is the level
    /// before it and the children are Nodes.
    ///
    /// For example, with 2175 items:
    ///
    ///     _levels[2]    1 node      the top node, which stands for all 9 nodes of _levels[1]
    ///     _levels[1]    9 nodes     each stands for up to 16 nodes of _levels[0]
    ///     _levels[0]    136 nodes   each stands for up to 16 of the IndexedRectangles in _items
    ///     _items        2175 items
    ///
    /// A node's own rectangle is the smallest one that holds all of its children, and its id is the lowest id among
    /// them. A search starts at the [singular] top node (which is _levels.back()[0]), and works down.
    std::vector<std::vector<Node>> _levels;
};

}  // namespace deformation
