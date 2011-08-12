#include "snapconfig.h"
/*
   $Log: dms.c,v $
   Revision 1.2  2004/04/22 02:35:24  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:56:57  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "util/dms.h"
#include "util/pi.h"
#include "util/errdef.h"
#include "util/chkalloc.h"

static char rcsid[]="$Id: dms.c,v 1.2 2004/04/22 02:35:24 ccrook Exp $";

/*------------------------------------------------------------------*/
/*  Angle format conversion routines - DMS to radians and           */
/*  vice-versa.  Uses the structure DMS defined in geodetic.h       */
/*  as                                                              */
/*                                                                  */
/*  typedef struct {                                                */
/*              int    deg                                          */
/*              int    min                                          */
/*              double sec                                          */
/*              char   neg   (TRUE = negative angle )               */
/*              } DMS;                                              */
/*                                                                  */
/*  dms_deg     Converts degrees, minutes, seconds to radians       */
/*  deg_dms     Converts radians to degrees, minutes, seconds       */
/*------------------------------------------------------------------*/


double dms_deg( DMS *dms )
{
    double d;
    d = dms->deg + dms->min/60.0 + dms->sec/3600.0;
    if( dms->neg ) d = -d;
    return d;
}

DMS *deg_dms( double d, DMS *dms )
{
    dms->neg = d<0.0;
    if (dms->neg) d = -d;
    d -= (dms->deg = floor(d));
    d *= 60.0;
    d -= (dms->min = floor(d));
    dms->sec = d*60.0;
    return dms;
}

/*--------------------------------------------------------

Routine to convert decimal degrees to a string containing
degrees, minutes, seconds.  The parameters are:

   d        double    The angle in degrees
   format   void *    Created using create_dms_format
   str      char *    The string to which the angle is written.  If
w-		      NULL, then it is written to a static string.


Return value:
   char *    The string to which the angle is written

Should be first initiallized with a call to init_dms_string with parameters

   ndeg     int       The number of digits to show for degrees
   ndp      int       The number of decimal places in the seconds
   prfx     int       If True then the sign prefixes the string
   deg      char *    text between degrees and minutes
   min      char *    text between minutes and seconds
   sec      char *    text after seconds
   plus     char *    Character string used if the angle is positive
   minus    char *    Character string used if the angle is negative

---------------------------------------------------------*/

typedef struct
{
    int ndeg;
    int ndp;
    double offset;
    int prfx;
    int radians;
    int type;
    char *deg;
    char *min;
    char *sec;
    char *plus;
    char *minus;
} DMS_format;


void *create_dms_format( int ndeg, int ndp, int fmt,
                         char *deg, char *min, char *sec, char *plus, char *minus )
{

    void *data;
    DMS_format *dmsf;

    int i, size, ndpmax;
    char *blank, *empty, *next;
    double offset;

    if( ndeg < 0 ) ndeg = 0;
    if( ndeg > 4 ) ndeg = 4;

    size = sizeof( DMS_format ) + 2;
    if( deg ) size += strlen(deg) + 1;
    if( min ) size += strlen(min) + 1;
    if( sec ) size += strlen(sec) + 1;
    if( plus ) size += strlen(plus) + 1;
    if( minus ) size += strlen(minus) + 1;

    data = check_malloc( size );

    dmsf = (DMS_format *) data;
    blank = (char *) data + sizeof(DMS_format);
    empty = blank + 1;
    *blank = ' ';
    *empty = 0;
    next = blank + 2;

    dmsf->ndeg = ndeg;
    dmsf->prfx = (fmt & DMSF_FMT_PREFIX_HEM);
    dmsf->radians = (fmt & DMSF_FMT_INPUT_RADIANS);
    dmsf->type = 0;
    if( fmt & DMSF_FMT_DM ) dmsf->type = 1;
    if( fmt & DMSF_FMT_DEG ) dmsf->type = 2;

    if( ndp < 0 ) ndp = 0;
    ndpmax = 6 + dmsf->type*2;
    if( ndp > ndpmax ) ndp = ndpmax;
    dmsf->ndp = ndp;

    offset = 0.5/3600.0;
    for( i=ndp; i--;) offset /= 10;
    if( dmsf->type == 1 ) offset *= 60.0;
    if( dmsf->type == 2 ) offset = 0;
    dmsf->offset = offset;

    if( deg )
    {
        dmsf->deg = next;
        strcpy( next, deg );
        next += strlen(deg) + 1;
    }
    else
    {
        dmsf->deg = blank;
    }

    if( min )
    {
        dmsf->min = next;
        strcpy( next, min );
        next += strlen(min) + 1;
    }
    else
    {
        dmsf->min = dmsf->type == 0 ? blank : empty;
    }

    if( sec )
    {
        dmsf->sec = next;
        strcpy( next, sec);
        next += strlen(sec) + 1;
    }
    else
    {
        dmsf->sec = empty;
    }

    if( plus)
    {
        dmsf->plus = next;
        strcpy( next, plus);
        next += strlen(plus) + 1;
    }
    else
    {
        dmsf->plus = empty;
    }

    if( minus )
    {
        dmsf->minus= next;
        strcpy( next, minus);
        next += strlen(minus) + 1;
    }
    else
    {
        dmsf->minus= blank;
    }

    return data;
}


void delete_dms_format( void *dmsf )
{
    check_free( dmsf );
}

#define FMT ((DMS_format *)dmsf)

char *dms_string( double d, void *dmsf, char *str )
{
    static char dmsstr[80];
    char *sign;
    int deg, min;
    double sec;
    double offset;
    int seclen;
    char *tgt;

    tgt = str ? str : dmsstr;

    if( FMT->radians ) d *= RTOD;
    sign = FMT->plus;
    if(d < 0 ) { d = -d; sign = FMT->minus; }

    if( FMT->type == 2 )
    {
        sprintf(tgt,"%s%.*lf%s",(FMT->prfx ? sign : ""),
                (int) (FMT->ndp), d, (FMT->prfx ? "" : sign) );

        return tgt;
    }

    d += FMT->offset;
    offset = FMT->offset*60.0;

    deg = floor(d);
    d -= deg;
    min = 0;
    if( FMT->type == 0 )
    {
        d *= 60.0;
        min = floor(d);
        d -= min;
        offset *= 60.0;
    }
    sec = d*60.0 - offset;
    if( sec < 0 ) sec = 0.0;

    seclen = FMT->ndp ? FMT->ndp+3 : FMT->ndp+2;

    if( FMT->type == 1 )
    {
        sprintf(tgt,"%s%*d%s%0*.*lf%s%s",(FMT->prfx ? sign : ""),
                (int) (FMT->ndeg), (int)deg, FMT->deg,
                seclen,(int)(FMT->ndp), sec, FMT->min, (FMT->prfx ? "" : sign) );
    }
    else
    {
        sprintf(tgt,"%s%*d%s%02d%s%0*.*lf%s%s",(FMT->prfx ? sign : ""),
                (int) (FMT->ndeg), (int)deg, FMT->deg, (int)min, FMT->min,
                seclen,(int)(FMT->ndp), sec, FMT->sec, (FMT->prfx ? "" : sign) );
    }

    return tgt;
}



