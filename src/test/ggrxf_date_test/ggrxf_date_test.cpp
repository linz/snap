// Standalone unit test for the dates of a GGRXF file (deformation/ggrxf_date.h), which are ISO 8601: that a time zone
// offset and a fraction of a second are applied, and that text that is not an ISO 8601 date or date-time is rejected.
// Failures are reported to stdout and the process exit code is the pass/fail signal (0 = all passed), as for the other
// test-only tools in src/test/.

#include <exception>
#include <stdexcept>
#include <string>

#include "deformation/ggrxf_date.h"
#include "test_support.h"
#include "util/datetimeparser.h"
#include "util/dateutil.h"

namespace {

using deformation::ggrxfDateToDayNumber;
using deformation::ggrxfDaysBetween;
using snap_test::check;
using snap_test::checkEqual;
using snap_test::checkThrows;

struct DateCase {
    const char* text;
    double expected;  ///< the day number that text is
};

/// How close a result must be to the day number, in days: 0.1 millisecond. A snap day number is about 2.4 million
/// days, where the spacing between doubles is about 0.04 millisecond.
constexpr double kToleranceDays = 0.0001 / 86400.0;

constexpr double kSecondsPerDay = 86400.0;

/// The day number of a date, and of the time on it given as seconds into the day.
double day_number(const int year, const int month, const int day, const double secondsIntoDay = 0.0) {
    return snap_date(year, month, day) + secondsIntoDay / kSecondsPerDay;
}

constexpr double kHour = 3600.0;

const DateCase kDateCases[] = {
    // A date, and a time on it, with no offset: taken as written.
    {"2020-01-01", day_number(2020, 1, 1)},
    {"2020-01-01T12:00:00", day_number(2020, 1, 1, 12 * kHour)},
    {"2020-01-01 06:00", day_number(2020, 1, 1, 6 * kHour)},
    {"2020-01-01T23:59:59", day_number(2020, 1, 1, 23 * kHour + 59 * 60 + 59)},
    {"2024-02-29", day_number(2024, 2, 29)},
    {"2000-02-29", day_number(2000, 2, 29)},

    // Z is UTC, so the time is as written.
    {"2020-01-01T00:00:00Z", day_number(2020, 1, 1)},
    {"2020-01-01T12:00:00Z", day_number(2020, 1, 1, 12 * kHour)},

    // An offset is how far ahead of UTC the clock is, so the time in UTC is the time written less the offset, which can
    // be on another day.
    {"2020-01-01T00:30:00+12:00", day_number(2019, 12, 31, 12.5 * kHour)},
    {"2020-01-01T12:00:00+12:00", day_number(2020, 1, 1)},
    {"2020-01-01T12:00:00-05:30", day_number(2020, 1, 1, 17.5 * kHour)},
    {"2020-12-31T20:00:00-05:00", day_number(2021, 1, 1, 1 * kHour)},
    {"2020-01-01T12:00:00+05", day_number(2020, 1, 1, 7 * kHour)},
    {"2020-03-01T00:00:00+01:00", day_number(2020, 2, 29, 23 * kHour)},

    // A fraction of a second is part of the time.
    {"2020-01-01T00:00:00.5", day_number(2020, 1, 1, 0.5)},
    {"2020-01-01T00:00:00.25Z", day_number(2020, 1, 1, 0.25)},
    {"2020-01-01T12:00:00.5+12:00", day_number(2020, 1, 1, 0.5)},
    {"2020-01-01T12:34:56.789", day_number(2020, 1, 1, 12 * kHour + 34 * 60 + 56.789)},
};

/// Text that is not an ISO 8601 date or date-time of the form that GGRXF files hold.
const char* const kInvalidTexts[] = {
    "",
    "not a date",
    "2020",
    "2020-13-01",
    "2020-00-10",
    "2020-01-32",
    "2023-02-29",
    "1900-02-29",
    "2020-01-01T24:00:00",
    "2020-01-01T12:60:00",
    "2020-01-01T12:00:60",
    "2020-01-01T12:00:00 junk",
    "2020-01-01Z",
    "2020-01-01T12:00:00+25:00",
    "20200101",
    "2020/01/01",
    "1 JAN 2020",
};

void check_date(const DateCase& dateCase) {
    const std::string call = std::string("ggrxfDateToDayNumber(\"") + dateCase.text + "\")";
    try {
        checkEqual(ggrxfDateToDayNumber(dateCase.text), dateCase.expected, kToleranceDays, call);
    } catch (const std::exception& error) {
        check(false, call + " threw: " + error.what());
    }
}

/// That the text is rejected with a DateTimeParseError that contains the text, and whose message does too.
void check_invalid(const std::string& text) {
    try {
        const double dayNumber = ggrxfDateToDayNumber(text);
        check(false, "ggrxfDateToDayNumber(\"" + text + "\") did not throw, and gave " + std::to_string(dayNumber));
    } catch (const datetimeparser::DateTimeParseError& error) {
        check(error.text() == text, "the error contains \"" + error.text() + "\" instead of \"" + text + "\"");
        check(std::string(error.what()).find(text) != std::string::npos,
              "the error message does not contain \"" + text + "\": " + error.what());
    } catch (const std::exception& error) {
        check(false, "ggrxfDateToDayNumber(\"" + text + "\") threw something else: " + error.what());
    }
}

/// That an invalid date is thrown as an std::invalid_argument, which is what the loading of a GGRXF model catches.
void check_invalid_argument() {
    checkThrows<std::invalid_argument>([] { ggrxfDateToDayNumber("not a date"); }, "not a date",
                                       "ggrxfDateToDayNumber(\"not a date\")");
}

void check_days_between(const std::string& start, const std::string& end, const double expected) {
    checkEqual(ggrxfDaysBetween(start, end), expected, kToleranceDays,
               "ggrxfDaysBetween(\"" + start + "\", \"" + end + "\")");
}

}  // namespace

int main() {
    for (const DateCase& dateCase : kDateCases) {
        check_date(dateCase);
    }
    for (const char* text : kInvalidTexts) {
        check_invalid(text);
    }
    check_invalid_argument();

    // The days between two dates are elapsed days, whatever offset each is written with.
    check_days_between("2020-01-01", "2020-01-02", 1.0);
    check_days_between("2020-01-01T00:30:00+12:00", "2020-01-01T12:30:00+12:00", 0.5);
    check_days_between("2020-01-01T00:00:00Z", "2020-01-02T00:00:00+12:00", 0.5);
    check_days_between("2020-01-01T00:00:00Z", "2020-01-01T00:00:00+12:00", -0.5);
    check_days_between("2020-02-28", "2020-03-01", 2.0);
    check_days_between("2019-02-28", "2019-03-01", 1.0);

    return snap_test::finish("ggrxf_date_test");
}
