// Standalone unit test for datetimeparser (util/datetimeparser.h): the parts that a date and time are read into by a
// format, as ISO 8601, and as a decimal year, the reason that text could not be read (what was wrong, where, and the
// message that says so), and month names. The functions that do not throw are tested in detail, and those that throw,
// which call them, once for each way that text can be wrong. Failures are reported to stdout and the process exit code
// is the pass/fail signal (0 = all passed), as for the other test-only tools in src/test/.

#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "util/datetimeparser.h"
#include "util/rangedint.h"

namespace {

using datetimeparser::DateTimeParseError;
using datetimeparser::DateTimeParts;
using datetimeparser::DayOfMonth;
using datetimeparser::DayOfYear;
using datetimeparser::Hour;
using datetimeparser::Minute;
using datetimeparser::Month;
using datetimeparser::MonthAndDay;
using datetimeparser::ParseError;
using datetimeparser::ParseErrorKind;
using datetimeparser::ParseResult;
using datetimeparser::Second;
using datetimeparser::UtcOffsetMinutes;
using datetimeparser::Year;

/// How close a fraction of a second or a decimal year must be to what is expected.
constexpr double kTolerance = 1.0e-12;

int failures = 0;

void check(const bool condition, const std::string& description) {
    if (condition) {
        return;
    }
    ++failures;
    std::cout << "FAIL: " << description << "\n";
}

void checkNumber(const std::string& label, const std::string& what, const long long actual, const long long expected) {
    check(actual == expected,
          label + ": the " + what + " is " + std::to_string(actual) + " instead of " + std::to_string(expected));
}

/// The parts of a date with a month and day, and the time on it if it has one.
DateTimeParts monthAndDayParts(const int year, const int month, const int day, const int hour = 0, const int minute = 0,
                               const int second = 0, const double fractionOfSecond = 0.0,
                               const std::optional<int>& offsetMinutes = std::nullopt) {
    std::optional<UtcOffsetMinutes> offset;
    if (offsetMinutes) {
        offset.emplace(*offsetMinutes);
    }
    return DateTimeParts(Year(year), MonthAndDay(Month(month), DayOfMonth(day)), Hour(hour), Minute(minute),
                         Second(second), fractionOfSecond, offset);
}

/// The parts of a date with a day of the year, and the time on it if it has one.
DateTimeParts dayOfYearParts(const int year, const int dayOfYear, const int hour = 0, const int minute = 0) {
    return DateTimeParts(Year(year), DayOfYear(dayOfYear), Hour(hour), Minute(minute));
}

/// Checks that parts are the parts expected.
void checkParts(const std::string& label, const DateTimeParts& actual, const DateTimeParts& expected) {
    checkNumber(label, "year", actual.year.value, expected.year.value);
    if (actual.day.index() != expected.day.index()) {
        check(false, label + " has " +
                         (expected.day.index() == 0 ? "a day of the year, instead of a month and day"
                                                    : "a month and day, instead of a day of the year"));
    } else if (const auto* expectedMonthAndDay = std::get_if<MonthAndDay>(&expected.day)) {
        const MonthAndDay& actualMonthAndDay = std::get<MonthAndDay>(actual.day);
        checkNumber(label, "month", actualMonthAndDay.month.value, expectedMonthAndDay->month.value);
        checkNumber(label, "day", actualMonthAndDay.day.value, expectedMonthAndDay->day.value);
    } else {
        checkNumber(label, "day of the year", std::get<DayOfYear>(actual.day).value,
                    std::get<DayOfYear>(expected.day).value);
    }
    checkNumber(label, "hour", actual.hour.value, expected.hour.value);
    checkNumber(label, "minute", actual.minute.value, expected.minute.value);
    checkNumber(label, "second", actual.second.value, expected.second.value);
    check(std::fabs(actual.fractionOfSecond - expected.fractionOfSecond) <= kTolerance,
          label + ": the fraction of a second is " + std::to_string(actual.fractionOfSecond) + " instead of " +
              std::to_string(expected.fractionOfSecond));
    if (!expected.offset) {
        check(!actual.offset, label + ": has a time zone, which it should not have");
    } else if (!actual.offset) {
        check(false, label + ": has no time zone, instead of " + std::to_string(expected.offset->value));
    } else {
        checkNumber(label, "time zone", actual.offset->value, expected.offset->value);
    }
}

/// Checks that a result is one that read the text, into the parts expected.
void checkRead(const std::string& label, const std::string& text, const ParseResult<DateTimeParts>& result,
               const DateTimeParts& expected) {
    if (!result.succeeded()) {
        check(false, label + " could not read the text: " + describe(*result.error(), text));
        return;
    }
    checkParts(label, result.value(), expected);
}

/// Text that is read by a format, and the parts that it gives.
struct ReadCase {
    std::string text;
    std::string format;
    DateTimeParts parts;
};

void checkReadCases(const std::vector<ReadCase>& cases) {
    for (const ReadCase& readCase : cases) {
        const std::string label = "tryParse(\"" + readCase.text + "\", \"" + readCase.format + "\")";
        checkRead(label, readCase.text, datetimeparser::tryParse(readCase.text, readCase.format), readCase.parts);
    }
}

/// Text that can not be read, with the format to read it by if it is read by one, why not, and pieces that the message
/// that says why must contain.
struct ErrorCase {
    std::string text;
    std::string format;
    ParseError error;
    std::vector<std::string> messageParts;
};

/// Checks that a result is a failure, for the reason expected, and that its message contains the pieces expected.
template <typename Value>
void checkError(const std::string& label, const ParseResult<Value>& result, const ErrorCase& expected) {
    if (result.succeeded()) {
        check(false, label + " read the text, instead of failing");
        return;
    }
    if (!result.error()) {
        check(false, label + " failed, and has no error");
        return;
    }
    const ParseError& error = *result.error();
    checkNumber(label, "kind of error", static_cast<int>(error.kind), static_cast<int>(expected.error.kind));
    checkNumber(label, "field", error.field, expected.error.field);
    checkNumber(label, "position", static_cast<long long>(error.position),
                static_cast<long long>(expected.error.position));
    checkNumber(label, "length", static_cast<long long>(error.length), static_cast<long long>(expected.error.length));
    checkNumber(label, "value", error.value, expected.error.value);
    checkNumber(label, "minimum", error.minimum, expected.error.minimum);
    checkNumber(label, "maximum", error.maximum, expected.error.maximum);

    const std::string message = describe(error, expected.text);
    for (const std::string& part : expected.messageParts) {
        check(message.find(part) != std::string::npos,
              label + ": the message does not contain \"" + part + "\": " + message);
    }
}

void checkFormats() {
    // The fields can be in any order that a format gives, with any characters between them, or none.
    checkReadCases({
        {"2024-03-15", "YMD", monthAndDayParts(2024, 3, 15)},
        {"2024-03-15", "YMDhms", monthAndDayParts(2024, 3, 15)},
        {"2024-03-15 12:34:56", "YMDhms", monthAndDayParts(2024, 3, 15, 12, 34, 56)},
        {"2024-03-15T12:34:56", "YMDhms", monthAndDayParts(2024, 3, 15, 12, 34, 56)},
        {"2024-03-15 12:34", "YMDhm", monthAndDayParts(2024, 3, 15, 12, 34)},
        {"2024-03-15 12", "YMDh", monthAndDayParts(2024, 3, 15, 12)},
        {"2024-03-15 23:59:59", "YMDhms", monthAndDayParts(2024, 3, 15, 23, 59, 59)},
        {"15/03/2024", "DMY", monthAndDayParts(2024, 3, 15)},
        {"15-03-2024 07:08:09", "DMYhms", monthAndDayParts(2024, 3, 15, 7, 8, 9)},
        {"03/15/2024", "MDY", monthAndDayParts(2024, 3, 15)},
        {"20240315", "YMD", monthAndDayParts(2024, 3, 15)},
        {"20240315123456", "YMDhms", monthAndDayParts(2024, 3, 15, 12, 34, 56)},
        {"2024-3-5", "YMD", monthAndDayParts(2024, 3, 5)},
        {"2024.03.15", "YMD", monthAndDayParts(2024, 3, 15)},
        {"2024 03 15", "YMD", monthAndDayParts(2024, 3, 15)},
        {"2024-03-15", "Y M D", monthAndDayParts(2024, 3, 15)},
        {" 2024-03-15 ", "YMD", monthAndDayParts(2024, 3, 15)},
        {"1000-01-01", "YMD", monthAndDayParts(1000, 1, 1)},
        {"4000-12-31", "YMD", monthAndDayParts(4000, 12, 31)},
        {"2024-02-29", "YMD", monthAndDayParts(2024, 2, 29)},
        {"2000-02-29", "YMD", monthAndDayParts(2000, 2, 29)},
        {"2023-02-28", "YMD", monthAndDayParts(2023, 2, 28)},
        {"2024-04-30", "YMD", monthAndDayParts(2024, 4, 30)},
    });
}

void checkMonthNamesInText() {
    // A month can be a name, which is not case sensitive, and is matched on its first three letters.
    checkReadCases({
        {"15 MAR 2024", "DMY", monthAndDayParts(2024, 3, 15)},
        {"15 mar 2024", "DMY", monthAndDayParts(2024, 3, 15)},
        {"15 Mar 2024", "DMY", monthAndDayParts(2024, 3, 15)},
        {"15 March 2024", "DMY", monthAndDayParts(2024, 3, 15)},
        {"15-Mar-2024", "DMY", monthAndDayParts(2024, 3, 15)},
        {"15MAR2024", "DMY", monthAndDayParts(2024, 3, 15)},
        {"2024 MAR 15", "YMD", monthAndDayParts(2024, 3, 15)},
        {"1 DEC 1998", "DMY", monthAndDayParts(1998, 12, 1)},
        {"19 AUGUST 2003", "DMY", monthAndDayParts(2003, 8, 19)},
        {"19 Augst 2003", "DMY", monthAndDayParts(2003, 8, 19)},
        {"25 dec 2011", "DMY", monthAndDayParts(2011, 12, 25)},
        {"25 dec 2011 7 15", "DMYhm", monthAndDayParts(2011, 12, 25, 7, 15)},
    });
}

void checkDaysOfTheYear() {
    // The format letter N is the day of the year, in place of the month and day.
    checkReadCases({
        {"2024.075", "YN", dayOfYearParts(2024, 75)},
        {"2024 366", "YN", dayOfYearParts(2024, 366)},
        {"2023 365", "YN", dayOfYearParts(2023, 365)},
        {"2010.001", "YN", dayOfYearParts(2010, 1)},
        {"2010.249 00.15", "YNhm", dayOfYearParts(2010, 249, 0, 15)},
        {"2010.001 12:30", "YNhm", dayOfYearParts(2010, 1, 12, 30)},
    });
}

void checkFractionsAndZones() {
    // A fraction of a second, and a time zone, may follow the seconds, when a format ends with the seconds.
    checkReadCases({
        {"2024-03-15 12:34:56.789", "YMDhms", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.789)},
        {"2024-03-15 12:34:56,5", "YMDhms", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.5)},
        {"2024-03-15T12:34:56.25Z", "YMDhms", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.25, 0)},
        {"2024-03-15T12:34:56Z", "YMDhms", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.0, 0)},
        {"2024-03-15T12:34:56+12:00", "YMDhms", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.0, 720)},
        {"2024-03-15T12:34:56-05:30", "YMDhms", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.0, -330)},
        {"2024-03-15T12:34:56+1200", "YMDhms", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.0, 720)},
        {"2024-03-15T12:34:56-0530", "YMDhms", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.0, -330)},
        {"2024-03-15T12:34:56+05", "YMDhms", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.0, 300)},
        {"2024-03-15T12:34:56.5+12:00", "YMDhms", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.5, 720)},
        {"2024-03-15T12:34:56+23:59", "YMDhms", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.0, 1439)},
        {"2024-03-15T12:34:56-23:59", "YMDhms", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.0, -1439)},
        {"2024-03-15T12:34:56+00:00", "YMDhms", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.0, 0)},
    });

    // At most 15 digits of a fraction are used, so that a long run of 9s is not rounded up to 1.
    const auto longFraction = datetimeparser::tryParse("2024-03-15 12:34:56.99999999999999999999", "YMDhms");
    check(longFraction.succeeded() && longFraction.value().fractionOfSecond < 1.0,
          "a fraction of a second of 20 nines is not less than 1");
    const auto manyDigits = datetimeparser::tryParse("2024-03-15 12:34:56.1234567890123456789", "YMDhms");
    check(manyDigits.succeeded() && std::fabs(manyDigits.value().fractionOfSecond - 0.123456789012345) <= 1.0e-15,
          "a fraction of a second of 19 digits is not its first 15");
}

