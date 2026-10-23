// Standalone unit test for Grid, GridParent and descendToLeaf (deformation/grid.h): the accessors of a grid, the checks
// that GridParent::addChild makes, resolveChild() choosing the last child that covers a point, and descendToLeaf()
// going down through the levels of a hierarchy. The grids are real, with real geometry, so locating a point in them
// works as it does in use. Only the values of their nodes are stand-ins, and no check reads them, since nothing here
// evaluates a grid's data.
//
// Grid, GridParent and descendToLeaf are templates over the type of the node data, and the only thing that their code
// asks of that type is axisCount(). The checks use RaggedGridData as a way in to that template code, not because
// RaggedGridData is under test; the same lines are compiled for ScalarGridData, so checking with it as well would run
// the same lines again. Each type of node data has its own tests. Failures are reported to stdout and the process
// exit code is the pass/fail signal (0 = all passed), as for the other test-only tools in src/test/.
//
// The grids are axis-aligned, with node (i, j) at c0 = x0 + i and c1 = y0 + j, so that the extent of a grid is easy
// to read from its coefficients: {x0, 1, 0, y0, 0, 1} and its node counts.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "deformation/grid.h"
#include "deformation/grid_geometry.h"
#include "deformation/ragged_grid_data.h"
#include "test_support.h"

namespace {

using deformation::Cell;
using deformation::descendToLeaf;
using deformation::Grid;
using deformation::GridGeometry;
using deformation::GridParent;
using deformation::RaggedGridData;
using deformation::RaggedNodeValueSource;
using deformation::RaggedSegmentRange;
using deformation::Resolution;

using RaggedGrid = Grid<RaggedGridData>;
using RaggedParent = GridParent<RaggedGridData>;

using snap_test::check;
using snap_test::checkThrows;

/// Ragged node data that has no segments, since no check evaluates it.
class FakeRaggedValues : public RaggedNodeValueSource {
 public:
    explicit FakeRaggedValues(const std::size_t axisCount) : _axisCount(axisCount) {}

    [[nodiscard]] std::size_t axisCount() const override {
        return _axisCount;
    }

    [[nodiscard]] bool hasEndDay(const std::uint32_t, const std::uint32_t) const override {
        return false;
    }

    [[nodiscard]] RaggedSegmentRange segments(const std::uint32_t, const std::uint32_t,
                                              const std::size_t) const override {
        return RaggedSegmentRange(nullptr, nullptr);
    }

