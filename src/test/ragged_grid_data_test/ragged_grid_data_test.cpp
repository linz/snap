// Standalone unit test for RaggedGridData (deformation/ragged_grid_data.h): the value of a node at a query day, which is
// given by the last of the node's segments that starts on or before the day. Failures are reported to stdout and the
// process exit code is the pass/fail signal (0 = all passed), as for the other test-only tools in src/test/.
//
// The values are read at the base node (0, 0) of a cell with fractions of 0, which has all of the weight, so the
// result of evaluate() is that node's value on each axis.

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "deformation/grid_geometry.h"
#include "deformation/ragged_grid_data.h"
#include "test_support.h"

namespace {

using deformation::Cell;
using deformation::RaggedGridData;
using deformation::RaggedNodeValueSource;
using deformation::RaggedSegment;
using deformation::RaggedSegmentRange;
using snap_test::check;

/// The segments of a grid in which every node has the same segments on each axis.
class SameSegmentsAtEveryNode : public RaggedNodeValueSource {
 public:
    explicit SameSegmentsAtEveryNode(std::vector<std::vector<RaggedSegment>> segmentsOfAxis)  ///< each axis's segments
        : _segmentsOfAxis(std::move(segmentsOfAxis)) {}

    [[nodiscard]] std::size_t axisCount() const override {
        return _segmentsOfAxis.size();
    }

    [[nodiscard]] bool hasEndDay(const std::uint32_t, const std::uint32_t) const override {
        return false;
    }

    [[nodiscard]] RaggedSegmentRange segments(const std::uint32_t, const std::uint32_t,
                                              const std::size_t axis) const override {
        const std::vector<RaggedSegment>& list = _segmentsOfAxis[axis];
        return RaggedSegmentRange(list.data(), list.data() + list.size());
    }

 private:
    std::vector<std::vector<RaggedSegment>> _segmentsOfAxis;
};

/// \return the value that evaluate() gives for the segments of every axis, on every axis, at a query day.
std::array<double, 3> valuesAt(const std::vector<std::vector<RaggedSegment>>& segmentsOfAxis, const double queryDay) {
    const RaggedGridData data(std::make_unique<SameSegmentsAtEveryNode>(segmentsOfAxis));
    return data.evaluate(Cell{0, 0, 0.0, 0.0}, queryDay);
}

void checkValue(const std::vector<std::vector<RaggedSegment>>& segmentsOfAxis, const double queryDay,
                const std::size_t axis, const double expected, const std::string& description) {
    const double actual = valuesAt(segmentsOfAxis, queryDay)[axis];
    check(actual == expected,
          description + ": expected " + std::to_string(expected) + ", got " + std::to_string(actual));
}

void check_values_at_given_days() {
    // Axis 0 has three segments. The first starts at day 10 with a value of 100 and a rate of 2 a day, the second at
    // day 20 with 150 and -1 a day, and the third at day 40 with 0 and 0.5 a day. Axis 1 has no segment.
    const std::vector<std::vector<RaggedSegment>> segments = {
        {RaggedSegment{10.0, 100.0, 2.0}, RaggedSegment{20.0, 150.0, -1.0}, RaggedSegment{40.0, 0.0, 0.5}}, {}};
    checkValue(segments, 5.0, 0, 0.0, "before the first segment");
    checkValue(segments, 9.999, 0, 0.0, "just before the first segment");
    checkValue(segments, 10.0, 0, 100.0, "on the day that the first segment starts");
    checkValue(segments, 15.0, 0, 110.0, "inside the first segment");
    checkValue(segments, 19.5, 0, 119.0, "just before the second segment");
    checkValue(segments, 20.0, 0, 150.0, "on the day that the second segment starts");
    checkValue(segments, 30.0, 0, 140.0, "inside the second segment");
    checkValue(segments, 40.0, 0, 0.0, "on the day that the last segment starts");
    checkValue(segments, 100.0, 0, 30.0, "long after the last segment starts");
    checkValue(segments, 1e9, 0, 0.5e9 - 20.0, "very long after the last segment starts");
    checkValue(segments, 15.0, 1, 0.0, "an axis with no segments");
    checkValue(segments, 15.0, 2, 0.0, "an axis beyond the grid's own axis count");
}

void check_axes_are_independent() {
    const std::vector<std::vector<RaggedSegment>> segments = {{RaggedSegment{0.0, 1.0, 0.0}},
                                                              {RaggedSegment{50.0, 7.0, 1.0}}};
    const std::array<double, 3> early = valuesAt(segments, 10.0);
    check(early[0] == 1.0 && early[1] == 0.0, "axis 1 has not started when axis 0 has");
    const std::array<double, 3> late = valuesAt(segments, 60.0);
    check(late[0] == 1.0 && late[1] == 17.0, "axis 1 has started and axis 0 has not changed");
}

/// The value that the segments give at a day, found by walking them from the first.
double valueByWalking(const std::vector<RaggedSegment>& segments, const double queryDay) {
    double value = 0.0;
    for (const RaggedSegment& segment : segments) {
        if (segment.startDay > queryDay) {
            break;
        }
        value = segment.startValue + segment.ratePerDay * (queryDay - segment.startDay);
    }
    return value;
}

void check_against_walking_from_the_first_segment() {
    std::mt19937 random(20261016);
    std::uniform_real_distribution<double> gap(0.0, 30.0);
    std::uniform_real_distribution<double> value(-500.0, 500.0);
    std::uniform_real_distribution<double> rate(-3.0, 3.0);
    std::uniform_int_distribution<int> length(0, 60);

    std::size_t differences = 0;
    for (int trial = 0; trial < 300; ++trial) {
        std::vector<RaggedSegment> segments;
        double day = 0.0;
        for (int k = length(random); k > 0; --k) {
            day += gap(random);
            segments.push_back(RaggedSegment{day, value(random), rate(random)});
        }
        const RaggedGridData data(
            std::make_unique<SameSegmentsAtEveryNode>(std::vector<std::vector<RaggedSegment>>{segments}));

        std::vector<double> days = {-10.0, 0.0, day, day + 100.0};
        for (const RaggedSegment& segment : segments) {  // every start day exactly, and either side of it
            days.push_back(segment.startDay);
            days.push_back(segment.startDay - 0.001);
            days.push_back(segment.startDay + 0.001);
        }
        for (int q = 0; q < 50; ++q) {
            days.push_back(gap(random) * 60.0 - 100.0);
        }
        for (const double queryDay : days) {
            if (data.evaluate(Cell{0, 0, 0.0, 0.0}, queryDay)[0] != valueByWalking(segments, queryDay)) {
                ++differences;
            }
        }
    }
    check(differences == 0, "the binary search agrees with walking from the first segment (" +
                                std::to_string(differences) + " differences)");
}

}  // namespace

int main() {
    check_values_at_given_days();
    check_axes_are_independent();
    check_against_walking_from_the_first_segment();

    return snap_test::finish("ragged_grid_data_test");
}