void checkIso8601() {
    // The extended form: yyyy-mm-dd, then T or a space and hh:mm, hh:mm:ss or hh:mm:ss.s, then a time zone.
    struct IsoCase {
        std::string text;
        DateTimeParts parts;
    };
    const std::vector<IsoCase> cases = {
        {"2024-03-15", monthAndDayParts(2024, 3, 15)},
        {"2024-03-15T12:34", monthAndDayParts(2024, 3, 15, 12, 34)},
        {"2024-03-15T12:34:56", monthAndDayParts(2024, 3, 15, 12, 34, 56)},
        {"2024-03-15 12:34:56", monthAndDayParts(2024, 3, 15, 12, 34, 56)},
        {"2024-03-15T00:00:00", monthAndDayParts(2024, 3, 15)},
        {"2024-03-15T23:59:59", monthAndDayParts(2024, 3, 15, 23, 59, 59)},
        {"2024-03-15T12:34:56.789", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.789)},
        {"2024-03-15T12:34:56Z", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.0, 0)},
        {"2024-03-15T12:34Z", monthAndDayParts(2024, 3, 15, 12, 34, 0, 0.0, 0)},
        {"2024-03-15T12:34+01:00", monthAndDayParts(2024, 3, 15, 12, 34, 0, 0.0, 60)},
        {"2024-03-15T12:34:56+12:00", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.0, 720)},
        {"2024-03-15T12:34:56-05:30", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.0, -330)},
        {"2024-03-15T12:34:56+05", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.0, 300)},
        {"2024-03-15T12:34:56.5-05:00", monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.5, -300)},
        {"2024-02-29T00:00:00Z", monthAndDayParts(2024, 2, 29, 0, 0, 0, 0.0, 0)},
        {"1000-01-01", monthAndDayParts(1000, 1, 1)},
        {"4000-12-31", monthAndDayParts(4000, 12, 31)},
    };
    for (const IsoCase& isoCase : cases) {
        const std::string label = "tryParseIso8601(\"" + isoCase.text + "\")";
        checkRead(label, isoCase.text, datetimeparser::tryParseIso8601(isoCase.text), isoCase.parts);
    }

    const auto longFraction = datetimeparser::tryParseIso8601("2024-03-15T12:34:56.99999999999999999999");
    check(longFraction.succeeded() && longFraction.value().fractionOfSecond < 1.0,
          "an ISO 8601 fraction of a second of 20 nines is not less than 1");
}