 private:
    std::size_t _axisCount;
};

/// \return the geometry of a grid that covers c0 in [x0, x0 + iNodeCount - 1] and c1 in [y0, y0 + jNodeCount - 1], with
/// no boundary tolerance.
GridGeometry makeGeometry(const double x0, const double y0, const std::uint32_t iNodeCount,
                          const std::uint32_t jNodeCount) {
    return GridGeometry({x0, 1.0, 0.0, y0, 0.0, 1.0}, iNodeCount, jNodeCount, 0.0);
}

/// \return a grid with no children, of ragged node data that has that many axes.
std::unique_ptr<RaggedGrid> makeLeaf(const double x0, const double y0, const std::uint32_t iNodeCount,
                                     const std::uint32_t jNodeCount, const std::size_t axisCount = 1) {
    return std::make_unique<RaggedGrid>(makeGeometry(x0, y0, iNodeCount, jNodeCount),
                                        RaggedGridData(std::make_unique<FakeRaggedValues>(axisCount)));
}

/// \return a grid that can have children, which has none yet.
std::unique_ptr<RaggedParent> makeParent(const double x0, const double y0, const std::uint32_t iNodeCount,
                                         const std::uint32_t jNodeCount, const std::size_t axisCount = 1) {
    return std::make_unique<RaggedParent>(makeGeometry(x0, y0, iNodeCount, jNodeCount),
                                          RaggedGridData(std::make_unique<FakeRaggedValues>(axisCount)));
}

/// \return whether the cell has that base corner and fractional position.
bool cellIs(const Cell& cell, const std::uint32_t bi, const std::uint32_t bj, const double fi, const double fj) {
    return cell.bi == bi && cell.bj == bj && cell.fi == fi && cell.fj == fj;
}

void check_grid_accessors() {
    // Covers c0 in [0, 4] and c1 in [0, 3], and has 2 axes.
    const auto grid = makeLeaf(0.0, 0.0, 5, 4, 2);
    check(grid->geometry().iNodeCount() == 5 && grid->geometry().jNodeCount() == 4, "the geometry of a grid");
    check(grid->data().axisCount() == 2, "the node data of a grid");
    check(grid->axisCount() == 2, "the axis count of a grid is that of its node data");
    check(grid->asParent() == nullptr, "a grid that is not a GridParent has no parent view of itself");

    const auto cell = grid->locate(1.5, 2.5);
    check(cell && cellIs(*cell, 1, 2, 0.5, 0.5), "locate of a grid is that of its geometry, for a point inside");
    check(!grid->locate(4.5, 1.0), "locate of a grid is that of its geometry, for a point outside");
}

void check_add_child_validation() {
    // The parent covers [0, 10] by [0, 10] and has 1 axis.
    const auto parent = makeParent(0.0, 0.0, 11, 11);

    checkThrows<std::invalid_argument>([&parent] { parent->addChild(nullptr); }, "must not be null",
                                       "a null child is rejected");
    checkThrows<std::invalid_argument>([&parent] { parent->addChild(makeLeaf(2.0, 2.0, 3, 3, 2)); }, "axisCount",
                                       "a child with a different axis count is rejected");
    checkThrows<std::invalid_argument>([&parent] { parent->addChild(makeLeaf(8.0, 8.0, 5, 5)); }, "contained",
                                       "a child that extends past the parent is rejected");
    check(parent->children().empty(), "a rejected child is not added");

    auto first = makeLeaf(2.0, 2.0, 3, 3);
    auto second = makeLeaf(6.0, 6.0, 3, 3);
    const RaggedGrid* firstPointer = first.get();
    const RaggedGrid* secondPointer = second.get();
    parent->addChild(std::move(first));
    parent->addChild(std::move(second));
    check(parent->children().size() == 2 && parent->children()[0].get() == firstPointer &&
              parent->children()[1].get() == secondPointer,
          "the children are in the order that they were added, and the parent owns them");
}

void check_resolve_child() {
    const auto parent = makeParent(0.0, 0.0, 11, 11);
    auto left = makeLeaf(2.0, 2.0, 4, 4);   // covers [2, 5] by [2, 5]
    auto right = makeLeaf(6.0, 6.0, 4, 4);  // covers [6, 9] by [6, 9]
    const RaggedGrid* leftPointer = left.get();
    const RaggedGrid* rightPointer = right.get();

    check(!parent->resolveChild(3.0, 3.0), "a parent with no children resolves no point");

    parent->addChild(std::move(left));
    parent->addChild(std::move(right));

    const auto inLeft = parent->resolveChild(3.0, 3.0);
    check(inLeft && &inLeft->grid.get() == leftPointer && cellIs(inLeft->cell, 1, 1, 0.0, 0.0),
          "a point in the first child resolves to it, with the cell in it");
    const auto inRight = parent->resolveChild(7.5, 6.5);
    check(inRight && &inRight->grid.get() == rightPointer && cellIs(inRight->cell, 1, 0, 0.5, 0.5),
          "a point in the second child resolves to it, with the cell in it");
    check(!parent->resolveChild(5.5, 5.5), "a point between the children, in the parent only, resolves to no child");
    check(!parent->resolveChild(0.5, 0.5), "a point in the parent but in no child resolves to no child");
}

void check_resolve_child_prefers_the_last_child() {
    const auto parent = makeParent(0.0, 0.0, 11, 11);
    auto lower = makeLeaf(2.0, 2.0, 4, 4);   // covers [2, 5] by [2, 5]
    auto higher = makeLeaf(3.0, 3.0, 4, 4);  // covers [3, 6] by [3, 6], so [3, 5] by [3, 5] is in both
    const RaggedGrid* lowerPointer = lower.get();
    const RaggedGrid* higherPointer = higher.get();
    parent->addChild(std::move(lower));
    parent->addChild(std::move(higher));

    const auto both = parent->resolveChild(4.0, 4.0);
    check(both && &both->grid.get() == higherPointer, "a point in two children resolves to the one added last");
    const auto onlyLower = parent->resolveChild(2.5, 2.5);
    check(onlyLower && &onlyLower->grid.get() == lowerPointer, "a point in the first of two children only");
    const auto onlyHigher = parent->resolveChild(5.5, 5.5);
    check(onlyHigher && &onlyHigher->grid.get() == higherPointer, "a point in the last of two children only");
}

void check_as_parent() {
    const auto parent = makeParent(0.0, 0.0, 3, 3);
    const RaggedGrid& asGrid = *parent;
    check(asGrid.asParent() == parent.get(), "a GridParent, held as a Grid, gives itself as its parent view");
}

void check_descend_to_leaf() {
    // Three levels: the root covers [0, 10] by [0, 10], its child covers [2, 8] by [2, 8], and that child's own child
    // covers [4, 6] by [4, 6].
    auto root = makeParent(0.0, 0.0, 11, 11);
    auto middle = makeParent(2.0, 2.0, 7, 7);
    auto leaf = makeLeaf(4.0, 4.0, 3, 3);
    const RaggedGrid* rootPointer = root.get();
    const RaggedGrid* middlePointer = middle.get();
    const RaggedGrid* leafPointer = leaf.get();
    middle->addChild(std::move(leaf));
    root->addChild(std::move(middle));

    const auto start = [&root](const double c0, const double c1) {
        return Resolution<RaggedGridData>{*root, *root->locate(c0, c1)};
    };

    const auto atLeaf = descendToLeaf(start(5.0, 5.0), 5.0, 5.0);
    check(&atLeaf.grid.get() == leafPointer && cellIs(atLeaf.cell, 1, 1, 0.0, 0.0),
          "a point in the innermost grid goes down all the levels, and the cell is the one in that grid");
    const auto atMiddle = descendToLeaf(start(3.0, 3.0), 3.0, 3.0);
    check(&atMiddle.grid.get() == middlePointer,
          "a point that only the middle grid covers stops there, as no child of it does");
    const auto atRoot = descendToLeaf(start(1.0, 1.0), 1.0, 1.0);
    check(&atRoot.grid.get() == rootPointer && cellIs(atRoot.cell, 1, 1, 0.0, 0.0),
          "a point that only the root covers stays at the root, as none of its children does");

    const Resolution<RaggedGridData> atLeafStart{*leafPointer, *leafPointer->locate(5.0, 5.0)};
    const auto fromLeaf = descendToLeaf(atLeafStart, 5.0, 5.0);
    check(&fromLeaf.grid.get() == leafPointer, "a grid that is not a GridParent stays where it is");
}

}  // namespace

int main() {
    check_grid_accessors();
    check_add_child_validation();
    check_resolve_child();
    check_resolve_child_prefers_the_last_child();
    check_as_parent();
    check_descend_to_leaf();

    return snap_test::finish("grid_test");
}
