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
#include <ctype.h>
#include "util/dateutil.h"

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

double snap_datetime_parse( const char *definition, const char *format )
{
    int ymdhmse[7] = { 0, 0, 0, 0, 0, 0, 0 };
    int minval[7] = { 1000, 1, 1, 0, 0, 0, 1 };
    int maxval[7] = { 4000, 12, 31, 24, 59, 59, 366 };
    int maxchars[7] = { 4, 2, 2, 2, 2, 2, 3 };
    const char *months = " JAN FEB MAR APR MAY JUN JUL AUG SEP OCT NOV DEC";
    const char *formatchars = "YMDhmsN";
    char buffer[16];
    const char *dp;
    const char *fp;
    int i;

    if( _stricmp(definition,"now") == 0 )
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

    if( strcmp(format,"Y")==0 )
    {
        char extra[2]={0};
        double years;
        double result=0.0;
        if( sscanf(definition,"%lf%1s",&years,extra) > 0 
            && extra[0]==0 && years > 1000.0 && years < 4000.0 )
        {
            result=year_as_snapdate(years);
        }
        return result;
    }

    dp = definition;

    /* For each field in the format */
    for( fp = format; *fp; fp++ )
    {
        const char *pfc;
        int idx;
        int ibuf;
        int nbuf;
        int isname;
        if( isspace(*fp)) continue;
        pfc = strchr(formatchars, *fp);
        if( ! pfc ) return 0.0;
        idx = pfc - formatchars;

        /* Find the beginning of the field */

        while( *dp )
        {
            if( isdigit(*dp)) break;
            if( idx == 1 && isalnum(*dp)) break;
            dp++;
        }
        ibuf = 0;

        nbuf = maxchars[idx];
        isname = 0;
        if( *dp && !isdigit(*dp))
        {
            buffer[0] = ' ';
            ibuf = 1;
            nbuf = 10;
            isname = 1;
        }

        while( *dp && isalnum(*dp) && ibuf < nbuf )
        {
            if( ! isdigit(*dp) && ! isname) break;
            if( isdigit(*dp) && isname) break;
            buffer[ibuf++] = *dp++;
        }
        buffer[ibuf] = 0;
        if( ! ibuf )
        {
            ymdhmse[idx]=0;
        }
        else if( isname )
        {
            const char *mptr;
            if( strlen( buffer ) < 4 ) return 0.0;
            buffer[4] = 0;
            mptr = strstr( months, buffer );
            if( ! mptr ) return 0.0;
            ymdhmse[idx] = (mptr-months)/4+1;
        }
        else
        {
            sscanf(buffer,"%d",&ymdhmse[idx]);
        }
    }
    if( ymdhmse[6] > 0 )
    {
        if( ymdhmse[6] > 366 ) return 0.0;
        ymdhmse[1] = ymdhmse[2] = 1;
    }
    for( i = 0; i < 5; i++ )
    {
        if( ymdhmse[i] < minval[i] || ymdhmse[i] > maxval[i] ) return 0.0;
    }
    return snap_datetime(ymdhmse[0],ymdhmse[1],ymdhmse[2],ymdhmse[3],ymdhmse[4],ymdhmse[5])+ymdhmse[6];
}

const char *date_as_string( double snapdate, char *format, char *buffer )
{
    static char datebuffer[MAX_DATE_LEN];
    if( ! buffer ) buffer=datebuffer;
    /* Current ignoring format */
    if( snapdate == UNDEFINED_DATE )
    {
        strcpy(buffer,"undefined");
    }
    else
    {
        int y,m,d,hh,mm,ss;
        date_as_ymdhms(snapdate,&y,&m,&d,&hh,&mm,&ss);
        sprintf(buffer,"%04d-%02d-%02d %02d:%02d:%02d",y,m,d,hh,mm,ss);
    }
    return buffer;
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