void checkIso8601IsStrict() {
    // Text that tryParse reads, but that is not ISO 8601 in its extended form, is not read.
    for (const char* text :
         {" 2024-03-15", "2024-03-15 ", "2024-3-15", "2024-03-5", "20240315", "2024/03/15", "2024.03.15", "2024 03 15",
          "15-03-2024", "15 MAR 2024", "2024-MAR-15", "2024-03-15T1234", "2024-03-15T12:34:56,5",
          "2024-03-15T12:34:56+0530", "2024-03-15T12:34:56 +05:30", "2024-03-15t12:34", "2024-03-15 junk"}) {
        check(!datetimeparser::tryParseIso8601(text).succeeded(),
              std::string("tryParseIso8601(\"") + text + "\") read the text");
    }
}

std::vector<ErrorCase> formatErrorCases() {
    using Kind = ParseErrorKind;
    return {
        // The text ended before a field.
        {"", "YMD", ParseError(Kind::EndOfText, 'Y', 0, 0), {"ended", "year"}},
        {"2024-01", "YMD", ParseError(Kind::EndOfText, 'D', 7, 0), {"ended", "day", "\"2024-01\""}},

        // A number outside the limits of its field.
        {"2024-13-01",
         "YMD",
         ParseError(Kind::OutOfRange, 'M', 5, 2, 13, 1, 12),
         {"month 13", "1 to 12", "character 5", "\"2024-13-01\""}},
        {"2024-00-10", "YMD", ParseError(Kind::OutOfRange, 'M', 5, 2, 0, 1, 12), {"month 0", "1 to 12"}},
        {"2024-01-32", "YMD", ParseError(Kind::OutOfRange, 'D', 8, 2, 32, 1, 31), {"day 32", "1 to 31"}},
        {"2024-01-00", "YMD", ParseError(Kind::OutOfRange, 'D', 8, 2, 0, 1, 31), {"day 0", "1 to 31"}},
        {"0999-01-01",
         "YMD",
         ParseError(Kind::OutOfRange, 'Y', 0, 4, 999, 1000, 4000),
         {"year 999", "1000 to 4000", "character 0"}},
        {"4001-01-01", "YMD", ParseError(Kind::OutOfRange, 'Y', 0, 4, 4001, 1000, 4000), {"year 4001"}},
        {"2024-01-01 24:00:00",
         "YMDhms",
         ParseError(Kind::OutOfRange, 'h', 11, 2, 24, 0, 23),
         {"hour 24", "0 to 23", "character 11"}},
        {"2024-01-01 12:60:00",
         "YMDhms",
         ParseError(Kind::OutOfRange, 'm', 14, 2, 60, 0, 59),
         {"minute 60", "0 to 59", "character 14"}},
        {"2024-01-01 12:00:60",
         "YMDhms",
         ParseError(Kind::OutOfRange, 's', 17, 2, 60, 0, 59),
         {"second 60", "0 to 59", "character 17"}},
        {"2024 367",
         "YN",
         ParseError(Kind::OutOfRange, 'N', 5, 3, 367, 1, 366),
         {"day of the year 367", "1 to 366", "character 5"}},
        {"2024 000", "YN", ParseError(Kind::OutOfRange, 'N', 5, 3, 0, 1, 366), {"day of the year 0"}},
        {"2023 366", "YN", ParseError(Kind::OutOfRange, 'N', 5, 3, 366, 1, 365), {"day of the year 366", "1 to 365"}},
        {"2100 366", "YN", ParseError(Kind::OutOfRange, 'N', 5, 3, 366, 1, 365), {"1 to 365"}},

        // A day that its month does not have.
        {"2023-02-29",
         "YMD",
         ParseError(Kind::NotInMonth, 'D', 8, 2, 29, 1, 28),
         {"day 29", "28 days", "character 8", "\"2023-02-29\""}},
        {"1900-02-29", "YMD", ParseError(Kind::NotInMonth, 'D', 8, 2, 29, 1, 28), {"28 days"}},
        {"2024-02-30", "YMD", ParseError(Kind::NotInMonth, 'D', 8, 2, 30, 1, 29), {"day 30", "29 days"}},
        {"2024-04-31", "YMD", ParseError(Kind::NotInMonth, 'D', 8, 2, 31, 1, 30), {"day 31", "30 days"}},
        {"31/06/2024", "DMY", ParseError(Kind::NotInMonth, 'D', 0, 2, 31, 1, 30), {"30 days", "character 0"}},

        // Letters where a month is that are not the name of one.
        {"15 XYZ 2024",
         "DMY",
         ParseError(Kind::UnknownMonthName, 'M', 3, 3),
         {"\"XYZ\"", "name of a month", "character 3"}},
        {"15 JA 2024", "DMY", ParseError(Kind::UnknownMonthName, 'M', 3, 2), {"\"JA\"", "name of a month"}},

        // Text left after the end of the date and time.
        {"2024-01-01 junk",
         "YMDhms",
         ParseError(Kind::TrailingText, '\0', 11, 4),
         {"after the end", "\"junk\"", "character 11"}},
        {"2024-01-01x", "YMD", ParseError(Kind::TrailingText, '\0', 10, 1), {"\"x\""}},
        {"2024-01-01 12:00:00 extra", "YMDhms", ParseError(Kind::TrailingText, '\0', 20, 5), {"\"extra\""}},
        {"2024-01-01Z", "YMDhms", ParseError(Kind::TrailingText, '\0', 10, 1), {"\"Z\""}},
        {"2024-01-01 12:34Z", "YMDhm", ParseError(Kind::TrailingText, '\0', 16, 1), {"\"Z\""}},
        {"2024-01-01 12:34:56.5", "YMDhm", ParseError(Kind::TrailingText, '\0', 16, 5), {"\":56.5\""}},

        // Punctuation after the date, or after a part of the time, with no number after it, is text left over.
        {"2024-01-01!!!", "YMDhms", ParseError(Kind::TrailingText, '\0', 10, 3), {"\"!!!\"", "character 10"}},
        {"2024-01-01,", "YMDhms", ParseError(Kind::TrailingText, '\0', 10, 1), {"\",\""}},
        {"2024-01-01 12:", "YMDhms", ParseError(Kind::TrailingText, '\0', 13, 1), {"\":\"", "character 13"}},
        {"2024-01-01T12:34:", "YMDhms", ParseError(Kind::TrailingText, '\0', 16, 1), {"\":\"", "character 16"}},

        // A fraction of a second or a time zone that is not complete.
        {"2024-01-01 12:00:00.", "YMDhms", ParseError(Kind::EndOfText, 'f', 20, 0), {"fraction of a second"}},
        {"2024-01-01 12:00:00.x",
         "YMDhms",
         ParseError(Kind::UnexpectedCharacter, 'f', 20, 1),
         {"fraction of a second", "character 20"}},
        {"2024-01-01 12:00:00+", "YMDhms", ParseError(Kind::EndOfText, 'z', 20, 0), {"time zone"}},
        {"2024-01-01 12:00:00+1x",
         "YMDhms",
         ParseError(Kind::UnexpectedCharacter, 'z', 21, 1),
         {"time zone", "character 21"}},
        {"2024-01-01 12:00:00+24:00",
         "YMDhms",
         ParseError(Kind::OutOfRange, 'z', 20, 2, 24, 0, 23),
         {"time zone 24", "0 to 23", "character 20"}},
        {"2024-01-01 12:00:00+05:60",
         "YMDhms",
         ParseError(Kind::OutOfRange, 'z', 23, 2, 60, 0, 59),
         {"time zone 60", "0 to 59", "character 23"}},

        // Without a colon, the minutes of a time zone start straight after the hours.
        {"2024-01-01T12:00:00+0560",
         "YMDhms",
         ParseError(Kind::OutOfRange, 'z', 22, 2, 60, 0, 59),
         {"time zone 60", "0 to 59", "character 22"}},

        // A format that can not be used.
        {"2024-01-01", "YMDx", ParseError(Kind::InvalidFormat, 'x', 3, 1), {"character x", "number 3"}},
        {"2024-01-01", "YYMD", ParseError(Kind::InvalidFormat, 'Y', 1, 1), {"character Y", "number 1", "repeat"}},
        {"2024-01-01", "Q", ParseError(Kind::InvalidFormat, 'Q', 0, 1), {"character Q", "number 0"}},
        {"2024-01-01", "MD", ParseError(Kind::InvalidFormat, '\0', 0, 2), {"year"}},
        {"2024-01-01", "YM", ParseError(Kind::InvalidFormat, '\0', 0, 2), {"month and day"}},
        {"2024-01-01", "YD", ParseError(Kind::InvalidFormat, '\0', 0, 2), {"month and day"}},
        {"2024-01-01", "YMDN", ParseError(Kind::InvalidFormat, '\0', 0, 4), {"day of the year"}},
        {"2024-01-01", "Y", ParseError(Kind::InvalidFormat, '\0', 0, 1), {"year"}},
        {"2024-01-01", "", ParseError(Kind::InvalidFormat, '\0', 0, 0), {"year"}},
    };
}

