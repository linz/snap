#include "snapconfig.h"

#include "util/datetimeparser.h"

#include <boost/numeric/conversion/cast.hpp>
#include <array>
#include <cmath>
#include <initializer_list>
#include <string>

#include "util/fieldscanner.hpp"
#include "util/rangedint.h"
#include "util/snapctype.h"

namespace datetimeparser {

namespace {

/// The letters of a format, in the order of the fields that they stand for.
constexpr std::string_view kFormatLetters = "YMDhmsN";

/// The most digits that each field of a format has, in the same order as kFormatLetters.
constexpr std::array<std::size_t, kFormatLetters.size()> kMaximumDigits = {4, 2, 2, 2, 2, 2, 3};

constexpr std::size_t kYearIndex = 0;
constexpr std::size_t kMonthIndex = 1;
constexpr std::size_t kDayIndex = 2;
constexpr std::size_t kHourIndex = 3;
constexpr std::size_t kMinuteIndex = 4;
constexpr std::size_t kSecondIndex = 5;
constexpr std::size_t kDayOfYearIndex = 6;
constexpr std::size_t kFieldCount = kFormatLetters.size();

/// The most letters of a month name that are read. The ones after the third are not looked at.
constexpr std::size_t kMaximumMonthNameLetters = 9;

/// The most digits of a fraction of a second that are used. Any after them are ignored. Without a limit, a long run of
/// 9s would round to exactly 1.0, but a fraction of a second must be less than 1.
constexpr std::size_t kMaximumFractionDigits = 15;
static_assert(kMaximumFractionDigits <= 15, "with more digits, a fraction of a second could round up to 1.0");

/// Reads through a text from the front, one character at a time, and remembers how far it has got. It does not
/// interpret the text: the code that uses it decides what each character means.
class Cursor {
 public:
    explicit Cursor(const std::string_view text) : _text(text) {}

    /// \return whether every character of the text has been passed.
    [[nodiscard]] bool atEnd() const {
        return _position >= _text.size();
    }

    /// \return the character that is ahead characters on from the next one to read, or 0 if the text ends before
    /// then.
    [[nodiscard]] char peek(const std::size_t ahead = 0) const {
        return _position + ahead < _text.size() ? _text[_position + ahead] : '\0';
    }

    /// Moves on past the next character. At the end of the text it does not move.
    /// \return the character that it moved past, or 0 if the text had ended.
    char next() {
        if (atEnd()) {
            return '\0';
        }
        return _text[_position++];
    }

    /// Moves on past the next character if it is the one expected, and not otherwise.
    /// \return whether it was.
    bool consume(const char expected) {
        if (atEnd() || peek() != expected) {
            return false;
        }
        ++_position;
        return true;
    }

    /// \return how many characters have been passed.
    [[nodiscard]] std::size_t position() const {
        return _position;
    }

