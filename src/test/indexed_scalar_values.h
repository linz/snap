// Shared by the unit tests of the scalar grids and layers: node values that show which node was read, and on which
// axis. Node (i, j) has the value offset + 100 * a + 10 * i + j on axis a. Bilinear interpolation of that is exact, so
// the value at a cell shows which nodes were read, and which way round the grid's axes run, and the offset tells grids
// apart.
//
// A node always gives three values, whatever the grid's axis count. The values for axes that the grid does not have
// are not 0 but follow the same formula, so code that wrongly used one would give a value that is not 0, which a test
// can see.

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "deformation/scalar_grid_data.h"
#include "test_support.h"

namespace snap_test {

/// How far a value may be from the one expected, for checkValues().
inline constexpr double kValueTolerance = 1e-12;

/// Checks that the values that a grid's interpolate() or a layer's evaluate() gave are the three expected. The values
/// for the axes that the grid does not have are 0.
inline void checkValues(const std::optional<std::array<double, 3>>& actual, const std::array<double, 3>& expected,
                        const std::string& messageOnError) {
    if (!actual) {
        check(false, messageOnError + ": there were no values");
        return;
    }
    for (std::size_t a = 0; a < expected.size(); ++a) {
        checkEqual((*actual)[a], expected[a], kValueTolerance, messageOnError + " (axis " + std::to_string(a) + ")");
    }
}

class IndexedScalarValues : public deformation::ScalarNodeValueSource {
 public:
    /// The (i, j) of a node.
    using Node = std::pair<std::uint32_t, std::uint32_t>;

    explicit IndexedScalarValues(const std::size_t axisCount,                     ///< how many axes the grid has
                                 const double offset = 0.0,                       ///< added to every value
                                 const bool hasEndDay = false,                    ///< what hasEndDay() gives
                                 const std::vector<Node>& nodesWithNoValue = {})  ///< the nodes that have no value
        : _axisCount(axisCount), _offset(offset), _hasEndDay(hasEndDay), _nodesWithNoValue(nodesWithNoValue) {}

    [[nodiscard]] std::size_t axisCount() const override {
        return _axisCount;
    }

    [[nodiscard]] std::optional<std::array<double, 3>> nodeValues(const std::uint32_t i,
                                                                  const std::uint32_t j) const override {
        if (std::find(_nodesWithNoValue.begin(), _nodesWithNoValue.end(), Node(i, j)) != _nodesWithNoValue.end()) {
            return std::nullopt;
        }
        std::array<double, 3> values{};
        static_assert(std::tuple_size_v<decltype(values)> <= (std::uint64_t{1} << std::numeric_limits<double>::digits),
                      "an axis index is converted to a double, which must be able to hold every index exactly");
        for (std::size_t a = 0; a < values.size(); ++a) {
            values[a] = _offset + 100.0 * static_cast<double>(a) + 10.0 * i + j;
        }
        return values;
    }

    [[nodiscard]] bool hasEndDay() const override {
        return _hasEndDay;
    }

 private:
    std::size_t _axisCount;
    double _offset;
    bool _hasEndDay;
    std::vector<Node> _nodesWithNoValue;
};

}  // namespace snap_test