void checkFormatErrors() {
    for (const ErrorCase& errorCase : formatErrorCases()) {
        const std::string label = "tryParse(\"" + errorCase.text + "\", \"" + errorCase.format + "\")";
        checkError(label, datetimeparser::tryParse(errorCase.text, errorCase.format), errorCase);
    }
}

std::vector<ErrorCase> iso8601ErrorCases() {
    using Kind = ParseErrorKind;
    return {
        {"", "", ParseError(Kind::EndOfText, 'Y', 0, 0), {"year"}},
        {"2024", "", ParseError(Kind::EndOfText, 'M', 4, 0), {"month"}},
        {"2024-03", "", ParseError(Kind::EndOfText, 'D', 7, 0), {"day", "\"2024-03\""}},
        {"2024-3-15", "", ParseError(Kind::UnexpectedCharacter, 'M', 6, 1), {"month", "character 6", "\"2024-3-15\""}},
        {"20240315", "", ParseError(Kind::UnexpectedCharacter, 'M', 4, 1), {"month", "character 4"}},
        {"2024/03/15", "", ParseError(Kind::UnexpectedCharacter, 'M', 4, 1), {"month"}},
        {"15 MAR 2024", "", ParseError(Kind::UnexpectedCharacter, 'Y', 2, 1), {"year", "character 2"}},
        {" 2024-03-15", "", ParseError(Kind::UnexpectedCharacter, 'Y', 0, 1), {"year", "character 0"}},
        {"2024-13-01", "", ParseError(Kind::OutOfRange, 'M', 5, 2, 13, 1, 12), {"month 13", "1 to 12"}},
        {"2024-01-32", "", ParseError(Kind::OutOfRange, 'D', 8, 2, 32, 1, 31), {"day 32", "1 to 31"}},
        {"0999-01-01", "", ParseError(Kind::OutOfRange, 'Y', 0, 4, 999, 1000, 4000), {"year 999"}},
        {"2023-02-29", "", ParseError(Kind::NotInMonth, 'D', 8, 2, 29, 1, 28), {"day 29", "28 days", "character 8"}},
        {"2024-04-31", "", ParseError(Kind::NotInMonth, 'D', 8, 2, 31, 1, 30), {"30 days"}},
        {"2024-03-15T24:00", "", ParseError(Kind::OutOfRange, 'h', 11, 2, 24, 0, 23), {"hour 24", "0 to 23"}},
        {"2024-03-15T12:60", "", ParseError(Kind::OutOfRange, 'm', 14, 2, 60, 0, 59), {"minute 60"}},
        {"2024-03-15T12:00:60", "", ParseError(Kind::OutOfRange, 's', 17, 2, 60, 0, 59), {"second 60", "character 17"}},
        {"2024-03-15T", "", ParseError(Kind::EndOfText, 'h', 11, 0), {"hour"}},
        {"2024-03-15T12", "", ParseError(Kind::EndOfText, 'm', 13, 0), {"minute"}},
        {"2024-03-15T12:34:", "", ParseError(Kind::EndOfText, 's', 17, 0), {"second"}},
        {"2024-03-15T12:34:56.", "", ParseError(Kind::EndOfText, 'f', 20, 0), {"fraction of a second"}},
        {"2024-03-15T12:34:56,5", "", ParseError(Kind::TrailingText, '\0', 19, 2), {"\",5\"", "character 19"}},
        {"2024-03-15T12:34:56+0530", "", ParseError(Kind::TrailingText, '\0', 22, 2), {"\"30\"", "character 22"}},
        {"2024-03-15T12:34:56+25:00", "", ParseError(Kind::OutOfRange, 'z', 20, 2, 25, 0, 23), {"time zone 25"}},
        {"2024-03-15T12:34:56+", "", ParseError(Kind::EndOfText, 'z', 20, 0), {"time zone"}},
        {"2024-03-15T12:34:56 junk", "", ParseError(Kind::TrailingText, '\0', 19, 5), {"\" junk\""}},
        {"2024-03-15Z", "", ParseError(Kind::TrailingText, '\0', 10, 1), {"\"Z\""}},
    };
}

