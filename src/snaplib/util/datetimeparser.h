#pragma once

/// \file
/// Reads a date and time written as text into its parts. snap_datetime_parse (dateutil.h) and the GGRXF deformation
/// model both use it. It returns the parts as they are written, and does not make a day number from them, so each
/// caller decides what a time zone offset or a fraction of a second means to it.
///
/// A string is read in one of three ways:
/// - by a format, a string of the letters Y M D h m s N that gives the order of the fields (see tryParse()), which
///   reads the many forms of date that SNAP files can hold,
/// - as ISO 8601, in its extended form only (see tryParseIso8601()), or
/// - as a year with a fraction, such as 2005.2 (see tryParseDecimalYear()).
///
/// Each way has two functions. The first, whose name begins try, returns what was read, or the reason that the text
/// could not be read. It does not throw, so it is cheap to call when the text is not what was expected, as when one
/// format is tried after another. The second does the same but throws a DateTimeParseError instead of returning the
/// reason.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

#include "util/rangedint.h"

namespace datetimeparser {

/// A year, which is from 1000 to 4000.
using Year = RangedInt<std::uint16_t, 1000, 4000>;

/// A month, from 1 for January to 12 for December.
using Month = RangedInt<std::uint8_t, 1, 12>;

/// A day of a month. The range is that of the longest months. The parser checks a day against its own month.
using DayOfMonth = RangedInt<std::uint8_t, 1, 31>;

/// A day of a year, counting 1 January as 1. The range is that of a leap year. The parser accepts 366 only in a leap
/// year.
using DayOfYear = RangedInt<std::uint16_t, 1, 366>;

/// An hour of the day, from 0 to 23. The hour 24 is not accepted.
using Hour = RangedInt<std::uint8_t, 0, 23>;

/// A minute of the hour.
using Minute = RangedInt<std::uint8_t, 0, 59>;

/// A second of the minute. A leap second, which is a second of 60, is not accepted.
using Second = RangedInt<std::uint8_t, 0, 59>;

/// The offset of a time zone from UTC, in minutes: how far ahead of UTC the clock in that zone is, at the same moment.
/// It is negative for a zone whose clock is behind UTC. The most that it can be is 23 hours 59 minutes either way.
/// A time of 12:34:56 in a zone that is 12 hours (720 minutes) ahead is 00:34:56 in UTC.
using UtcOffsetMinutes = RangedInt<std::int16_t, -1439, 1439>;

/// A month and the day of it that a date is on.
struct MonthAndDay {
    MonthAndDay(const Month& month,     ///< the month
                const DayOfMonth& day)  ///< the day of that month
        : month(month), day(day) {}

    const Month month;     ///< the month
    const DayOfMonth day;  ///< the day of that month
};

/// The parts of a date and time that were read from a string. The year is always there, and so is the day, either as a
/// month and day or as a day of the year, as the string gave it.
struct DateTimeParts {
    DateTimeParts(const Year& year,                                 ///< the year
                  const std::variant<MonthAndDay, DayOfYear>& day,  ///< the month and day, or the day of the year
                  const Hour& hour = Hour(0),                       ///< the hour, 0 if the string had no time
                  const Minute& minute = Minute(0),                 ///< the minute, 0 if the string had no time
                  const Second& second = Second(0),                 ///< the second, 0 if the string had no seconds
                  const double fractionOfSecond = 0.0,              ///< the fraction of a second, 0 if none
                  const std::optional<UtcOffsetMinutes>& offset = std::nullopt)  ///< the time zone, if it had one
        : year(year),
          day(day),
          hour(hour),
          minute(minute),
          second(second),
          fractionOfSecond(fractionOfSecond),
          offset(offset) {}

    const Year year;                                 ///< the year
    const std::variant<MonthAndDay, DayOfYear> day;  ///< the month and day, or the day of the year
    const Hour hour;                                 ///< the hour, 0 if the string had no time
    const Minute minute;                             ///< the minute, 0 if the string had no time
    const Second second;                             ///< the second, 0 if the string had no seconds

    /// The fraction of a second that followed the seconds, from 0 up to but not including 1. It is 0 if the string
    /// had none.
    const double fractionOfSecond;

    /// The time zone that followed the time, as its offset from UTC (see UtcOffsetMinutes): 0 for Z, 720 for +12:00
    /// and -330 for -05:30. It is empty if the string had no time zone, which does not say that the time is in any
    /// particular zone.
    const std::optional<UtcOffsetMinutes> offset;
};

/// What was wrong with a string that could not be read.
enum class ParseErrorKind {
    EndOfText,            ///< the text ended where a field, or the rest of one, was needed
    UnexpectedCharacter,  ///< a character that the form does not allow at that place
    OutOfRange,           ///< a number outside its limits, which value, minimum and maximum give
    NotInMonth,           ///< a day number that the month does not have, which value and maximum give
    UnknownMonthName,     ///< letters where a month is, that are not the name of one
    TrailingText,         ///< text left after a complete date and time
    InvalidFormat         ///< a format with a character that is not one of Y M D h m s N, or that can not give a date
};

/// Why a string could not be read, and where. It holds only numbers and a letter, so making one is cheap. describe()
/// turns it into a message when one is wanted.
struct ParseError {
    ParseError(const ParseErrorKind kind,   ///< what was wrong
               const char field,            ///< the field that was being read, or 0
               const std::size_t position,  ///< where in the text it went wrong
               const std::size_t length,    ///< how many characters from there were the cause
               const int value = 0,         ///< the number that was read
               const int minimum = 0,       ///< the lowest number that the field can have
               const int maximum = 0)       ///< the highest number that the field can have
        : kind(kind),
          field(field),
          position(position),
          length(length),
          value(value),
          minimum(minimum),
          maximum(maximum) {}

