#include "snapconfig.h"
/* dateutil.c

   Dates in SNAP are generally held as double values, integer julian day plus
   time as a fraction of a day.

   Routines to convert between day, month, and year date formats and
   Julian day number */

/*
   $Log: dateutil.c,v $
   Revision 1.2  2004/04/22 02:35:26  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:51:32  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <charconv>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include <boost/algorithm/string/predicate.hpp>

#include "util/snapctype.h"
#include "util/dateutil.h"
#include "util/fieldscanner.hpp"

#define SAME_DATE_TOLERANCE 1.0e-7

static long julian_day (int day, int month,int year)
{
    long jdn;

    jdn = (long) year * 367 + month * 275 / 9
          - (year + (month > 2)) * 7 / 4
          - ((year - (month < 3)) / 100 + 1) * 3 / 4 + day + 1721029L;

    return (jdn);
}


static void julian_date(long jdn, int *day, int *month, int *year)
{
    long lyear=0, lmonth=0, lday=0, temp_var;

    if( jdn > 0 )
    {
        temp_var = jdn - 1721119L;
        lyear = (4 * temp_var - 1) / 146097L;
        temp_var = 4 * temp_var - 1 - 146097L * lyear;
        lday = temp_var / 4;
        temp_var = (4 * lday + 3) / 1461;
        lday = 4 * lday + 3 - 1461 * temp_var;
        lday = (lday + 4) / 4;
        lmonth = (5 * lday - 3) / 153;
        lday = 5 * lday - 3 - 153 * lmonth;
        lday = (lday + 5) / 5;
        lyear = 100 * lyear + temp_var;
        if (lmonth < 10)
            lmonth += 3;
        else
        {
            lmonth -= 9;
            lyear++;
        }
    }

    if( year ) *year = (int) lyear;
    if( month) *month = (int) lmonth;
    if( day ) *day = (int) lday;
}

double snap_date( int year, int month, int day )
{
    return (double) julian_day( day, month, year );
}

double snap_datetime( int year, int month, int day, int hour, int min, int sec )
{
    return ((double) julian_day( day, month, year )) + (hour+min/60.0+sec/3600.0)/24.0;
}

double snap_yds( int year, int dayno, int secs )
{
    return snap_datetime( year, 1, dayno, 0, 0, secs );
}

double snap_datetime_now()
{
    time_t now;
    struct tm *ltime;
    time(&now);
    ltime = localtime(&now);
    return snap_datetime(
               ltime->tm_year+1900,ltime->tm_mon+1,ltime->tm_mday,
               ltime->tm_hour,ltime->tm_min,ltime->tm_sec);
}

double snap_datetime_parse( std::string_view definition, std::optional<std::string_view> format )
{
    int ymdhmse[7] = { 0, 0, 0, 0, 0, 0, 0 };
    const int minval[7] = { 1000, 1, 1, 0, 0, 0, 1 };
    const int maxval[7] = { 4000, 12, 31, 24, 59, 59, 366 };
    const std::size_t maxchars[7] = { 4, 2, 2, 2, 2, 2, 3 };
    const std::string_view months = " JAN FEB MAR APR MAY JUN JUL AUG SEP OCT NOV DEC";
    const std::string_view formatchars = "YMDhmsN";

    if( boost::algorithm::iequals( definition, "now" ) )
    {
        return snap_datetime_now();
    }

    if( ! format )
    {
        double result=snap_datetime_parse(definition, "YMDhms");
        if( ! result ) result=snap_datetime_parse(definition, "DMYhms");
        if( ! result ) result=snap_datetime_parse(definition, "Y");
        return result;
    }

    if( *format == "Y" )
    {
        char extra[2]={0};
        double years;
        double result=0.0;
        const std::string definitionText( definition );
        if( sscanf(definitionText.c_str(),"%lf%1s",&years,extra) > 0
            && extra[0]==0 && years > 1000.0 && years < 4000.0 )
        {
            result=year_as_snapdate(years);
        }
        return result;
    }

    std::string_view remaining = definition;

    /* For each field in the format */
    for( const char formatchar : *format )
    {
        if( ISSPACE(formatchar)) continue;
        const std::size_t idx = formatchars.find( formatchar );
        if( idx == std::string_view::npos ) return 0.0;

        /* Find the beginning of the field */

        while( ! remaining.empty() )
        {
            if( is_digit(remaining.front())) break;
            if( idx == 1 && ISALNUM(remaining.front())) break;
            remaining.remove_prefix( 1 );
        }
        std::string buffer;

        std::size_t nbuf = maxchars[idx];
        bool isname = false;
        if( ! remaining.empty() && ! is_digit(remaining.front()))
        {
            buffer = ' ';
            nbuf = 10;
            isname = true;
        }

        while( ! remaining.empty() && ISALNUM(remaining.front()) && buffer.size() < nbuf )
        {
            if( ! is_digit(remaining.front()) && ! isname) break;
            if( is_digit(remaining.front()) && isname) break;
            buffer.push_back( remaining.front() );
            remaining.remove_prefix( 1 );
        }
        if( buffer.empty() )
        {
            ymdhmse[idx]=0;
        }
        else if( isname )
        {
            if( buffer.size() < 4 ) return 0.0;
            const std::size_t monthpos = months.find( buffer.substr( 0, 4 ) );
            if( monthpos == std::string_view::npos ) return 0.0;
            ymdhmse[idx] = static_cast<int>( monthpos/4+1 );
        }
        else
        {
            std::from_chars( buffer.data(), buffer.data()+buffer.size(), ymdhmse[idx] );
        }
    }
    if( ymdhmse[6] > 0 )
    {
        if( ymdhmse[6] > 366 ) return 0.0;
        ymdhmse[1] = ymdhmse[2] = 1;
        ymdhmse[6] -= 1;
    }
    for( int i = 0; i < 5; i++ )
    {
        if( ymdhmse[i] < minval[i] || ymdhmse[i] > maxval[i] ) return 0.0;
    }
    return snap_datetime(ymdhmse[0],ymdhmse[1],ymdhmse[2],ymdhmse[3],ymdhmse[4],ymdhmse[5])+ymdhmse[6];
}

