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

#define UNDEFINED_DATE     0.0

/* Snap uses dates as double day number */

double snap_date( int year, int month, int day );
double snap_datetime( int year, int month, int day, int hour, int min, int sec );
double snap_yds( int year, int dayno, int secs );
double snap_datetime_now();
/// Parses a date or date time, returning UNDEFINED_DATE if it is not valid.
/// If no format is given the default formats are tried in turn.
double snap_datetime_parse( std::string_view definition, std::optional<std::string_view> format = std::nullopt );
int same_date( double date0, double date1 );

/* Conversion to other date formats */
double date_as_year( double snapdate );
double year_as_snapdate( double years );
void date_as_ymd( double snapdate, int *year, int *month, int *day );
void date_as_ymdhms( double snapdate, int *year, int *month, int *day, int *hour, int *min, int *sec );
void date_as_yds( double snapdate, int *year, int *dayno, int *secs );

/// How date_as_string formats a date.
enum class DateStringFormat
{
    dateTime,           ///< Date and time, yyyy-mm-dd hh:mm:ss
    dateOnly,           ///< Date only, yyyy-mm-dd
    timeIfNotMidnight   ///< As dateTime, but the time is omitted if it is 00:00:00
};

/// Formats a date as text, or "undefined" if it is UNDEFINED_DATE.
std::string date_as_string( double snapdate, DateStringFormat format = DateStringFormat::dateTime );

#endif
