#ifndef _DMS_H
#define _DMS_H

/*
   $Log: dms.h,v $
   Revision 1.2  2004/04/22 02:35:24  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:57:21  CHRIS
   Initial revision

*/

/* dms.h - header file for dms.c, degrees, minutes, seconds conversion
   routines */

typedef struct                       /*  DMS angle */
{
    int    deg;
    int    min;
    double sec;
    char   neg;          /*  TRUE = negative, FALSE = positive */
} DMS;

double dms_deg( DMS *dms );
DMS *deg_dms( double d, DMS *dms );

#define DMSF_FMT_PREFIX_HEM 1
#define DMSF_FMT_SUFFIX_HEM 0
#define DMSF_FMT_DMS 0
#define DMSF_FMT_DM  2
#define DMSF_FMT_DEG 4
#define DMSF_FMT_INPUT_RADIANS 8

void *create_dms_format( int ndeg, int ndp, int fmt,
                         const char *deg, const char *min, const char *sec, 
                         const char *plus, const char *minus );
void delete_dms_format( void *dmsf );

/* Note: if str is null this returns a static string */

const char *dms_string( double d, void *dmsf, char *str );

#endif