void checkIso8601Errors() {
    for (const ErrorCase& errorCase : iso8601ErrorCases()) {
        const std::string label = "tryParseIso8601(\"" + errorCase.text + "\")";
        checkError(label, datetimeparser::tryParseIso8601(errorCase.text), errorCase);
    }
}

void checkDecimalYears() {
    // A decimal year is one number, from 1000 to 4000, which can be written as any number can.
    struct YearCase {
        std::string text;
        double expected;
    };
    const std::vector<YearCase> cases = {
        {"2005.2", 2005.2},     {"2000", 2000.0},       {"2000.", 2000.0},        {"1000", 1000.0},
        {"1000.0", 1000.0},     {"4000", 4000.0},       {"4000.0", 4000.0},       {"1000.5", 1000.5},
        {"3999.999", 3999.999}, {"+2005", 2005.0},      {"2.005e3", 2005.0},      {" 2005.2", 2005.2},
        {"2005.2 ", 2005.2},    {"\t2005.2\n", 2005.2}, {"2020.1015", 2020.1015}, {"2020.12345", 2020.12345},
    };
    for (const YearCase& yearCase : cases) {
        const std::string label = "tryParseDecimalYear(\"" + yearCase.text + "\")";
        const ParseResult<double> result = datetimeparser::tryParseDecimalYear(yearCase.text);
        if (!result.succeeded()) {
            check(false, label + " could not read the text: " + describe(*result.error(), yearCase.text));
            continue;
        }
        check(std::fabs(result.value() - yearCase.expected) <= kTolerance,
              label + " is " + std::to_string(result.value()) + " instead of " + std::to_string(yearCase.expected));
    }

    using Kind = ParseErrorKind;
    const std::vector<ErrorCase> errorCases = {
        {"", "", ParseError(Kind::EndOfText, 'Y', 0, 0), {"ended", "year"}},
        {"   ", "", ParseError(Kind::EndOfText, 'Y', 3, 0), {"ended"}},
        {"abc", "", ParseError(Kind::UnexpectedCharacter, 'Y', 0, 3), {"year", "character 0", "\"abc\""}},
        {"2005.2x", "", ParseError(Kind::UnexpectedCharacter, 'Y', 0, 7), {"character 0"}},
        {" 2005.2x", "", ParseError(Kind::UnexpectedCharacter, 'Y', 1, 7), {"character 1"}},
        {"2000.0.0", "", ParseError(Kind::UnexpectedCharacter, 'Y', 0, 8), {}},
        {"2024-01-01", "", ParseError(Kind::UnexpectedCharacter, 'Y', 0, 10), {}},
        {"999", "", ParseError(Kind::OutOfRange, 'Y', 0, 3, 999, 1000, 4000), {"year 999", "1000 to 4000"}},
        {"4001", "", ParseError(Kind::OutOfRange, 'Y', 0, 4, 4001, 1000, 4000), {"year 4001"}},

        // A year that is not a whole number is reported rounded away from the allowed years, so that the number that
        // is reported is outside them.
        {"999.5", "", ParseError(Kind::OutOfRange, 'Y', 0, 5, 999, 1000, 4000), {"year 999 "}},
        {"999.9", "", ParseError(Kind::OutOfRange, 'Y', 0, 5, 999, 1000, 4000), {"year 999 "}},
        {"4000.5", "", ParseError(Kind::OutOfRange, 'Y', 0, 6, 4001, 1000, 4000), {"year 4001 "}},
        {"20201015", "", ParseError(Kind::OutOfRange, 'Y', 0, 8, 20201015, 1000, 4000), {"year 20201015"}},
    };
    for (const ErrorCase& errorCase : errorCases) {
        const std::string label = "tryParseDecimalYear(\"" + errorCase.text + "\")";
        checkError(label, datetimeparser::tryParseDecimalYear(errorCase.text), errorCase);
    }
}

