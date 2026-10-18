#pragma once

#include <string>

namespace deformation {

// GGRXF's own day-count representation for `dateString`, an ISO 8601 date or
// date-time in its extended form (yyyy-mm-dd, then optionally T or a space
// and hh:mm, hh:mm:ss or hh:mm:ss.s, then optionally Z or an offset such
// as +12:00), read by util/datetimeparser.h's parseIso8601. A fraction of a
// second is part of the day count. A time with an offset is converted to
// UTC, so 2020-01-01T00:30:00+12:00 is on 31 December 2019. A time with
// no offset is local time in a zone that the text does not give, so it is
// taken as written. Only ever meaningful via subtraction against another
// such day number (the real elapsed-day count between two dates), never on
// its own. Throws datetimeparser::DateTimeParseError (a
// std::invalid_argument), whose message quotes the string and says what is
// wrong with it, if dateString is not such a date or date-time.
double ggrxfDateToDayNumber(const std::string& dateString);

// The day count from startDate to endDate - ragged_segments' own
// query-day definition, and the same day-count basis
// scalar_with_time_function grids use too.
double ggrxfDaysBetween(const std::string& startDate, const std::string& endDate);

}  // namespace deformation
