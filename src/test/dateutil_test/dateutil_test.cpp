// Standalone unit test for the date and time parsing of snap_datetime_parse (util/dateutil.h). Awaiting migration to
// CTest - until then, failures are reported to stdout and the process exit code is the pass/fail signal (0 = all
// passed), matching how other test-only tools in src/test/ are checked.
//
// Each row gives a string, the format passed with it (nullptr for none) and the date that it must give, written as
// that date. A result must be within kToleranceDays of the date, and a string that is not a date must give exactly 0.
//
// Most rows were recorded from the calls that the regression test suites make, together with the calls that the
// default makes to itself for each of its formats, and these must give what they gave before. The rows after them
// are of forms that the regression tests do not use. They hold what a date and time are in ISO 8601: a day that is
// not in its month, a second of 60, an hour of 24 and text after a complete date or time are not accepted. A month
// name is accepted in any case. A time zone offset and a fraction of a second are accepted and ignored.

#include <cmath>
#include <cstdio>
#include <iostream>
#include <string_view>

#include "util/dateutil.h"

namespace {

struct ParseCase {
    const char* definition;
    const char* format;  ///< the format passed to snap_datetime_parse, or nullptr to pass none
    double expected;     ///< the date that definition is, as a snap date, or kInvalid
};

/// What snap_datetime_parse returns for a string that is not a date.
constexpr double kInvalid = 0.0;

/// How close a result must be to the date, in days: 0.1 millisecond. A snap date is about 2.4 million days, where
/// the spacing between doubles is about 0.04 millisecond.
constexpr double kToleranceDays = 0.0001 / 86400.0;

constexpr double kSecondsPerDay = 86400.0;

double date(const int year, const int month, const int day) {
    return snap_date(year, month, day);
}

double date_time(const int year, const int month, const int day, const int hour, const int minute,
                 const double second) {
    return snap_date(year, month, day) + (hour * 3600.0 + minute * 60.0 + second) / kSecondsPerDay;
}

/// The date that a decimal year such as 2005.2 gives.
double decimal_year(const double years) {
    return year_as_snapdate(years);
}

/// The date for a day of the year, counting 1 January as day 1.
double day_of_year(const int year, const int dayNumber, const int hour, const int minute) {
    return snap_date(year, 1, 1) + (dayNumber - 1) + (hour * 3600.0 + minute * 60.0) / kSecondsPerDay;
}

const ParseCase kCases[] = {
    // Recorded from the regression tests: no format, so the default formats are tried in turn.
    {"02-01-2005", nullptr, date(2005, 1, 2)},
    {"02-JAN-2005", nullptr, date(2005, 1, 2)},
    {"02012005", nullptr, date(2005, 1, 2)},
    {"1 DEC 1998", nullptr, date(1998, 12, 1)},
    {"1 JAN 2000", nullptr, date(2000, 1, 1)},
    {"1 JAN 2008", nullptr, date(2008, 1, 1)},
    {"1 JAN 2018", nullptr, date(2018, 1, 1)},
    {"1 JAN 2025", nullptr, date(2025, 1, 1)},
    {"1 JAN 2100", nullptr, date(2100, 1, 1)},
    {"1 JAN 2121", nullptr, date(2121, 1, 1)},
    {"1 JAN 2128", nullptr, date(2128, 1, 1)},
    {"1 JAN 2141", nullptr, date(2141, 1, 1)},
    {"1 JAN 2148", nullptr, date(2148, 1, 1)},
    {"1 JAN 2151", nullptr, date(2151, 1, 1)},
    {"1 JAN 2158", nullptr, date(2158, 1, 1)},
    {"1 JAN 3000", nullptr, date(3000, 1, 1)},
    {"1 MAR 2143", nullptr, date(2143, 3, 1)},
    {"1 MAR 2144", nullptr, date(2144, 3, 1)},
    {"1 MAY 1998", nullptr, date(1998, 5, 1)},
    {"1 MAY 2000", nullptr, date(2000, 5, 1)},
    {"1 MAY 2002", nullptr, date(2002, 5, 1)},
    {"1 MAY 2005", nullptr, date(2005, 5, 1)},
    {"1 MAY 2006", nullptr, date(2006, 5, 1)},
    {"1 MAY 2010", nullptr, date(2010, 5, 1)},
    {"14 APR 2009", nullptr, date(2009, 4, 14)},
    {"15 MAY 1999", nullptr, date(1999, 5, 15)},
    {"16 MAR 1909 12:20:58", nullptr, date_time(1909, 3, 16, 12, 20, 58)},
    {"18 JUL 2002", nullptr, date(2002, 7, 18)},
    {"19 AUG 2003", nullptr, date(2003, 8, 19)},
    {"19 AUGUST 2003", nullptr, date(2003, 8, 19)},
    {"1999-01-01", nullptr, date(1999, 1, 1)},
    {"1999-05-15T03:45:18", nullptr, date_time(1999, 5, 15, 3, 45, 18)},
    {"1999-12-01", nullptr, date(1999, 12, 1)},
    {"19991201", nullptr, date(1999, 12, 1)},
    {"1MAY2006", nullptr, date(2006, 5, 1)},
    {"20 09 2009", nullptr, date(2009, 9, 20)},
    {"20 SEP 2009", nullptr, date(2009, 9, 20)},
    {"2000", nullptr, decimal_year(2000)},
    {"2000-01-01", nullptr, date(2000, 1, 1)},
    {"2000-01-01T09:30:35", nullptr, date_time(2000, 1, 1, 9, 30, 35)},
    {"2000-01-15", nullptr, date(2000, 1, 15)},
    {"2000-02-01", nullptr, date(2000, 2, 1)},
    {"2000-02-15", nullptr, date(2000, 2, 15)},
    {"2000-03-15", nullptr, date(2000, 3, 15)},
    {"2000-04-01", nullptr, date(2000, 4, 1)},
    {"2000-04-15", nullptr, date(2000, 4, 15)},
    {"2000.0", nullptr, decimal_year(2000.0)},
    {"20001201", nullptr, date(2000, 12, 1)},
    {"2001-01-01", nullptr, date(2001, 1, 1)},
    {"2001-01-02", nullptr, date(2001, 1, 2)},
    {"2001-01-15", nullptr, date(2001, 1, 15)},
    {"2001-02-01", nullptr, date(2001, 2, 1)},
    {"2001-02-03", nullptr, date(2001, 2, 3)},
    {"2001-02-15", nullptr, date(2001, 2, 15)},
    {"2001-03-02", nullptr, date(2001, 3, 2)},
    {"2001-03-15", nullptr, date(2001, 3, 15)},
    {"2001-04-03", nullptr, date(2001, 4, 3)},
    {"2001-04-15", nullptr, date(2001, 4, 15)},
    {"2001/01/01", nullptr, date(2001, 1, 1)},
    {"20011201", nullptr, date(2001, 12, 1)},
    {"2004-12-25", nullptr, date(2004, 12, 25)},
    {"2005", nullptr, decimal_year(2005)},
    {"2005-01-01", nullptr, date(2005, 1, 1)},
    {"2005-01-02", nullptr, date(2005, 1, 2)},
    {"2005-01-15", nullptr, date(2005, 1, 15)},
    {"2005-02-01", nullptr, date(2005, 2, 1)},
    {"2005-02-15", nullptr, date(2005, 2, 15)},
    {"2005-03-01", nullptr, date(2005, 3, 1)},
    {"2005-03-02", nullptr, date(2005, 3, 2)},
    {"2005-03-07", nullptr, date(2005, 3, 7)},
    {"2005-03-15", nullptr, date(2005, 3, 15)},
    {"2005-03-22", nullptr, date(2005, 3, 22)},
    {"2005-03-30", nullptr, date(2005, 3, 30)},
    {"2005.2", nullptr, decimal_year(2005.2)},
    {"20050102", nullptr, date(2005, 1, 2)},
    {"2006-09-07", nullptr, date(2006, 9, 7)},
    {"2006-10-02", nullptr, date(2006, 10, 2)},
    {"2006-11-07", nullptr, date(2006, 11, 7)},
    {"2008", nullptr, decimal_year(2008)},
    {"2008-01-01", nullptr, date(2008, 1, 1)},
    {"2009 09 20", nullptr, date(2009, 9, 20)},
    {"2009 FEB 13", nullptr, date(2009, 2, 13)},
    {"2009-01-01", nullptr, date(2009, 1, 1)},
    {"2009-09-12", nullptr, date(2009, 9, 12)},
    {"20090401122003", nullptr, date_time(2009, 4, 1, 12, 20, 3)},
    {"2010", nullptr, decimal_year(2010)},
    {"2010-01-01T09:30:35", nullptr, date_time(2010, 1, 1, 9, 30, 35)},
    {"20140913", nullptr, date(2014, 9, 13)},
    {"2015-01-02", nullptr, date(2015, 1, 2)},
    {"2015-06-01", nullptr, date(2015, 6, 1)},
    {"20150601", nullptr, date(2015, 6, 1)},
    {"2018-01-01", nullptr, date(2018, 1, 1)},
    {"2020", nullptr, decimal_year(2020)},
    {"2020.5", nullptr, decimal_year(2020.5)},
    {"21 OCT 2010", nullptr, date(2010, 10, 21)},
    {"22 NOV 2009", nullptr, date(2009, 11, 22)},
    {"23 DEC 2009", nullptr, date(2009, 12, 23)},
    {"2JAN1889", nullptr, date(1889, 1, 2)},
    {"5 MAY 2003", nullptr, date(2003, 5, 5)},
    {"7 JUN 2009", nullptr, date(2009, 6, 7)},
    {"notadate", nullptr, kInvalid},

    // Recorded from the regression tests: formats that callers give.
    {"2010.03.05", "YMD", date(2010, 3, 5)},
    {"2010.04.03", "YMD", date(2010, 4, 3)},
    {"2010.12.25", "YMD", date(2010, 12, 25)},
    {"2010.249 00.15", "YNhm", day_of_year(2010, 249, 0, 15)},
    {"2010.249 00.22", "YNhm", day_of_year(2010, 249, 0, 22)},
    {"2010.249 00.28", "YNhm", day_of_year(2010, 249, 0, 28)},
    {"2010.279 19.46", "YNhm", day_of_year(2010, 279, 19, 46)},
    {"2010.279 19.59", "YNhm", day_of_year(2010, 279, 19, 59)},
    {"2010.279 20.06", "YNhm", day_of_year(2010, 279, 20, 6)},
    {"2010.279 20.12", "YNhm", day_of_year(2010, 279, 20, 12)},

    // Recorded from the regression tests: the formats that the default tries in turn, each given on its own.
    {"02-01-2005", "DMYhms", date(2005, 1, 2)},
    {"02-01-2005", "YMDhms", kInvalid},
    {"02-JAN-2005", "DMYhms", date(2005, 1, 2)},
    {"02-JAN-2005", "YMDhms", kInvalid},
    {"02012005", "DMYhms", date(2005, 1, 2)},
    {"02012005", "YMDhms", kInvalid},
    {"1 DEC 1998", "DMYhms", date(1998, 12, 1)},
    {"1 DEC 1998", "YMDhms", kInvalid},
    {"1 JAN 2000", "DMYhms", date(2000, 1, 1)},
    {"1 JAN 2000", "YMDhms", kInvalid},
    {"1 JAN 2008", "DMYhms", date(2008, 1, 1)},
    {"1 JAN 2008", "YMDhms", kInvalid},
    {"1 JAN 2018", "DMYhms", date(2018, 1, 1)},
    {"1 JAN 2018", "YMDhms", kInvalid},
    {"1 JAN 2025", "DMYhms", date(2025, 1, 1)},
    {"1 JAN 2025", "YMDhms", kInvalid},
    {"1 JAN 2100", "DMYhms", date(2100, 1, 1)},
    {"1 JAN 2100", "YMDhms", kInvalid},
    {"1 JAN 2121", "DMYhms", date(2121, 1, 1)},
    {"1 JAN 2121", "YMDhms", kInvalid},
    {"1 JAN 2128", "DMYhms", date(2128, 1, 1)},
    {"1 JAN 2128", "YMDhms", kInvalid},
    {"1 JAN 2141", "DMYhms", date(2141, 1, 1)},
    {"1 JAN 2141", "YMDhms", kInvalid},
    {"1 JAN 2148", "DMYhms", date(2148, 1, 1)},
    {"1 JAN 2148", "YMDhms", kInvalid},
    {"1 JAN 2151", "DMYhms", date(2151, 1, 1)},
    {"1 JAN 2151", "YMDhms", kInvalid},
    {"1 JAN 2158", "DMYhms", date(2158, 1, 1)},
    {"1 JAN 2158", "YMDhms", kInvalid},
    {"1 JAN 3000", "DMYhms", date(3000, 1, 1)},
    {"1 JAN 3000", "YMDhms", kInvalid},
    {"1 MAR 2143", "DMYhms", date(2143, 3, 1)},
    {"1 MAR 2143", "YMDhms", kInvalid},
    {"1 MAR 2144", "DMYhms", date(2144, 3, 1)},
    {"1 MAR 2144", "YMDhms", kInvalid},
    {"1 MAY 1998", "DMYhms", date(1998, 5, 1)},
    {"1 MAY 1998", "YMDhms", kInvalid},
    {"1 MAY 2000", "DMYhms", date(2000, 5, 1)},
    {"1 MAY 2000", "YMDhms", kInvalid},
    {"1 MAY 2002", "DMYhms", date(2002, 5, 1)},
    {"1 MAY 2002", "YMDhms", kInvalid},
    {"1 MAY 2005", "DMYhms", date(2005, 5, 1)},
    {"1 MAY 2005", "YMDhms", kInvalid},
    {"1 MAY 2006", "DMYhms", date(2006, 5, 1)},
    {"1 MAY 2006", "YMDhms", kInvalid},
    {"1 MAY 2010", "DMYhms", date(2010, 5, 1)},
    {"1 MAY 2010", "YMDhms", kInvalid},
    {"14 APR 2009", "DMYhms", date(2009, 4, 14)},
    {"14 APR 2009", "YMDhms", kInvalid},
    {"15 MAY 1999", "DMYhms", date(1999, 5, 15)},
    {"15 MAY 1999", "YMDhms", kInvalid},
    {"16 MAR 1909 12:20:58", "DMYhms", date_time(1909, 3, 16, 12, 20, 58)},
    {"16 MAR 1909 12:20:58", "YMDhms", kInvalid},
    {"18 JUL 2002", "DMYhms", date(2002, 7, 18)},
    {"18 JUL 2002", "YMDhms", kInvalid},
    {"19 AUG 2003", "DMYhms", date(2003, 8, 19)},
    {"19 AUG 2003", "YMDhms", kInvalid},
    {"19 AUGUST 2003", "DMYhms", date(2003, 8, 19)},
    {"19 AUGUST 2003", "YMDhms", kInvalid},
    {"1999-01-01", "YMDhms", date(1999, 1, 1)},
    {"1999-05-15T03:45:18", "YMDhms", date_time(1999, 5, 15, 3, 45, 18)},
    {"1999-12-01", "YMDhms", date(1999, 12, 1)},
    {"19991201", "YMDhms", date(1999, 12, 1)},
    {"1MAY2006", "DMYhms", date(2006, 5, 1)},
    {"1MAY2006", "YMDhms", kInvalid},
    {"20 09 2009", "DMYhms", date(2009, 9, 20)},
    {"20 09 2009", "YMDhms", kInvalid},
    {"20 SEP 2009", "DMYhms", date(2009, 9, 20)},
    {"20 SEP 2009", "YMDhms", kInvalid},
    {"2000", "DMYhms", kInvalid},
    {"2000", "Y", decimal_year(2000)},
    {"2000", "YMDhms", kInvalid},
    {"2000-01-01", "YMDhms", date(2000, 1, 1)},
    {"2000-01-01T09:30:35", "YMDhms", date_time(2000, 1, 1, 9, 30, 35)},
    {"2000-01-15", "YMDhms", date(2000, 1, 15)},
    {"2000-02-01", "YMDhms", date(2000, 2, 1)},
    {"2000-02-09", "YMDhms", date(2000, 2, 9)},
    {"2000-02-15", "YMDhms", date(2000, 2, 15)},
    {"2000-03-15", "YMDhms", date(2000, 3, 15)},
    {"2000-04-01", "YMDhms", date(2000, 4, 1)},
    {"2000-04-15", "YMDhms", date(2000, 4, 15)},
    {"2000.0", "DMYhms", kInvalid},
    {"2000.0", "Y", decimal_year(2000.0)},
    {"2000.0", "YMDhms", kInvalid},
    {"20001201", "YMDhms", date(2000, 12, 1)},
    {"2001-01-01", "YMDhms", date(2001, 1, 1)},
    {"2001-01-02", "YMDhms", date(2001, 1, 2)},
    {"2001-01-15", "YMDhms", date(2001, 1, 15)},
    {"2001-02-01", "YMDhms", date(2001, 2, 1)},
    {"2001-02-03", "YMDhms", date(2001, 2, 3)},
    {"2001-02-15", "YMDhms", date(2001, 2, 15)},
    {"2001-03-02", "YMDhms", date(2001, 3, 2)},
    {"2001-03-15", "YMDhms", date(2001, 3, 15)},
    {"2001-04-03", "YMDhms", date(2001, 4, 3)},
    {"2001-04-15", "YMDhms", date(2001, 4, 15)},
    {"2001-11-30", "YMDhms", date(2001, 11, 30)},
    {"2001/01/01", "YMDhms", date(2001, 1, 1)},
    {"20011201", "YMDhms", date(2001, 12, 1)},
    {"2004-12-25", "YMDhms", date(2004, 12, 25)},
    {"2005", "DMYhms", kInvalid},
    {"2005", "Y", decimal_year(2005)},
    {"2005", "YMDhms", kInvalid},
    {"2005-01-01", "YMDhms", date(2005, 1, 1)},
    {"2005-01-02", "YMDhms", date(2005, 1, 2)},
    {"2005-01-15", "YMDhms", date(2005, 1, 15)},
    {"2005-02-01", "YMDhms", date(2005, 2, 1)},
    {"2005-02-15", "YMDhms", date(2005, 2, 15)},
    {"2005-03-01", "YMDhms", date(2005, 3, 1)},
    {"2005-03-02", "YMDhms", date(2005, 3, 2)},
    {"2005-03-07", "YMDhms", date(2005, 3, 7)},
    {"2005-03-15", "YMDhms", date(2005, 3, 15)},
    {"2005-03-22", "YMDhms", date(2005, 3, 22)},
    {"2005-03-30", "YMDhms", date(2005, 3, 30)},
    {"2005.2", "DMYhms", kInvalid},
    {"2005.2", "Y", decimal_year(2005.2)},
    {"2005.2", "YMDhms", kInvalid},
    {"20050102", "YMDhms", date(2005, 1, 2)},
    {"2006-09-07", "YMDhms", date(2006, 9, 7)},
    {"2006-10-02", "YMDhms", date(2006, 10, 2)},
    {"2006-11-07", "YMDhms", date(2006, 11, 7)},
    {"2008", "DMYhms", kInvalid},
    {"2008", "Y", decimal_year(2008)},
    {"2008", "YMDhms", kInvalid},
    {"2008-01-01", "YMDhms", date(2008, 1, 1)},
    {"2008-01-01 12:00:00", "YMDhms", date_time(2008, 1, 1, 12, 0, 0)},
    {"2008-03-12", "YMDhms", date(2008, 3, 12)},
    {"2009 09 20", "YMDhms", date(2009, 9, 20)},
    {"2009 FEB 13", "YMDhms", date(2009, 2, 13)},
    {"2009-01-01", "YMDhms", date(2009, 1, 1)},
    {"2009-09-12", "YMDhms", date(2009, 9, 12)},
    {"2009-12-23 12:00:00", "YMDhms", date_time(2009, 12, 23, 12, 0, 0)},
    {"20090401122003", "YMDhms", date_time(2009, 4, 1, 12, 20, 3)},
    {"2010", "DMYhms", kInvalid},
    {"2010", "Y", decimal_year(2010)},
    {"2010", "YMDhms", kInvalid},
    {"2010-01-01T09:30:35", "YMDhms", date_time(2010, 1, 1, 9, 30, 35)},
    {"2010-05-01", "YMDhms", date(2010, 5, 1)},
    {"20140913", "YMDhms", date(2014, 9, 13)},
    {"2015-01-02", "YMDhms", date(2015, 1, 2)},
    {"2015-06-01", "YMDhms", date(2015, 6, 1)},
    {"20150601", "YMDhms", date(2015, 6, 1)},
    {"2018-01-01", "YMDhms", date(2018, 1, 1)},
    {"2020", "DMYhms", kInvalid},
    {"2020", "Y", decimal_year(2020)},
    {"2020", "YMDhms", kInvalid},
    {"2020.5", "DMYhms", kInvalid},
    {"2020.5", "Y", decimal_year(2020.5)},
    {"2020.5", "YMDhms", kInvalid},
    {"21 OCT 2010", "DMYhms", date(2010, 10, 21)},
    {"21 OCT 2010", "YMDhms", kInvalid},
    {"22 NOV 2009", "DMYhms", date(2009, 11, 22)},
    {"22 NOV 2009", "YMDhms", kInvalid},
    {"23 DEC 2009", "DMYhms", date(2009, 12, 23)},
    {"23 DEC 2009", "YMDhms", kInvalid},
    {"2JAN1889", "DMYhms", date(1889, 1, 2)},
    {"2JAN1889", "YMDhms", kInvalid},
    {"5 MAY 2003", "DMYhms", date(2003, 5, 5)},
    {"5 MAY 2003", "YMDhms", kInvalid},
    {"7 JUN 2009", "DMYhms", date(2009, 6, 7)},
    {"7 JUN 2009", "YMDhms", kInvalid},
    {"notadate", "DMYhms", kInvalid},
    {"notadate", "Y", kInvalid},
    {"notadate", "YMDhms", kInvalid},

    // Forms that the regression tests do not use, and that were accepted before and still are.
    {"2024-01-01T00:00:00Z", nullptr, date(2024, 1, 1)},
    {"2024-01-01 12:34:56", nullptr, date_time(2024, 1, 1, 12, 34, 56)},
    {"2024-01-01T12:34", nullptr, date_time(2024, 1, 1, 12, 34, 0)},
    {"2024-01-01T12", nullptr, date_time(2024, 1, 1, 12, 0, 0)},
    {"2024-01-01T23:59:59", nullptr, date_time(2024, 1, 1, 23, 59, 59)},
    {"2024-02-29", nullptr, date(2024, 2, 29)},
    {"2000-02-29", nullptr, date(2000, 2, 29)},
    {"1000-01-01", nullptr, date(1000, 1, 1)},
    {"4000-12-31", nullptr, date(4000, 12, 31)},
    {"2024/01/01", nullptr, date(2024, 1, 1)},
    {"2024.01.01", nullptr, date(2024, 1, 1)},
    {"2024 01 01", nullptr, date(2024, 1, 1)},
    {"20240101", nullptr, date(2024, 1, 1)},
    {"2024-1-1", nullptr, date(2024, 1, 1)},
    {"2024-1-01", nullptr, date(2024, 1, 1)},
    {"1-1-2024", nullptr, date(2024, 1, 1)},
    {"01/01/2024", nullptr, date(2024, 1, 1)},
    {"31-12-2023", nullptr, date(2023, 12, 31)},
    {"1 JANUARY 2000", nullptr, date(2000, 1, 1)},
    {"2000 JAN 1", nullptr, date(2000, 1, 1)},
    {"2010.249", "YN", day_of_year(2010, 249, 0, 0)},
    {"2012 366", "YN", day_of_year(2012, 366, 0, 0)},
    {"2010.001 12:30", "YNhm", day_of_year(2010, 1, 12, 30)},
    {"2000.5", nullptr, decimal_year(2000.5)},
    {"2000.25", nullptr, decimal_year(2000.25)},
    {"1999.999", nullptr, decimal_year(1999.999)},
    {"2000.", nullptr, decimal_year(2000.0)},
    {" 2024-01-01", nullptr, date(2024, 1, 1)},
    {"2024-01-01 ", nullptr, date(2024, 1, 1)},
    {"2024-01-01", "YMD", date(2024, 1, 1)},
    {"01-01-2024", "DMY", date(2024, 1, 1)},
    {"2024-01-01 12:34", "YMDhm", date_time(2024, 1, 1, 12, 34, 0)},

    // Forms that were not accepted before and still are not.
    {"2024-01-01T23:60:00", nullptr, kInvalid},
    {"2024-13-01", nullptr, kInvalid},
    {"2024-00-10", nullptr, kInvalid},
    {"2024-01-00", nullptr, kInvalid},
    {"2024-01-32", nullptr, kInvalid},
    {"0999-12-31", nullptr, kInvalid},
    {"4001-01-01", nullptr, kInvalid},
    {"240101", nullptr, kInvalid},
    {"12-31-2023", nullptr, kInvalid},
    {"1 XYZ 2000", nullptr, kInvalid},
    {"1 JA 2000", nullptr, kInvalid},
    {"JAN 1 2000", nullptr, kInvalid},
    {"2010 367", "YN", kInvalid},
    {"2010.000", "YN", kInvalid},
    {"999.5", nullptr, kInvalid},
    {"4000.5", nullptr, kInvalid},
    {".5", nullptr, kInvalid},
    {"2000.0.0", nullptr, kInvalid},
    {"2000abc", nullptr, kInvalid},
    {" ", nullptr, kInvalid},
    {"", nullptr, kInvalid},
    {"2024-01-01", "Q", kInvalid},
    {"2024-01-01", "", kInvalid},

    // Forms that were not accepted before, because a month name had to be in capitals. The help says that a month name
    // can be in any case, in the #date command (which made it capitals itself) and for the date format of a CSV file.
    {"1 jan 2000", nullptr, date(2000, 1, 1)},
    {"1 Jan 2000", nullptr, date(2000, 1, 1)},
    {"13 Jan 2008", nullptr, date(2008, 1, 13)},
    {"13-Jan-2008", nullptr, date(2008, 1, 13)},
    {"13-Jan-2008 12:50", nullptr, date_time(2008, 1, 13, 12, 50, 0)},
    {"2009 feb 13", nullptr, date(2009, 2, 13)},
    {"19 August 2003", nullptr, date(2003, 8, 19)},
    {"25 dec 2011 7 15", "DMYhm", date_time(2011, 12, 25, 7, 15, 0)},
    {"25 december 2011 7 15", "DMYhm", date_time(2011, 12, 25, 7, 15, 0)},
    {"25/12/2011 07:15", "DMYhm", date_time(2011, 12, 25, 7, 15, 0)},

    // Forms that were accepted before, and are not dates.
    {"2024-01-01T24:00:00", nullptr, kInvalid},
    {"2024-01-01T12:34:60", nullptr, kInvalid},
    {"2023-02-29", nullptr, kInvalid},
    {"1900-02-29", nullptr, kInvalid},
    {"2024-02-30", nullptr, kInvalid},
    {"2024-02-31", nullptr, kInvalid},
    {"2024-04-31", nullptr, kInvalid},
    {"2010 366", "YN", kInvalid},
    {"2024-01-01 junk", nullptr, kInvalid},
    {"2024-01-01T00:00:00 extra", nullptr, kInvalid},
    {"2024-01-01Z", nullptr, kInvalid},

    // A time zone offset or a fraction of a second is accepted, and is ignored: the time is the one written, as
    // snap_datetime_parse has always taken it.
    {"2024-01-01T12:34:56.5", nullptr, date_time(2024, 1, 1, 12, 34, 56)},
    {"2024-01-01T12:34:56.789", nullptr, date_time(2024, 1, 1, 12, 34, 56)},
    {"2024-01-01T12:34:56+12:00", nullptr, date_time(2024, 1, 1, 12, 34, 56)},
    {"2024-01-01T12:34:56-05:30", nullptr, date_time(2024, 1, 1, 12, 34, 56)},
    {"2024-01-01T12:34:56+1200", nullptr, date_time(2024, 1, 1, 12, 34, 56)},
    {"2024-01-01T00:30:00+12:00", nullptr, date_time(2024, 1, 1, 0, 30, 0)},
};

int failures = 0;

void check_parse(const ParseCase& parseCase) {
    const double actual = parseCase.format
                              ? snap_datetime_parse(parseCase.definition, std::string_view(parseCase.format))
                              : snap_datetime_parse(parseCase.definition);
    const bool matches =
        parseCase.expected == kInvalid ? actual == kInvalid : std::fabs(actual - parseCase.expected) <= kToleranceDays;
    if (matches) {
        return;
    }
    ++failures;
    std::printf("FAIL: snap_datetime_parse(\"%s\", %s%s%s): expected %.9f got %.9f\n", parseCase.definition,
                parseCase.format ? "\"" : "", parseCase.format ? parseCase.format : "none",
                parseCase.format ? "\"" : "", parseCase.expected, actual);
}

// "now" gives the time that it is called, so it can only be checked for being a recent date.
void check_now() {
    for (const char* definition : {"now", "NOW", "Now"}) {
        if (snap_datetime_parse(definition) < date(2024, 1, 1)) {
            ++failures;
            std::printf("FAIL: snap_datetime_parse(\"%s\") is not the current date\n", definition);
        }
    }
}

}  // namespace

int main() {
    for (const ParseCase& parseCase : kCases) {
        check_parse(parseCase);
    }
    check_now();

    if (failures > 0) {
        std::cout << failures << " dateutil_test check(s) failed\n";
        return 1;
    }
    std::cout << "dateutil_test: all checks passed\n";
    return 0;
}
