#include "snapconfig.h"

/*
   $Log: get_date.c,v $
   Revision 1.2  2004/04/22 02:35:25  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:48:51  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "util/get_date.h"

static time_t now;
static struct tm *lt;
static const char *mon[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG",
                      "SEP", "OCT", "NOV", "DEC"
                     };
static char runtime[GETDATELEN];

char *get_date( char * datestr )
{
    if( !datestr ) datestr = runtime;

    // Test tooling needs deterministic .bin output to diff byte-for-byte
    // against a committed golden copy. The real current time can't do
    // that, so this override writes a fixed value into the full
    // GETDATELEN buffer instead - covering every byte, not just the
    // string prefix, so no output byte is left non-deterministic.
    const char *fixed_date = getenv( "SNAP_TEST_FIXED_DATE" );
    if( fixed_date ) {
        memset( datestr, 0, GETDATELEN );
        snprintf( datestr, GETDATELEN, "%s", fixed_date );
        return datestr;
    }

    time( &now );
    lt = localtime( &now );
    sprintf( datestr, "%2d-%3s-%4d %02d:%02d:%02d", lt->tm_mday,
             mon[lt->tm_mon],1900+lt->tm_year,lt->tm_hour,lt->tm_min,lt->tm_sec);
    return datestr;
}
