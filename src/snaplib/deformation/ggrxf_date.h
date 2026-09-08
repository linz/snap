#pragma once

#include <string>

namespace deformation {

// GGRXF's own day-count representation for `dateString` (an ISO-8601 date
// or date-time string) - reuses snap's own general date utility
// (util/dateutil.h's snap_datetime_parse), which already parses this
// shape correctly (confirmed against real ISO-8601 date and date-time
// strings, including century-boundary leap-year cases). Only ever
// meaningful via subtraction against another such day number (the real
// elapsed-day count between two dates), never on its own. Throws
// std::invalid_argument, naming the actual string, if dateString can't be
// parsed - snap_datetime_parse itself returns 0.0 on a parse failure
// rather than throwing, too implicit for this module's own fail-loud
// convention, and 0.0 (an ancient proleptic Julian day) is never a
// plausible real GGRXF date, so treating it as a parse failure is safe.
double ggrxfDateToDayNumber( const std::string &dateString );

// The day count from startDate to endDate - ragged_segments' own
// query-day definition, and the same day-count basis
// scalar_with_time_function grids use too.
double ggrxfDaysBetween( const std::string &startDate, const std::string &endDate );

} // namespace deformation
