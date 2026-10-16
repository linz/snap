// Standalone unit test for the validation of TimeFunction (deformation/time_function.h), in particular that a Linear
// function's startDate, when it has one, must equal its functionReferenceDate, as the GGRXF format requires. Failures
// are reported to stdout and the process exit code is the pass/fail signal (0 = all passed), as for the other test-only
// tools in src/test/.

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "deformation/time_function.h"

namespace {

using deformation::TimeFunction;
using deformation::TimeFunctionShape;

int failures = 0;

void check(const bool condition, const std::string& description) {
    if (condition) {
        return;
    }
    ++failures;
    std::cout << "FAIL: " << description << "\n";
}

/// \return whether making a TimeFunction from the params throws an std::invalid_argument.
bool constructionThrows(const TimeFunction::Params& params) {
    try {
        const TimeFunction function(params);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

TimeFunction::Params linearParams(const double scaleFactor) {
    TimeFunction::Params params;
    params.shape = TimeFunctionShape::Linear;
    params.scaleFactor = scaleFactor;
    return params;
}

void check_linear_without_a_start_date() {
    TimeFunction::Params params = linearParams(2.0);
    params.functionReferenceDateDays = 100.0;
    const TimeFunction function(params);
    check(function.evaluate(110.0) == 20.0, "a linear function after its reference date");
    check(function.evaluate(90.0) == -20.0,
          "a linear function with no start date is unbounded before its reference date");
}

void check_linear_with_a_start_date_equal_to_the_reference_date() {
    TimeFunction::Params params = linearParams(2.0);
    params.functionReferenceDateDays = 100.0;
    params.startDateDays = 100.0;
    const TimeFunction function(params);
    check(function.evaluate(110.0) == 20.0, "a linear function with an equal start date, after it");
    check(function.evaluate(90.0) == 0.0, "a linear function with an equal start date is zero before it");
}

void check_linear_with_a_different_start_date_is_rejected() {
    TimeFunction::Params params = linearParams(1.0);
    params.functionReferenceDateDays = 0.0;
    params.startDateDays = 10.0;
    check(constructionThrows(params), "a linear function whose start date is after its reference date");

    params.startDateDays = -10.0;
    check(constructionThrows(params), "a linear function whose start date is before its reference date");

    params.functionReferenceDateDays = std::nullopt;
    check(constructionThrows(params), "a linear function with no reference date");
}

void check_other_shapes_are_not_affected() {
    TimeFunction::Params step;
    step.shape = TimeFunctionShape::Step;
    step.eventDateDays = 50.0;
    step.startDateDays = 3.0;  // is not used by a step, and is not compared with anything
    check(!constructionThrows(step), "a step function is not affected by its start date");

    TimeFunction::Params ramp;
    ramp.shape = TimeFunctionShape::Ramp;
    ramp.startDateDays = 10.0;
    ramp.endDateDays = 20.0;
    ramp.functionReferenceDateDays = 0.0;  // a ramp's functionReferenceDate is not its start date
    check(!constructionThrows(ramp), "a ramp function may have a reference date that is not its start date");
}

}  // namespace

int main() {
    check_linear_without_a_start_date();
    check_linear_with_a_start_date_equal_to_the_reference_date();
    check_linear_with_a_different_start_date_is_rejected();
    check_other_shapes_are_not_affected();

    if (failures > 0) {
        std::cout << failures << " time_function_test check(s) failed\n";
        return 1;
    }
    std::cout << "time_function_test: all checks passed\n";
    return 0;
}
