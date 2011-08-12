#ifndef _TMPROJ_H
#define _TMPROJ_H

/*
   $Log: tmproj.h,v $
   Revision 1.1  1995/12/22 17:01:41  CHRIS
   Initial revision

*/

#ifndef TMPROJ_H_RCSID
#define TMPROJ_H_RCSID "$Id: tmproj.h,v 1.1 1995/12/22 17:01:41 CHRIS Exp $"
#endif

typedef struct
{
    double meridian;          /* Central meridian */
    double scalef;            /* Scale factor */
    double orglat;            /* Origin latitude */
    double falsee;            /* False easting */
    double falsen;            /* False northing */
    double utom;              /* Unit to metre conversion */

    double a, rf, f, e2, ep2;     /* Ellipsoid parameters */
    double om;                /* Intermediate calculation */
} tmprojection;


/* Functions defined in TMPROJ.C */

void define_tmprojection( tmprojection *tm, double a, double rf,
                          double cm, double sf, double lto,
                          double fe, double fn, double utom );

void tm_geod( tmprojection *tm, double ce, double cn, double *lt, double *ln );
void geod_tm( tmprojection *tm, double lt, double ln, double *ce, double *cn );

#endif



