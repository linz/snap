#include "snapconfig.h"

/*
   $Log: get_date.c,v $
   Revision 1.2  2004/04/22 02:35:25  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:48:51  CHRIS
   Initial revision

*/

#include <stdio.h>
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
    time( &now );
    lt = localtime( &now );
    sprintf( datestr, "%2d-%3s-%4d %02d:%02d:%02d", lt->tm_mday,
             mon[lt->tm_mon],1900+lt->tm_year,lt->tm_hour,lt->tm_min,lt->tm_sec);
    return datestr;
}