std::string date_as_string( const double snapdate, const DateStringFormat format )
{
    if( snapdate == UNDEFINED_DATE )
    {
        return "undefined";
    }
    int y=0;
    int m=0;
    int d=0;
    int hh=0;
    int mm=0;
    int ss=0;
    date_as_ymdhms(snapdate,&y,&m,&d,&hh,&mm,&ss);
    std::ostringstream text;
    text << std::setfill('0') << std::setw(4) << y << '-' << std::setw(2) << m << '-' << std::setw(2) << d;
    const bool midnight = hh==0 && mm==0 && ss==0;
    const bool printTime = format == DateStringFormat::dateTime ||
                           (format == DateStringFormat::timeIfNotMidnight && ! midnight);
    if( printTime )
    {
        text << ' ' << std::setw(2) << hh << ':' << std::setw(2) << mm << ':' << std::setw(2) << ss;
    }
    return text.str();
}


double date_as_year( double snapdate )
{
    static double refdate=0.0;
    static double yearlen=1.0;
    static double year0=0;
    double year;

    year = (snapdate-refdate)/yearlen;
    if( year < 0 || year > 1 )
    {
        int d, m, y;
        julian_date( (long) snapdate, &d, &m, &y );
        refdate = julian_day( 1, 1, y);
        yearlen = julian_day( 1, 1, y+1 ) - refdate;
        year0 = y;
        year = (snapdate - refdate)/yearlen;
    }
    year += year0;
    return year;
}

double year_as_snapdate( double years )
{
    double d0, d1;
    int y0;
    y0=(int)years;
    d0=snap_date(y0,1,1);
    d1=snap_date(y0+1,1,1);
    return d0+(d1-d0)*(years-y0);
}

int same_date( double date0, double date1 )
{
    return fabs(date0-date1) < SAME_DATE_TOLERANCE;
}

void date_as_ymd( double snapdate, int *year, int *month, int *day )
{
    julian_date( (long) snapdate, day, month, year );
}

void date_as_ymdhms( double snapdate, int *year, int *month, int *day, int *hour, int *min, int *sec )
{
    int h,m,s;
    date_as_ymd(snapdate,year,month,day);
    snapdate -= floor(snapdate);
    snapdate *= 24;
    h = (int) snapdate;
    snapdate = (snapdate - h)*60;
    m = (int) snapdate;
    snapdate = (snapdate - m)*60;
    s = (int) snapdate;
    if( hour ) *hour = h;
    if( min ) *min = m;
    if( sec ) *sec = s;
}


void date_as_yds( double snapdate, int *year, int *dayno, int *secs )
{
    int m,d,hr,mn,sc;
    double y0;
    date_as_ymdhms( snapdate,year,&m,&d,&hr,&mn,&sc);
    y0=snap_datetime(*year,1,1,hr,mn,sc);
    (*dayno)=(int)(snapdate+1.1-y0);
    (*secs)=hr*3600+mn*60+sc;
}
