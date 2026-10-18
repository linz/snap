#ifndef _DATEUTIL_H
#define _DATEUTIL_H

/*
   $Log: dateutil.h,v $
   Revision 1.2  2004/04/22 02:35:26  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:52:02  CHRIS
   Initial revision

*/

/* Header file for dateutil.c - SNAP date functions */

#include <optional>
#include <string>
#include <string_view>

#define DAYS_PER_YEAR 365.25

/* Unspecified date */

#define UNDEFINED_DATE 0.0

/* Snap uses dates as double day number */

double snap_date(int year, int month, int day);
double snap_datetime(int year, int month, int day, int hour, int min, int sec);
double snap_yds(int year, int dayno, int secs);
double snap_datetime_now();
/// Reads a date or a date and time from text. The text is read by datetimeparser::tryParse() (or by
/// datetimeparser::tryParseDecimalYear() for the format Y), which describes the formats. In short, the format is a
/// string of letters that gives the order of the fields in the text: Y the year, M the month, D the day of the month,
/// h the hour, m the minute and s the second, as in YMDhms or DMYhms. The letter N, the day of the year (1 January is
/// 1), stands for the month and day together, so it takes the place of M and D, as in YNhm. The fields may be
/// separated by any characters or by none. The format Y alone is a decimal year, such as 2005.2. A month may be a
/// number or a case-insensitive name, such as JAN, Jan or January. Leading and trailing spaces are accepted. The text
/// "now", which is also case-insensitive, is the current local date and time.
///
/// A date that does not exist is not valid, such as 30 February or 29 February in a year that is not a leap year, nor
/// is an hour of 24, a second of 60, or any text left after the date and time.
///
/// A time zone offset (such as +12:00) and a fraction of a second after the seconds are accepted but ignored: the time is
/// taken as it is written, and is not converted to UTC.
///
/// If no format is given, the formats YMDhms, DMYhms and Y are tried in turn, and the first that reads the text is used.
///
/// \return the date as a SNAP day number, or UNDEFINED_DATE if the text is not valid.
double snap_datetime_parse(std::string_view definition,                            ///< the text to read
                           std::optional<std::string_view> format = std::nullopt);  ///< the order of its fields
int same_date(double date0, double date1);

/* Conversion to other date formats */
double date_as_year(double snapdate);
double year_as_snapdate(double years);
void date_as_ymd(double snapdate, int* year, int* month, int* day);
void date_as_ymdhms(double snapdate, int* year, int* month, int* day, int* hour, int* min, int* sec);
void date_as_yds(double snapdate, int* year, int* dayno, int* secs);

/// How date_as_string formats a date.
enum class DateStringFormat {
    dateTime,          ///< Date and time, yyyy-mm-dd hh:mm:ss
    dateOnly,          ///< Date only, yyyy-mm-dd
    timeIfNotMidnight  ///< As dateTime, but the time is omitted if it is 00:00:00
};

/// Formats a date as text, or "undefined" if it is UNDEFINED_DATE.
std::string date_as_string(double snapdate, DateStringFormat format = DateStringFormat::dateTime);

#endif