void checkMonthNames() {
    // Each of the twelve, as its abbreviation in capitals, in lower case and as a full name.
    const std::array<std::string, 12> capitals = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                                  "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    const std::array<std::string, 12> lowerCase = {"jan", "feb", "mar", "apr", "may", "jun",
                                                   "jul", "aug", "sep", "oct", "nov", "dec"};
    const std::array<std::string, 12> fullNames = {"January",   "February", "March",    "April",
                                                   "May",       "June",     "July",     "August",
                                                   "September", "October",  "November", "December"};
    for (std::size_t i = 0; i < capitals.size(); ++i) {
        const int expected = static_cast<int>(i) + 1;
        for (const std::string& name : {capitals[i], lowerCase[i], fullNames[i]}) {
            const auto month = datetimeparser::monthFromName(name);
            check(month.has_value() && month->value == expected,
                  "monthFromName(\"" + name + "\") is not month " + std::to_string(expected));
        }
    }

    // Matched on the first three letters, so what follows them is not looked at, and the case does not matter.
    for (const char* name : {"jAn", "SEPT", "Sept", "september", "SEPTEMBER", "Septxyz", "Janu1"}) {
        const auto month = datetimeparser::monthFromName(name);
        check(month.has_value(), std::string("monthFromName(\"") + name + "\") is not a month");
    }
    check(datetimeparser::monthFromName("Augst").value().value == 8, "monthFromName(\"Augst\") is not August");

    // Fewer than three letters, and anything else that is not the start of a month, is not one.
    for (const char* name : {"", "J", "Ja", "XYZ", "123", " JAN", "JA N", "Jn"}) {
        check(!datetimeparser::monthFromName(name).has_value(),
              std::string("monthFromName(\"") + name + "\") is a month");
    }
}

