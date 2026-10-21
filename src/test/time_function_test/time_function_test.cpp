// Standalone unit test for TimeFunction (deformation/time_function.h): the validation of its parameters, in particular
// that a Linear function's startDate, when it has one, must equal its functionReferenceDate, as the GGRXF format
// requires; the values of the Linear, Step and Ramp shapes; and the effect of a functionReferenceDate. Failures are
// reported to stdout and the process exit code is the pass/fail signal (0 = all passed), as for the other test-only
// tools in src/test/.

#include <array>
#include <optional>
#include <stdexcept>
#include <string>

#include "deformation/time_function.h"
#include "test_support.h"

namespace {

using deformation::TimeFunction;
using deformation::TimeFunctionShape;
using snap_test::check;
using snap_test::checkThrows;

/// Makes a TimeFunction from the params, which throws if they are not valid.
void construct(const TimeFunction::Params& params) {
    const TimeFunction function(params);
}

/// \return whether making a TimeFunction from the params throws an std::invalid_argument.
bool constructionThrows(const TimeFunction::Params& params) {
    try {
        construct(params);
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

void check_step_evaluation() {
    TimeFunction::Params params;
    params.shape = TimeFunctionShape::Step;
    params.scaleFactor = 3.0;
    params.eventDateDays = 50.0;
    const TimeFunction function(params);
    check(function.evaluate(49.0) == 0.0, "a step function before its event date");
    check(function.evaluate(50.0) == 3.0, "a step function on its event date has made its whole jump");
    check(function.evaluate(60.0) == 3.0, "a step function after its event date");
}

void check_ramp_evaluation() {
    TimeFunction::Params params;
    params.shape = TimeFunctionShape::Ramp;
    params.scaleFactor = 2.0;
    params.startDateDays = 10.0;
    params.endDateDays = 20.0;
    const TimeFunction function(params);
    check(function.evaluate(5.0) == 0.0, "a ramp function before its start date");
    check(function.evaluate(10.0) == 0.0, "a ramp function on its start date");
    check(function.evaluate(15.0) == 1.0, "a ramp function half way through its transition");
    check(function.evaluate(20.0) == 2.0, "a ramp function on its end date has made its whole transition");
    check(function.evaluate(25.0) == 2.0, "a ramp function after its end date");
}

// The format's ramp is 0 if Δ < 0, Δ/Δ_end if 0 <= Δ < Δ_end, and 1 if Δ >= Δ_end, with Δ_end = endDate - startDate.
// When the end date is the start date, no Δ is in the middle row, so the ramp is a step on its start date.
void check_zero_length_ramp_is_a_step() {
    TimeFunction::Params params;
    params.shape = TimeFunctionShape::Ramp;
    params.scaleFactor = 2.0;
    params.startDateDays = 10.0;
    params.endDateDays = 10.0;
    const TimeFunction function(params);
    check(function.evaluate(9.0) == 0.0, "a ramp with no length before its start date");
    check(function.evaluate(10.0) == 2.0, "a ramp with no length on its start date has made its whole transition");
    check(function.evaluate(11.0) == 2.0, "a ramp with no length after its start date");
}

// A functionReferenceDate (R) chooses the baseline that the values are measured from. The value that the shape has on
// R is subtracted from every value, so that the result is zero on R, and the result is then scaled:
//     scale * (shape(t) - shape(R))
// The format calls this the zeroing subtraction. R does not move the shape's own transition or change its size.
//
// Take a step of size S at time T. The shape is 0 before T and 1 from T on, so the values are:
//
//                   t < T    t >= T
//     no R            0        S
//     R before T      0        S     shape(R) is 0, so nothing is subtracted
//     R == T         -S        0     shape(R) is 1, so 1 is subtracted from every value
//     R after T      -S        0     shape(R) is 1, as for R == T
//
// In every row the function rises by S at T and never falls as t increases. R only moves where the zero is.
//
// A linear function follows the same rule, which is why its values are negative before its reference date.
void check_function_reference_date_moves_the_zero() {
    TimeFunction::Params step;
    step.shape = TimeFunctionShape::Step;
    step.scaleFactor = 2.0;
    step.eventDateDays = 50.0;  // T

    step.functionReferenceDateDays = 40.0;  // R before T
    const TimeFunction before(step);
    check(before.evaluate(40.0) == 0.0, "a step function on its reference date, which is before its event date");
    check(before.evaluate(70.0) == 2.0, "a step function after its event date, with a reference date before it");

    step.functionReferenceDateDays = 50.0;  // R == T
    const TimeFunction on(step);
    check(on.evaluate(40.0) == -2.0, "a step function before its event date, with a reference date on it");
    check(on.evaluate(50.0) == 0.0, "a step function on its event date, which is its reference date");
    check(on.evaluate(70.0) == 0.0, "a step function after its event date, with a reference date on it");

    step.functionReferenceDateDays = 60.0;  // R after T
    const TimeFunction after(step);
    check(after.evaluate(40.0) == -2.0, "a step function before its event date, with a reference date after it");
    check(after.evaluate(55.0) == 0.0, "a step function between its event date and its reference date");
    check(after.evaluate(60.0) == 0.0, "a step function on its reference date, which is after its event date");

    // Half way through the transition of a ramp the shape has the value 0.5, so that is what is subtracted.
    TimeFunction::Params ramp;
    ramp.shape = TimeFunctionShape::Ramp;
    ramp.startDateDays = 10.0;
    ramp.endDateDays = 20.0;
    ramp.functionReferenceDateDays = 15.0;
    const TimeFunction rampFunction(ramp);
    check(rampFunction.evaluate(10.0) == -0.5, "a ramp function before its reference date");
    check(rampFunction.evaluate(15.0) == 0.0, "a ramp function on its reference date");
    check(rampFunction.evaluate(20.0) == 0.5, "a ramp function after its reference date");
}

void check_missing_parameters_are_rejected() {
    TimeFunction::Params step;
    step.shape = TimeFunctionShape::Step;
    checkThrows<std::invalid_argument>([&step] { construct(step); }, "eventDateDays",
                                       "a step function with no event date");

    TimeFunction::Params ramp;
    ramp.shape = TimeFunctionShape::Ramp;
    checkThrows<std::invalid_argument>([&ramp] { construct(ramp); }, "startDateDays",
                                       "a ramp function with no start date and no end date");
    ramp.startDateDays = 10.0;
    checkThrows<std::invalid_argument>([&ramp] { construct(ramp); }, "endDateDays",
                                       "a ramp function with no end date");
    ramp.startDateDays = std::nullopt;
    ramp.endDateDays = 20.0;
    checkThrows<std::invalid_argument>([&ramp] { construct(ramp); }, "startDateDays",
                                       "a ramp function with no start date");
}

void check_unimplemented_shapes_are_rejected() {
    struct Shape {
        TimeFunctionShape shape;
        std::string name;
    };
    const std::array<Shape, 6> unimplemented = {{{TimeFunctionShape::Quadratic, "Quadratic"},
                                                 {TimeFunctionShape::Exponential, "Exponential"},
                                                 {TimeFunctionShape::LogBaseE, "LogBaseE"},
                                                 {TimeFunctionShape::LogBase10, "LogBase10"},
                                                 {TimeFunctionShape::Cyclic, "Cyclic"},
                                                 {TimeFunctionShape::HyperbolicTangent, "HyperbolicTangent"}}};
    for (const Shape& entry : unimplemented) {
        TimeFunction::Params params;
        params.shape = entry.shape;
        checkThrows<std::invalid_argument>([&params] { construct(params); }, entry.name,
                                           "the " + entry.name + " shape names itself when rejected");
    }

    // A value that is none of the named shapes, which the message calls "unknown".
    TimeFunction::Params params;
    params.shape = static_cast<TimeFunctionShape>(99);
    checkThrows<std::invalid_argument>([&params] { construct(params); }, "unknown",
                                       "a shape that is not one of the named shapes");
}

}  // namespace

int main() {
    check_linear_without_a_start_date();
    check_linear_with_a_start_date_equal_to_the_reference_date();
    check_linear_with_a_different_start_date_is_rejected();
    check_other_shapes_are_not_affected();
    check_step_evaluation();
    check_ramp_evaluation();
    check_zero_length_ramp_is_a_step();
    check_function_reference_date_moves_the_zero();
    check_missing_parameters_are_rejected();
    check_unimplemented_shapes_are_rejected();

    return snap_test::finish("time_function_test");
}