    const ParseErrorKind kind;  ///< what was wrong

    /// The field that was being read, as its letter in a format: Y, M, D, h, m, s or N, or f for a fraction of a
    /// second, or z for a time zone. It is 0 if no field was being read, as for text left after the end.
    const char field;

    const std::size_t position;  ///< where in the text it went wrong, counting from 0
    const std::size_t length;    ///< how many characters from position were the cause, or 0 if the text ended there
    const int value;             ///< for OutOfRange and NotInMonth, the number that was read
    const int minimum;           ///< for OutOfRange, the lowest number that the field can have
    const int maximum;           ///< for OutOfRange and NotInMonth, the highest number that the field can have
};

/// The outcome of reading a string: what it held, or the reason that it could not be read.
template <typename Value>
class ParseResult {
 public:
    /// A string that was read.
    explicit ParseResult(const Value& value) : _value(value) {}

    /// A string that could not be read.
    explicit ParseResult(const ParseError& error) : _error(error) {}

    /// \return whether the string was read.
    [[nodiscard]] bool succeeded() const {
        return _value.has_value();
    }

    /// \return what the string held. It must have been read, so that succeeded() is true.
    [[nodiscard]] const Value& value() const {
        return *_value;
    }

    /// \return why the string could not be read, or nothing if it could.
    [[nodiscard]] const std::optional<ParseError>& error() const {
        return _error;
    }

 private:
    std::optional<Value> _value;
    std::optional<ParseError> _error;
};

/// Reads text by a format. The format is a string of the letters Y M D h m s N, in the order that the fields come in
/// the text: year, month, day, hour, minute, second, and day of the year (counting 1 January as 1, and used in place
/// of the month and day). A format needs a year, and either a month and day or a day of the year.
///
/// Before each field, any characters that cannot start it are skipped: for a month, anything that is not a digit or
/// a letter, and for any other field, anything that is not a digit. So the fields can be separated by any such
/// characters, or by nothing if each number has all its digits. A number is read up to its width, which is 4 digits
/// for the year, 3 for the day of the year and 2 for the others. A month is a number or the name of one (see
/// monthFromName()).
///
/// A format that ends with the seconds also accepts, straight after them, a fraction of a second (a point or a comma,
/// then one or more digits) and a time zone (Z, or + or - then the hours, then the minutes with or without a colon,
/// or no minutes). They are returned in the parts, for the caller to use or to ignore.
///
/// Nothing may follow the last field. A number outside its limits is an error, and so is a day that is not in its
/// month, using the leap years of the Gregorian calendar. The hour, minute and second are 0 if the format has none.
///
/// \return the parts that the text held, or why it could not be read.
ParseResult<DateTimeParts> tryParse(std::string_view text,     ///< the text to read
                                    std::string_view format);  ///< the order of the fields in it

/// Reads text as ISO 8601, in its extended form. That is yyyy-mm-dd, optionally followed by T (or a space) and hh:mm,
/// hh:mm:ss or hh:mm:ss.s, where the fraction has as many digits as it needs, and then optionally Z, or + or - and the
/// hours with or without the minutes, as the time zone. Each field has all its digits, and the separators are
/// exactly those shown. A month is a number, as ISO 8601 has no month names. The limits are those of tryParse().
///
/// \return the parts that the text held, or why it could not be read.
ParseResult<DateTimeParts> tryParseIso8601(std::string_view text);  ///< the text to read

/// Reads text as a year with a fraction, such as 2005.2, with nothing after it. The year must be from 1000 to 4000.
/// \return the year and its fraction, or why the text could not be read.
ParseResult<double> tryParseDecimalYear(std::string_view text);  ///< the text to read

/// The error that parse(), parseIso8601() and parseDecimalYear() throw. Its message says what was wrong with the text,
/// and quotes it, except when the fault is in the format.
class DateTimeParseError : public std::invalid_argument {
 public:
    DateTimeParseError(const ParseError& error,  ///< why the text could not be read
                       std::string_view text);   ///< the text that could not be read, which is copied

    /// \return why the text could not be read.
    [[nodiscard]] const ParseError& error() const {
        return _error;
    }

    /// \return the text that could not be read.
    [[nodiscard]] const std::string& text() const {
        return _text;
    }

 private:
    ParseError _error;
    std::string _text;
};

/// Reads text by a format as tryParse() does, but throws if it can not be read, instead of returning the reason.
/// \return the parts that the text held.
/// \throws DateTimeParseError if the text can not be read by the format.
DateTimeParts parse(std::string_view text,     ///< the text to read
                    std::string_view format);  ///< the order of the fields in it

/// Reads text as ISO 8601 as tryParseIso8601() does, but throws if it can not be read, instead of returning the reason.
/// \return the parts that the text held.
/// \throws DateTimeParseError if the text is not ISO 8601.
DateTimeParts parseIso8601(std::string_view text);  ///< the text to read

/// Reads text as a year with a fraction as tryParseDecimalYear() does, but throws if it can not be read.
/// \return the year and its fraction.
/// \throws DateTimeParseError if the text is not a year with a fraction.
double parseDecimalYear(std::string_view text);  ///< the text to read

/// Says in words what was wrong with the text, and quotes it, except when the fault is in the format. For example: the
/// day 31 is more than the 30 days of its month at character 8 of "2024-04-31".
/// \return the message.
std::string describe(const ParseError& error,  ///< what was wrong
                     std::string_view text);   ///< the text that it was wrong with

/// Finds the month that a name stands for. The name is matched by its first three letters, in any case, so JAN, Jan
/// and January are all January. The letters after the third are not looked at.
/// \return the month, or nothing if name is not the name of one.
std::optional<Month> monthFromName(std::string_view name);  ///< the letters of the name

}  // namespace datetimeparser