 private:
    std::string_view _text;
    std::size_t _position = 0;
};

/// \return whether character is a letter of the alphabet.
bool isLetter(const char character) {
    return ISALNUM(character) && !is_digit(character);
}

/// \return the error for a character, or the end of the text, that is not what the field needs next.
ParseError unexpectedAt(const Cursor& cursor,  ///< where the text is not as needed
                        const char field) {    ///< the field that was being read
    if (cursor.atEnd()) {
        return ParseError(ParseErrorKind::EndOfText, field, cursor.position(), 0);
    }
    return ParseError(ParseErrorKind::UnexpectedCharacter, field, cursor.position(), 1);
}

/// Reads up to maximumDigits digits as a number. It stops at the first character that is not a digit, and reads no
/// digits, giving 0, if that is the first.
/// \return the number of digits that were read.
std::size_t readDigits(Cursor& cursor,                   ///< the text, moved on past the digits
                       const std::size_t maximumDigits,  ///< the most digits to read
                       int& value) {                     ///< set to the number that they make
    std::size_t count = 0;
    value = 0;
    while (count < maximumDigits && is_digit(cursor.peek())) {
        // Shift the digits read so far one place left and add the new one (its distance from '0' is between [0-9]).
        value = value * 10 + (cursor.next() - '0');
        ++count;
    }
    return count;
}

/// Reads exactly count digits as a number.
/// \return why they could not be read, or nothing if they were.
std::optional<ParseError> readExactDigits(Cursor& cursor,           ///< the text, moved on past the digits
                                          const std::size_t count,  ///< how many digits there must be
                                          const char field,         ///< the field that they are of
                                          int& value) {             ///< set to the number that they make
    if (readDigits(cursor, count, value) < count) {
        return unexpectedAt(cursor, field);
    }
    return std::nullopt;
}

/// Checks that a number is within the limits of the type of a field.
/// \return which limits it is outside of, or nothing if it is within them.
template <typename Ranged>
std::optional<ParseError> checkRange(const char field,            ///< the field that the number is of
                                     const std::size_t position,  ///< where the number starts in the text
                                     const std::size_t length,    ///< how many characters it has
                                     const int value) {           ///< the number
    if (Ranged::from(value)) {
        return std::nullopt;
    }
    return ParseError(ParseErrorKind::OutOfRange, field, position, length, value, Ranged::kMinimum, Ranged::kMaximum);
}

/// Checks the number that was read for a field of a format against the limits of that field.
/// \return which limits it is outside of, or nothing if it is within them.
std::optional<ParseError> checkFieldRange(const std::size_t index,     ///< the field, as its place in kFormatLetters
                                          const std::size_t position,  ///< where the number starts in the text
                                          const std::size_t length,    ///< how many characters it has
                                          const int value) {           ///< the number
    const char field = kFormatLetters[index];
    switch (index) {
        case kYearIndex:
            return checkRange<Year>(field, position, length, value);
        case kMonthIndex:
            return checkRange<Month>(field, position, length, value);
        case kDayIndex:
            return checkRange<DayOfMonth>(field, position, length, value);
        case kHourIndex:
            return checkRange<Hour>(field, position, length, value);
        case kMinuteIndex:
            return checkRange<Minute>(field, position, length, value);
        case kSecondIndex:
            return checkRange<Second>(field, position, length, value);
        default:
            return checkRange<DayOfYear>(field, position, length, value);
    }
}

/// \return whether a year is a leap year, in the Gregorian calendar.
constexpr bool isLeapYear(const int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

/// \return how many days a month has.
constexpr int daysInMonth(const int year, const int month) {
    constexpr std::array<int, 12> days = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 && isLeapYear(year) ? 29 : days[static_cast<std::size_t>(month - 1)];
}

/// Checks that a day is in its month. The year and the month must already be within their limits.
/// \return why it is not, or nothing if it is.
std::optional<ParseError> checkDayInMonth(const int year,              ///< the year
                                          const int month,             ///< the month
                                          const int day,               ///< the day of the month
                                          const std::size_t position,  ///< where the day starts in the text
                                          const std::size_t length) {  ///< how many characters it has
    const int days = daysInMonth(year, month);
    if (day <= days) {
        return std::nullopt;
    }
    return ParseError(ParseErrorKind::NotInMonth, 'D', position, length, day, 1, days);
}

/// Checks that a day of the year is in its year. The year must already be within its limits.
/// \return why it is not, or nothing if it is.
std::optional<ParseError> checkDayInYear(const int year,              ///< the year
                                         const int day,               ///< the day of the year
                                         const std::size_t position,  ///< where the day starts in the text
                                         const std::size_t length) {  ///< how many characters it has
    const int days = isLeapYear(year) ? 366 : 365;
    if (day <= days) {
        return std::nullopt;
    }
    return ParseError(ParseErrorKind::OutOfRange, 'N', position, length, day, 1, days);
}

/// Reads a fraction of a second, which is a point, or a comma if allowed, and then one or more digits. Digits after
/// the fifteenth are read but not used.
/// \return why it could not be read, or nothing if it was.
std::optional<ParseError> readFraction(Cursor& cursor,         ///< the text, with its next character the point or comma
                                       const bool allowComma,  ///< whether a comma can be the point
                                       double& fraction) {     ///< set to the fraction, from 0 to less than 1
    if (!cursor.consume('.') && !(allowComma && cursor.consume(','))) {
        return unexpectedAt(cursor, 'f');
    }
    if (!is_digit(cursor.peek())) {
        return unexpectedAt(cursor, 'f');
    }
    long long digits = 0;
    double divisor = 1.0;
    std::size_t used = 0;
    while (is_digit(cursor.peek())) {
        const char digit = cursor.next();
        if (used < kMaximumFractionDigits) {
            digits = digits * 10 + (digit - '0');
            divisor *= 10.0;
            ++used;
        }
    }
    fraction = static_cast<double>(digits) / divisor;
    return std::nullopt;
}

/// Reads a time zone, which is Z, or + or - and the hours of its offset from UTC, and then its minutes if it has them.
/// \return why it could not be read, or nothing if it was.
std::optional<ParseError> readZone(Cursor& cursor,  ///< the text, with its next character the Z, + or -
                                   const bool allowMinutesWithoutColon,  ///< whether +hhmm is allowed as well as +hh:mm
                                   std::optional<UtcOffsetMinutes>& offset) {  ///< set to the offset
    if (cursor.consume('Z')) {
        offset.emplace(0);
        return std::nullopt;
    }
    const int sign = cursor.peek() == '-' ? -1 : 1;
    if (!cursor.consume('+') && !cursor.consume('-')) {
        return unexpectedAt(cursor, 'z');
    }
    const std::size_t hoursPosition = cursor.position();
    int hours = 0;
    if (const auto error = readExactDigits(cursor, 2, 'z', hours)) {
        return error;
    }
    int minutes = 0;
    const std::size_t minutesPosition = cursor.position() + 1;
    if (cursor.consume(':') || (allowMinutesWithoutColon && is_digit(cursor.peek()))) {
        if (const auto error = readExactDigits(cursor, 2, 'z', minutes)) {
            return error;
        }
    }
    if (hours > 23) {
        return ParseError(ParseErrorKind::OutOfRange, 'z', hoursPosition, 2, hours, 0, 23);
    }
    if (minutes > 59) {
        return ParseError(ParseErrorKind::OutOfRange, 'z', minutesPosition, 2, minutes, 0, 59);
    }
    offset.emplace(sign * (hours * 60 + minutes));
    return std::nullopt;
}

/// Makes the parts from the numbers that were read, which must all be within their limits and make a real date.
DateTimeParts makeParts(const std::array<int, kFieldCount>& values,  ///< the numbers, in the order of kFormatLetters
                        const bool hasDayOfYear,                     ///< whether the day was a day of the year
                        const double fractionOfSecond,               ///< the fraction of a second
                        const std::optional<UtcOffsetMinutes>& offset) {  ///< the time zone, if there was one
    const std::variant<MonthAndDay, DayOfYear> day =
        hasDayOfYear ? std::variant<MonthAndDay, DayOfYear>(DayOfYear(values[kDayOfYearIndex]))
                     : std::variant<MonthAndDay, DayOfYear>(
                           MonthAndDay(Month(values[kMonthIndex]), DayOfMonth(values[kDayIndex])));
    return DateTimeParts(Year(values[kYearIndex]), day, Hour(values[kHourIndex]), Minute(values[kMinuteIndex]),
                         Second(values[kSecondIndex]), fractionOfSecond, offset);
}

/// \return the value of a result that must have succeeded, or throws the reason that it did not.
template <typename Value>
Value valueOrThrow(const ParseResult<Value>& result,  ///< what was read
                   const std::string_view text) {     ///< the text that it was read from
    if (!result.succeeded()) {
        throw DateTimeParseError(*result.error(), text);
    }
    return result.value();
}

/// \return the name of a field, from its letter in a format, for a message.
std::string fieldName(const char field) {
    switch (field) {
        case 'Y':
            return "year";
        case 'M':
            return "month";
        case 'D':
            return "day";
        case 'h':
            return "hour";
        case 'm':
            return "minute";
        case 's':
            return "second";
        case 'N':
            return "day of the year";
        case 'f':
            return "fraction of a second";
        case 'z':
            return "time zone";
        default:
            return "date";
    }
}

}  // namespace

std::optional<Month> monthFromName(const std::string_view name) {
    constexpr std::array<std::string_view, 12> abbreviations = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                                                "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    // At least 3 characters are needed for a unique match, as JUN and JUL are the same up to the second.
    // Only the first 3 characters are compared, so a misspelling after them is NOT detected: Augst and Augxyz are
    // both read as August.
    if (name.size() < 3) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < abbreviations.size(); ++i) {
        if (compare_ignoring_case(name.substr(0, 3), abbreviations[i]) == 0) {
            return Month::from(boost::numeric_cast<int>(i) + 1);
        }
    }
    return std::nullopt;
}

ParseResult<DateTimeParts> tryParse(const std::string_view text, const std::string_view format) {
    using Result = ParseResult<DateTimeParts>;

    // The fields of the format, in order.
    std::string order;
    std::array<bool, kFieldCount> inFormat = {};
    for (std::size_t i = 0; i < format.size(); ++i) {
        const char letter = format[i];
        if (ISSPACE(letter)) {
            continue;
        }
        const std::size_t index = kFormatLetters.find(letter);
        if (index == std::string_view::npos || inFormat[index]) {
            return Result(ParseError(ParseErrorKind::InvalidFormat, letter, i, 1));
        }
        inFormat[index] = true;
        order.push_back(letter);
    }
    const bool hasDayOfYear = inFormat[kDayOfYearIndex];
    const bool hasMonthAndDay = inFormat[kMonthIndex] && inFormat[kDayIndex];
    const bool hasMonthOrDay = inFormat[kMonthIndex] || inFormat[kDayIndex];
    if (!inFormat[kYearIndex] || (hasDayOfYear ? hasMonthOrDay : !hasMonthAndDay)) {
        return Result(ParseError(ParseErrorKind::InvalidFormat, '\0', 0, format.size()));
    }

    Cursor cursor(text);
    std::array<int, kFieldCount> values = {};
    std::array<std::size_t, kFieldCount> positions = {};
    std::array<std::size_t, kFieldCount> lengths = {};
    bool timeEnded = false;
    bool secondsRead = false;

    for (const char letter : order) {
        const std::size_t index = kFormatLetters.find(letter);
        const bool isTimeField = index >= kHourIndex && index <= kSecondIndex;

        if (isTimeField) {
            // The time may be missing, which leaves the hour, minute and second 0. But if what follows the date is not
            // a number, such as "junk", stop here without reading it, so that the check at the end reports it as
            // trailing text.
            if (timeEnded) {
                continue;
            }
            while (!cursor.atEnd() && !ISALNUM(cursor.peek())) {
                cursor.next();
            }
            if (index == kHourIndex && (cursor.peek() == 'T' || cursor.peek() == 't') && is_digit(cursor.peek(1))) {
                cursor.next();
            }
            if (!is_digit(cursor.peek())) {
                timeEnded = true;
                continue;
            }
        } else {
            while (!cursor.atEnd() && !is_digit(cursor.peek()) && (index != kMonthIndex || !isLetter(cursor.peek()))) {
                cursor.next();
            }
            if (cursor.atEnd()) {
                return Result(unexpectedAt(cursor, letter));
            }
        }

        const std::size_t start = cursor.position();
        if (index == kMonthIndex && !is_digit(cursor.peek())) {
            std::size_t letterCount = 0;
            while (letterCount < kMaximumMonthNameLetters && isLetter(cursor.peek())) {
                cursor.next();
                ++letterCount;
            }
            const std::optional<Month> month = monthFromName(text.substr(start, letterCount));
            if (!month) {
                return Result(ParseError(ParseErrorKind::UnknownMonthName, letter, start, letterCount));
            }
            values[index] = month->value;
        } else {
            readDigits(cursor, kMaximumDigits[index], values[index]);
            if (const auto error = checkFieldRange(index, start, cursor.position() - start, values[index])) {
                return Result(*error);
            }
        }
        positions[index] = start;
        lengths[index] = cursor.position() - start;
        secondsRead = secondsRead || index == kSecondIndex;
    }

    if (hasDayOfYear) {
        if (const auto error = checkDayInYear(values[kYearIndex], values[kDayOfYearIndex], positions[kDayOfYearIndex],
                                              lengths[kDayOfYearIndex])) {
            return Result(*error);
        }
    } else if (const auto error = checkDayInMonth(values[kYearIndex], values[kMonthIndex], values[kDayIndex],
                                                  positions[kDayIndex], lengths[kDayIndex])) {
        return Result(*error);
    }

    double fraction = 0.0;
    std::optional<UtcOffsetMinutes> offset;
    if (secondsRead && order.back() == 's') {
        if (cursor.peek() == '.' || cursor.peek() == ',') {
            if (const auto error = readFraction(cursor, true, fraction)) {
                return Result(*error);
            }
        }
        if (cursor.peek() == 'Z' || cursor.peek() == '+' || cursor.peek() == '-') {
            if (const auto error = readZone(cursor, true, offset)) {
                return Result(*error);
            }
        }
    }

    while (!cursor.atEnd() && ISSPACE(cursor.peek())) {
        cursor.next();
    }
    if (!cursor.atEnd()) {
        return Result(
            ParseError(ParseErrorKind::TrailingText, '\0', cursor.position(), text.size() - cursor.position()));
    }
    return Result(makeParts(values, hasDayOfYear, fraction, offset));
}

ParseResult<DateTimeParts> tryParseIso8601(const std::string_view text) {
    using Result = ParseResult<DateTimeParts>;

    Cursor cursor(text);
    std::array<int, kFieldCount> values = {};
    std::array<std::size_t, kFieldCount> positions = {};

    // The date, as yyyy-mm-dd.
    for (const std::size_t index : {kYearIndex, kMonthIndex, kDayIndex}) {
        if (index != kYearIndex && !cursor.consume('-')) {
            return Result(unexpectedAt(cursor, kFormatLetters[index]));
        }
        positions[index] = cursor.position();
        if (const auto error = readExactDigits(cursor, kMaximumDigits[index], kFormatLetters[index], values[index])) {
            return Result(*error);
        }
        if (const auto error = checkFieldRange(index, positions[index], kMaximumDigits[index], values[index])) {
            return Result(*error);
        }
    }
    if (const auto error = checkDayInMonth(values[kYearIndex], values[kMonthIndex], values[kDayIndex],
                                           positions[kDayIndex], kMaximumDigits[kDayIndex])) {
        return Result(*error);
    }

    // The time, as hh:mm or hh:mm:ss, which may have a fraction of a second after the seconds, and then a time zone.
    double fraction = 0.0;
    std::optional<UtcOffsetMinutes> offset;
    if (cursor.consume('T') || cursor.consume(' ')) {
        for (const std::size_t index : {kHourIndex, kMinuteIndex, kSecondIndex}) {
            if (index == kSecondIndex && cursor.peek() != ':') {
                break;
            }
            if (index != kHourIndex && !cursor.consume(':')) {
                return Result(unexpectedAt(cursor, kFormatLetters[index]));
            }
            positions[index] = cursor.position();
            if (const auto error =
                    readExactDigits(cursor, kMaximumDigits[index], kFormatLetters[index], values[index])) {
                return Result(*error);
            }
            if (const auto error = checkFieldRange(index, positions[index], kMaximumDigits[index], values[index])) {
                return Result(*error);
            }
            if (index == kSecondIndex && cursor.peek() == '.') {
                if (const auto error = readFraction(cursor, false, fraction)) {
                    return Result(*error);
                }
            }
        }
        if (cursor.peek() == 'Z' || cursor.peek() == '+' || cursor.peek() == '-') {
            if (const auto error = readZone(cursor, false, offset)) {
                return Result(*error);
            }
        }
    }

    if (!cursor.atEnd()) {
        return Result(
            ParseError(ParseErrorKind::TrailingText, '\0', cursor.position(), text.size() - cursor.position()));
    }
    return Result(makeParts(values, false, fraction, offset));
}

ParseResult<double> tryParseDecimalYear(const std::string_view text) {
    using Result = ParseResult<double>;

    std::size_t start = 0;
    std::size_t end = text.size();
    while (start < end && ISSPACE(text[start])) {
        ++start;
    }
    while (end > start && ISSPACE(text[end - 1])) {
        --end;
    }
    if (start == end) {
        return Result(ParseError(ParseErrorKind::EndOfText, 'Y', start, 0));
    }
    const std::string_view number = text.substr(start, end - start);
    const std::optional<double> year = parse_double(number);
    if (!year) {
        return Result(ParseError(ParseErrorKind::UnexpectedCharacter, 'Y', start, number.size()));
    }
    if (!std::isfinite(*year) || *year < Year::kMinimum || *year > Year::kMaximum) {
        const int value =
            std::isfinite(*year) && std::fabs(*year) < 1e9 ? boost::numeric_cast<int>(std::lround(*year)) : 0;
        return Result(
            ParseError(ParseErrorKind::OutOfRange, 'Y', start, number.size(), value, Year::kMinimum, Year::kMaximum));
    }
    return Result(*year);
}

DateTimeParseError::DateTimeParseError(const ParseError& error, const std::string_view text)
    : std::invalid_argument(describe(error, text)), _error(error), _text(text) {}

DateTimeParts parse(const std::string_view text, const std::string_view format) {
    return valueOrThrow(tryParse(text, format), text);
}

DateTimeParts parseIso8601(const std::string_view text) {
    return valueOrThrow(tryParseIso8601(text), text);
}

double parseDecimalYear(const std::string_view text) {
    return valueOrThrow(tryParseDecimalYear(text), text);
}

std::string describe(const ParseError& error, const std::string_view text) {
    const std::string where = " at character " + std::to_string(error.position) + " of \"" + std::string(text) + "\"";
    const std::string field = fieldName(error.field);
    switch (error.kind) {
        case ParseErrorKind::EndOfText:
            return "the text ended where the " + field + " was expected, in \"" + std::string(text) + "\"";
        case ParseErrorKind::UnexpectedCharacter:
            return "the " + field + " was expected but the text has something else" + where;
        case ParseErrorKind::OutOfRange:
            return "the " + field + " " + std::to_string(error.value) + " is not from " +
                   std::to_string(error.minimum) + " to " + std::to_string(error.maximum) + where;
        case ParseErrorKind::NotInMonth:
            return "the day " + std::to_string(error.value) + " is more than the " + std::to_string(error.maximum) +
                   " days of its month" + where;
        case ParseErrorKind::UnknownMonthName:
            return "\"" + std::string(text.substr(error.position, error.length)) + "\" is not the name of a month" +
                   where;
        case ParseErrorKind::TrailingText:
            return "there is text after the end of the date, \"" +
                   std::string(text.substr(error.position, error.length)) + "\"" + where;
        case ParseErrorKind::InvalidFormat:
            if (error.field != '\0') {
                return std::string("the format has the character ") + error.field + " (number " +
                       std::to_string(error.position) +
                       "), which is not one of Y M D h m s N, or is a repeat of one that it already has";
            }
            return "the format does not have a year with either a month and day or a day of the year";
    }
    return "the date could not be read";
}

}  // namespace datetimeparser
