#pragma once

#include <cstdint>
#include <optional>

namespace deformation {

// The named time-function shapes the GGRXF specification defines. Every
// shape is evaluated on Δ, the real day-count difference between the query
// date and that shape's own required date parameter (see TimeFunction::
// Params below for which parameter that is per shape) - never a
// calendar-year convention: LINZDEF's own utlDateAsYear turned out to
// already disagree with both an exact-calendar-year convention and a fixed-
// 365.2425-day-year convention, so real day counts avoid needing to pick a
// year-length convention at all.
//
// TimeFunction currently only implements Linear, Step, and Ramp - the only
// three any real component in the existing shipped deformation model needs.
// The other six are named here because a parser must still be able to
// identify and reject them cleanly by name; TimeFunction's constructor
// throws for any of them (see TimeFunction::TimeFunction).
enum class TimeFunctionShape : std::uint8_t {
    Linear = 0,             // implemented - unbounded rate through zero, or delayed onset if startDate is set
    Step = 1,               // implemented - instantaneous jump at eventDate
    Ramp = 2,               // implemented - bounded linear transition between startDate and endDate
    Quadratic = 3,          // not implemented - TimeFunction's constructor throws
    Exponential = 4,        // not implemented - TimeFunction's constructor throws
    LogBaseE = 5,           // not implemented - TimeFunction's constructor throws
    LogBase10 = 6,          // not implemented - TimeFunction's constructor throws
    Cyclic = 7,             // not implemented - TimeFunction's constructor throws
    HyperbolicTangent = 8,  // not implemented - TimeFunction's constructor throws
};

// One named time function: a shape (see TimeFunctionShape), its own
// required date parameter(s), and a scale factor. evaluate() computes that
// shape's own reference function of Δ, scaled, with an optional zeroing
// subtraction - see evaluate() below for exactly what that means.
class TimeFunction {
 public:
    // Every date field here is a day count relative to whatever epoch the
    // caller chooses - the caller converts real calendar dates to day
    // counts once, when building a Params, not per query; TimeFunction
    // itself has no notion of calendar dates at all.
    struct Params {
        TimeFunctionShape shape = TimeFunctionShape::
            Linear;  // which named shape refFunc() computes; only Linear/Step/Ramp are currently implemented

        double scaleFactor =
            1.0;  // multiplies refFunc(Δ); days-denominated for Linear (a rate per day), a dimensionless amplitude for every other shape

        std::optional<double>
            startDateDays;  // required by Ramp (the transition's own start); optionally marks Linear's own delayed onset
        std::optional<double> endDateDays;  // required by Ramp (the transition's own end)
        std::optional<double>
            eventDateDays;  // required by Step (its own instantaneous jump point), and by the not-yet-implemented Exponential/LogBaseE/LogBase10/HyperbolicTangent shapes

        // Independently optional on every shape, including the currently
        // unimplemented ones: when set, evaluate() returns refFunc(Δ) -
        // refFunc(Δ at this date) instead of refFunc(Δ) alone. Also doubles
        // as Linear's and Quadratic's own required date parameter (the date
        // Δ itself is measured from), since those two shapes have no other
        // required date.
        std::optional<double> functionReferenceDateDays;

        std::optional<double>
            timeConstantDays;  // required by the not-yet-implemented Exponential/LogBaseE/LogBase10/HyperbolicTangent shapes
        std::optional<double> frequencyPerDay;  // required by the not-yet-implemented Cyclic shape
    };

    // Validates params eagerly, at construction, rather than lazily inside
    // evaluate() - a file declaring an unsupported or malformed time
    // function should fail as soon as it's read, not on the first query
    // that happens to reach it.
    explicit TimeFunction(
        const Params&
            params);  // copied - no heap-owning members, no move benefit over a copy. Throws std::invalid_argument if params.shape is not Linear/Step/Ramp, or if params is missing a date/parameter its own shape requires

    // Evaluates this shape's own reference function at queryDateDays: Δ is
    // computed relative to this shape's own required date parameter (see
    // Params above), refFunc(Δ) is scaled by scaleFactor, and if
    // functionReferenceDateDays is set, refFunc(Δ at functionReferenceDateDays)
    // is subtracted from the result first.
    double evaluate(
        double queryDateDays) const;  // query date, as a day count in the same epoch as every Params field above

 private:
    // Δ at dateDays, relative to this shape's own required date parameter
    // (functionReferenceDateDays for Linear, eventDateDays for Step,
    // startDateDays for Ramp).
    double deltaAt(double dateDays) const;  // a day count in the same epoch as every Params field above

    // This shape's own refFunc(delta) - the per-shape formula from the
    // GGRXF specification's named time-function shapes table, before
    // scaleFactor or the functionReferenceDate zeroing subtraction are
    // applied.
    double refFunc(double delta) const;  // delta is deltaAt()'s own return value - a day count difference, not a date

    Params params_;  // this function's own shape and parameters, as validated by the constructor
};

}  // namespace deformation
