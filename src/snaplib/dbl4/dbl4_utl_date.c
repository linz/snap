/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
** $Id: dbl4_utl_date.c,v 1.5 2005/04/04 23:57:58 ccrook Exp $
**//**
** \file
**      Utility functions relating to DateTimeType
**
*************************************************************************
*/

static char sccsid[] = "%W%";

#include "dbl4_common.h"

#include <stdio.h>

#include "dbl4_utl_date.h"
#include "dbl4_utl_error.h"

/*************************************************************************
** Function name: utlSetDate
**//**
**    Stores a date in a DateTimeType object
**
**  \param dt                  The date to set
**  \param year                The year (4 digit)
**  \param month               The month (1-12)
**  \param day                 The day (1-31)
**
**  \return
**
**************************************************************************
*/

void utlSetDate( DateTimeType *dt, int year, int month, int day)
{
    utlSetDateTime( dt, year, month, day, 0, 0, 0.0 );
}


/*************************************************************************
** Function name: utlSetDateTime
**//**
**    Stores a date and time in a DateTimeType
**
**  \param dt                  The date to set
**  \param year                The year (4 digit)
**  \param month               The month (1-12)
**  \param day                 The day (1-31)
**  \param hour                The hour (0-23)
**  \param min                 The minute (0-59)
**  \param sec                 The second (0-60)
**
**  \return
**
**************************************************************************
*/

void utlSetDateTime( DateTimeType *dt, int year, int month, int day, int hour,
                     int min, float sec)
{
    dt->dtYear = year;
    dt->dtMon = month;
    dt->dtDay = day;
    dt->dtHour = hour;
    dt->dtMin = min;
    dt->dtSec = sec;
    dt->years = 0.0;
}


/*************************************************************************
** Function name: utlCopyDate
**//**
**    Copies a date time type.
**
**  \param tgt                 The destination for the date
**  \param src                 The source for the date
**
**  \return
**
**************************************************************************
*/

void utlCopyDate( DateTimeType *tgt, DateTimeType *src)
{
    tgt->dtYear = src->dtYear;
    tgt->dtMon  = src->dtMon;
    tgt->dtDay  = src->dtDay;
    tgt->dtHour = src->dtHour;
    tgt->dtMin  = src->dtMin;
    tgt->dtSec  = src->dtSec;
    tgt->years = src->years;
}



/*************************************************************************
** Function name: utlParseDate
**//**
**    Copies a date time type.
**
**  \param tgt                 The destination for the date
**  \param str                 The source for the date
**
**  \return                    STS_INVALID_DATA if the string
**                             cannot be parsed.
**
**************************************************************************
*/

StatusType utlParseDate( DateTimeType *tgt, char *str)
{
    tgt->dtYear = tgt->dtMon = tgt->dtDay = 0;
    tgt->dtHour = tgt->dtMin = 0;
    tgt->dtSec = 0.0;
    tgt->years = 0.0;
    if( sscanf(str,"%hd/%hd/%hd",
               &tgt->dtMon,&tgt->dtDay,&tgt->dtYear) != 3 &&
            sscanf(str,"%hd-%hd-%hd %hd:%hd:%f",
                   &tgt->dtYear, &tgt->dtMon,&tgt->dtDay,
                   &tgt->dtHour, &tgt->dtMin, &tgt->dtSec)
            != 6 )
    {
        RETURN_STATUS(STS_INVALID_DATA);
    }

    return STS_OK;
}


/*************************************************************************
** Function name: utlCompareDate
**//**
**    Compares two dates, and returns 1 if the first is greater, -1 if the
**    second is greater, and 0 if they are the same.
**
**  \param dt1                 The first date
**  \param dt2                 The second date
**
**  \return                    Returns 1, 0, or -1
**
**************************************************************************
*/

int utlCompareDate( DateTimeType *dt1, DateTimeType *dt2 )
{
    if( dt1->dtYear > dt2->dtYear ) return 1;
    else if( dt1->dtYear < dt2->dtYear ) return -1;

    else if( dt1->dtMon > dt2->dtMon ) return 1;
    else if( dt1->dtMon < dt2->dtMon ) return -1;

    else if( dt1->dtDay > dt2->dtDay ) return 1;
    else if( dt1->dtDay < dt2->dtDay ) return -1;

    else if( dt1->dtHour > dt2->dtHour ) return 1;
    else if( dt1->dtHour < dt2->dtHour ) return -1;

    else if( dt1->dtMin > dt2->dtMin ) return 1;
    else if( dt1->dtMin < dt2->dtMin ) return -1;

    else if( dt1->dtSec > dt2->dtSec ) return 1;
    else if( dt1->dtSec < dt2->dtSec ) return -1;

    return 0;
}



/*************************************************************************
** Function name: utlIsLeapYear
**//**
**    Returns 1 if a year is a leap year, 0 otherwise.
**
**  \param year                The year number
**
**  \return                    The result
**
**************************************************************************
*/

int utlIsLeapYear( int year )
{
    if( year % 4 != 0 ) return 0;
    if( year % 100 != 0 ) return 1;
    if( year % 400 != 0 ) return 0;
    return 1;
}


/*************************************************************************
** Function name: utlDateAsYear
**//**
**    Converts a date to a decimal year number.  The result is cached into
**    the date object.
**
**  \param date                The date
**
**  \return                    The decimal year number
**
**************************************************************************
*/


double utlDateAsYear( DateTimeType *date )
{
    static int monthday[] = {0,31,59,90,120,151,181,212,243,273,304,334};
    int mon = date->dtMon;
    double days;
    double ndays = 365.0;

    if( date->years > 0.0 )
    {
        return date->years;
    }

    /* Make sure the date is in range ... could put in an assertion, but
       for present just ignore errors */

    if( mon < 1 || mon > 12 ) mon = 1;

    /* Calculate the number of days if this isn't a leap year */

    days = monthday[mon-1] + date->dtDay - 1
           + (date->dtHour+date->dtMin/60.0+date->dtSec/3600.0)/24.0;

    /* Account for leap years - day number is incremented if the month is
       after February, and number of days in the year in incremented */

    if( utlIsLeapYear( date->dtYear ) )
    {
        if( mon > 2 ) days++;
        ndays++;
    }

    /* Calculate the year number by dividing the day number by the number of
       days */

    date->years = date->dtYear+(days/ndays);
    return date->years;
}

