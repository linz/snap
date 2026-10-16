// Standalone unit test for RectangleIndex and IndexedRectangle (deformation/rectangle_index.h), which find the rectangle
// that contains a point with the lowest id. Failures are reported to stdout and the process exit code is the pass/fail
// signal (0 = all passed), as for the other test-only tools in src/test/.
//
// The last test compares the index with a plain scan of every rectangle, for rectangles of mixed sizes that overlap,
// each with its own tolerances, and for counts of rectangles either side of the points where the tree gains a level.

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "deformation/rectangle_index.h"

namespace {

using deformation::AcceptEveryRectangle;
using deformation::IndexedRectangle;
using deformation::RectangleIndex;

int failures = 0;

void check(const bool condition, const std::string& description) {
    if (condition) {
        return;
    }
    ++failures;
    std::cout << "FAIL: " << description << "\n";
}

/// \return a description of an id that may be missing, for a failure message.
std::string describe(const std::optional<std::size_t>& id) {
    return id ? std::to_string(*id) : std::string("no id");
}

/// Checks the id that the index gives for a point.
void checkId(const std::optional<std::size_t>& actual,    ///< what the index gave
             const std::optional<std::size_t>& expected,  ///< what it must give
             const std::string& description) {            ///< what the check is of
    check(actual == expected, description + ": expected " + describe(expected) + ", got " + describe(actual));
}

/// \return whether making the rectangle throws an std::invalid_argument.
bool constructionThrows(const double min0, const double max0, const double min1, const double max1,
                        const double tolerance0 = 0.0, const double tolerance1 = 0.0) {
    try {
        const IndexedRectangle rectangle(min0, max0, min1, max1, 0, tolerance0, tolerance1);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void check_rectangle_construction() {
    const IndexedRectangle plain(1.0, 3.0, 10.0, 20.0, 7);
    check(plain.min0 == 1.0 && plain.max0 == 3.0 && plain.min1 == 10.0 && plain.max1 == 20.0 && plain.id == 7,
          "a rectangle without tolerances keeps its edges and its id");

    const IndexedRectangle widened(1.0, 3.0, 10.0, 20.0, 7, 0.5, 2.0);
    check(widened.min0 == 0.5 && widened.max0 == 3.5 && widened.min1 == 8.0 && widened.max1 == 22.0,
          "the tolerances widen the edges along their own coordinates");

    check(IndexedRectangle(2.0, 2.0, 5.0, 5.0, 0).min0 == 2.0, "a rectangle that is a single point is allowed");
    check(constructionThrows(3.0, 1.0, 0.0, 1.0), "min0 above max0 is rejected");
    check(constructionThrows(0.0, 1.0, 3.0, 1.0), "min1 above max1 is rejected");
    check(constructionThrows(0.0, 1.0, 0.0, 1.0, -0.1, 0.0), "a negative tolerance0 is rejected");
    check(constructionThrows(0.0, 1.0, 0.0, 1.0, 0.0, -0.1), "a negative tolerance1 is rejected");
    const double notANumber = std::numeric_limits<double>::quiet_NaN();
    check(constructionThrows(notANumber, 1.0, 0.0, 1.0), "a NaN edge is rejected");
    check(constructionThrows(0.0, 1.0, 0.0, 1.0, notANumber, 0.0), "a NaN tolerance is rejected");
}

void check_empty_and_single_rectangle() {
    const RectangleIndex empty{std::vector<IndexedRectangle>()};
    check(empty.size() == 0, "an empty index has size 0");
    checkId(empty.firstContaining(0.0, 0.0), std::nullopt, "an empty index contains no point");

    std::vector<IndexedRectangle> one;
    one.emplace_back(0.0, 1.0, 0.0, 1.0, 42);
    const RectangleIndex single(std::move(one));
    check(single.size() == 1, "an index of one rectangle has size 1");
    checkId(single.firstContaining(0.5, 0.5), 42, "a point inside the only rectangle");
    checkId(single.firstContaining(0.0, 0.0), 42, "a corner of the only rectangle, which is included");
    checkId(single.firstContaining(1.0, 0.5), 42, "an edge of the only rectangle, which is included");
    checkId(single.firstContaining(1.0001, 0.5), std::nullopt, "a point just outside the only rectangle");
    checkId(single.firstContaining(0.5, -0.0001), std::nullopt, "a point just below the only rectangle");
}

void check_tolerance_applies_to_its_own_coordinate() {
    std::vector<IndexedRectangle> list;
    list.emplace_back(0.0, 1.0, 0.0, 1.0, 5, 0.1, 0.0);  // c0 is allowed 0.1 beyond the edges, c1 none
    const RectangleIndex index(std::move(list));
    checkId(index.firstContaining(1.05, 0.5), 5, "a point beyond the c0 edge, within tolerance0");
    checkId(index.firstContaining(-0.05, 0.5), 5, "a point before the c0 edge, within tolerance0");
    checkId(index.firstContaining(1.15, 0.5), std::nullopt, "a point beyond the c0 edge by more than tolerance0");
    checkId(index.firstContaining(0.5, 1.05), std::nullopt, "a point beyond the c1 edge, which has no tolerance");
}

void check_lowest_id_wins_in_any_order() {
    // Three rectangles that all contain (5, 5), and a fourth that does not.
    const auto rectangles = [](const std::vector<std::size_t>& ids) {
        std::vector<IndexedRectangle> list;
        for (const std::size_t id : ids) {
            const double reach = id == 9 ? 1.0 : 10.0 + static_cast<double>(id);
            list.emplace_back(0.0, reach, 0.0, reach, id);
        }
        return list;
    };
    const RectangleIndex ascending(rectangles({3, 4, 6, 9}));
    const RectangleIndex descending(rectangles({9, 6, 4, 3}));
    const RectangleIndex scrambled(rectangles({6, 3, 9, 4}));
    checkId(ascending.firstContaining(5.0, 5.0), 3, "ids given in ascending order");
    checkId(descending.firstContaining(5.0, 5.0), 3, "ids given in descending order");
    checkId(scrambled.firstContaining(5.0, 5.0), 3, "ids given in no order");
    checkId(scrambled.firstContaining(0.5, 0.5), 3, "a point that every rectangle contains");
}

void check_accept() {
    // Rectangles 1, 2 and 3 all contain (0.5, 0.5). Rectangle 4 does not.
    std::vector<IndexedRectangle> list;
    list.emplace_back(0.0, 1.0, 0.0, 1.0, 1);
    list.emplace_back(0.0, 2.0, 0.0, 2.0, 2);
    list.emplace_back(0.0, 3.0, 0.0, 3.0, 3);
    list.emplace_back(5.0, 6.0, 5.0, 6.0, 4);
    const RectangleIndex index(std::move(list));

    checkId(index.firstContaining(0.5, 0.5, AcceptEveryRectangle()), 1, "AcceptEveryRectangle given explicitly");
    checkId(index.firstContaining(0.5, 0.5, [](std::size_t) { return true; }), 1,
            "a lambda that takes every rectangle");
    checkId(index.firstContaining(0.5, 0.5, [](std::size_t id) { return id != 1; }), 2, "the lowest id is passed over");
    checkId(index.firstContaining(0.5, 0.5, [](std::size_t id) { return id == 3; }), 3,
            "only the highest id is accepted");
    checkId(index.firstContaining(0.5, 0.5, [](std::size_t) { return false; }), std::nullopt,
            "every rectangle that contains the point is passed over");

    std::vector<std::size_t> asked;
    checkId(index.firstContaining(0.5, 0.5,
                                  [&asked](std::size_t id) {
                                      asked.push_back(id);
                                      return false;
                                  }),
            std::nullopt, "accept that records the ids it is asked about");
    std::sort(asked.begin(), asked.end());
    check(asked == std::vector<std::size_t>({1, 2, 3}),
          "accept is asked about exactly the rectangles that contain the point, and not about rectangle 4");
}

/// The values that make one rectangle for the random test, kept apart from IndexedRectangle, which cannot be moved
/// around in a list.
struct Spec {
    double min0;
    double max0;
    double min1;
    double max1;
    std::size_t id;
    double tolerance0;
    double tolerance1;
};

/// \return the rectangles for the specs, in the order of the specs.
std::vector<IndexedRectangle> makeRectangles(const std::vector<Spec>& specs) {
    std::vector<IndexedRectangle> list;
    list.reserve(specs.size());
    for (const Spec& spec : specs) {
        list.emplace_back(spec.min0, spec.max0, spec.min1, spec.max1, spec.id, spec.tolerance0, spec.tolerance1);
    }
    return list;
}

void check_against_a_plain_scan() {
    std::mt19937 random(20261016);
    std::uniform_real_distribution<double> position(0.0, 1000.0);
    std::uniform_real_distribution<double> extent(0.5, 60.0);
    std::uniform_real_distribution<double> tolerance(0.0, 2.0);
    const auto accept = [](const std::size_t id) { return id % 5 != 0; };  // as if some grids rejected the point

    for (const std::size_t count : {std::size_t{1}, std::size_t{2}, std::size_t{16}, std::size_t{17}, std::size_t{256},
                                    std::size_t{257}, std::size_t{3000}}) {
        std::vector<Spec> specs;
        for (std::size_t id = 0; id < count; ++id) {
            const double min0 = position(random);
            const double min1 = position(random);
            specs.push_back(
                {min0, min0 + extent(random), min1, min1 + extent(random), id, tolerance(random), tolerance(random)});
        }
        const std::vector<IndexedRectangle> reference = makeRectangles(specs);  // in id order, for the plain scan
        std::vector<Spec> shuffled = specs;
        std::shuffle(shuffled.begin(), shuffled.end(), random);
        const RectangleIndex index(makeRectangles(shuffled));
        check(index.size() == count, "the index has every rectangle it was given, for " + std::to_string(count));

        std::size_t wrongAnswers = 0;
        std::size_t acceptCalledOutOfTurn = 0;
        for (int query = 0; query < 20000; ++query) {
            const double c0 = position(random);
            const double c1 = position(random);

            std::optional<std::size_t> expected;
            for (const IndexedRectangle& rectangle : reference) {
                if (c0 >= rectangle.min0 && c0 <= rectangle.max0 && c1 >= rectangle.min1 && c1 <= rectangle.max1 &&
                    accept(rectangle.id)) {
                    expected = rectangle.id;
                    break;
                }
            }

            // accept must only be asked about a rectangle with a lower id than any that it has already taken.
            std::optional<std::size_t> lowestTaken;
            const auto checkedAccept = [&](const std::size_t id) {
                if (lowestTaken && id >= *lowestTaken) {
                    ++acceptCalledOutOfTurn;
                }
                const bool taken = accept(id);
                if (taken && (!lowestTaken || id < *lowestTaken)) {
                    lowestTaken = id;
                }
                return taken;
            };
            if (index.firstContaining(c0, c1, checkedAccept) != expected) {
                ++wrongAnswers;
            }

            // Without an accept, the answer is the lowest id of any rectangle that contains the point.
            std::optional<std::size_t> expectedWithoutAccept;
            for (const IndexedRectangle& rectangle : reference) {
                if (c0 >= rectangle.min0 && c0 <= rectangle.max0 && c1 >= rectangle.min1 && c1 <= rectangle.max1) {
                    expectedWithoutAccept = rectangle.id;
                    break;
                }
            }
            if (index.firstContaining(c0, c1) != expectedWithoutAccept) {
                ++wrongAnswers;
            }
        }
        check(wrongAnswers == 0, "the index agrees with a plain scan for " + std::to_string(count) + " rectangles (" +
                                     std::to_string(wrongAnswers) + " of 40000 answers differ)");
        check(acceptCalledOutOfTurn == 0, "accept is only asked about ids below the lowest it has taken, for " +
                                              std::to_string(count) + " rectangles (" +
                                              std::to_string(acceptCalledOutOfTurn) + " calls out of turn)");
    }
}

}  // namespace

int main() {
    check_rectangle_construction();
    check_empty_and_single_rectangle();
    check_tolerance_applies_to_its_own_coordinate();
    check_lowest_id_wins_in_any_order();
    check_accept();
    check_against_a_plain_scan();

    if (failures > 0) {
        std::cout << failures << " rectangle_index_test check(s) failed\n";
        return 1;
    }
    std::cout << "rectangle_index_test: all checks passed\n";
    return 0;
}