/// Checks that a call throws a DateTimeParseError for the text, for the reason expected, and with the message that
/// describe() gives for it.
template <typename Function>
void checkThrows(const std::string& label, Function call, const std::string& text, const ParseErrorKind kind) {
    try {
        call();
        check(false, label + " did not throw");
    } catch (const DateTimeParseError& error) {
        checkNumber(label, "kind of error", static_cast<int>(error.error().kind), static_cast<int>(kind));
        check(error.text() == text, label + " has the text \"" + error.text() + "\" instead of \"" + text + "\"");
        check(std::string(error.what()) == datetimeparser::describe(error.error(), text),
              label + " has a message that is not that of describe(): " + error.what());
    } catch (const std::exception& error) {
        check(false, label + " threw something else: " + error.what());
    }
}

void checkThrowingFunctions() {
    using Kind = ParseErrorKind;
    using datetimeparser::parse;
    using datetimeparser::parseDecimalYear;
    using datetimeparser::parseIso8601;

    // The throwing functions give what the others do, when the text can be read.
    checkParts("parse(\"2024-03-15 12:34:56.5+12:00\", \"YMDhms\")", parse("2024-03-15 12:34:56.5+12:00", "YMDhms"),
               monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.5, 720));
    checkParts("parseIso8601(\"2024-03-15T12:34:56Z\")", parseIso8601("2024-03-15T12:34:56Z"),
               monthAndDayParts(2024, 3, 15, 12, 34, 56, 0.0, 0));
    check(std::fabs(parseDecimalYear("2005.2") - 2005.2) <= kTolerance, "parseDecimalYear(\"2005.2\") is not 2005.2");

    // One for each way that the text can be wrong.
    checkThrows("parse of an empty text", [] { parse("", "YMD"); }, "", Kind::EndOfText);
    checkThrows("parse of a month of 13", [] { parse("2024-13-01", "YMD"); }, "2024-13-01", Kind::OutOfRange);
    checkThrows("parse of 29 February 2023", [] { parse("2023-02-29", "YMD"); }, "2023-02-29", Kind::NotInMonth);
    checkThrows(
        "parse of a month name that is not one", [] { parse("15 XYZ 2024", "DMY"); }, "15 XYZ 2024",
        Kind::UnknownMonthName);
    checkThrows(
        "parse of text after the date", [] { parse("2024-01-01 junk", "YMDhms"); }, "2024-01-01 junk",
        Kind::TrailingText);
    checkThrows(
        "parse with a format that has a letter that is not one", [] { parse("2024-01-01", "YMDx"); }, "2024-01-01",
        Kind::InvalidFormat);
    checkThrows(
        "parseIso8601 of a date with slashes", [] { parseIso8601("2024/03/15"); }, "2024/03/15",
        Kind::UnexpectedCharacter);
    checkThrows("parseIso8601 of 29 February 2023", [] { parseIso8601("2023-02-29"); }, "2023-02-29", Kind::NotInMonth);
    checkThrows(
        "parseDecimalYear of text that is not a number", [] { parseDecimalYear("abc"); }, "abc",
        Kind::UnexpectedCharacter);
    checkThrows("parseDecimalYear of a year of 999", [] { parseDecimalYear("999"); }, "999", Kind::OutOfRange);

    // The error is an std::invalid_argument, so it can be caught as one.
    try {
        parse("not a date", "YMD");
        check(false, "parse(\"not a date\", \"YMD\") did not throw");
    } catch (const std::invalid_argument&) {
        // As it should be.
    }
}

void checkParseResult() {
    const ParseResult<DateTimeParts> succeeded = datetimeparser::tryParse("2024-03-15", "YMD");
    check(succeeded.succeeded(), "a result that was read does not say so");
    check(!succeeded.error().has_value(), "a result that was read has an error");

    const ParseResult<DateTimeParts> failed = datetimeparser::tryParse("2024-13-15", "YMD");
    check(!failed.succeeded(), "a result that was not read says that it was");
    check(failed.error().has_value(), "a result that was not read has no error");
}

}  // namespace

int main() {
    checkFormats();
    checkMonthNamesInText();
    checkDaysOfTheYear();
    checkFractionsAndZones();
    checkIso8601();
    checkIso8601IsStrict();
    checkFormatErrors();
    checkIso8601Errors();
    checkDecimalYears();
    checkMonthNames();
    checkThrowingFunctions();
    checkParseResult();

    if (failures > 0) {
        std::cout << failures << " datetimeparser_test check(s) failed\n";
        return 1;
    }
    std::cout << "datetimeparser_test: all checks passed\n";
    return 0;
}
