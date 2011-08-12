#ifndef GNPROJ_H
#define GNPROJ_H

/*
   $Log: gnproj.h,v $
   Revision 1.1  1995/12/22 16:53:09  CHRIS
   Initial revision

*/

#ifndef GNPROJ_H_RCSID
#define GNPROJ_H_RCSID "$Id: gnproj.h,v 1.1 1995/12/22 16:53:09 CHRIS Exp $"
#endif

typedef struct
{
    double orglat, orglon;  /* Origin of coords */
    double fe, fn;          /* False easting and northing */
    double a;               /* Radius of sphere */
    double csolt, snolt;
} GnomicProjection;

void define_gnomic_projection( GnomicProjection *gp, double a, double rf,
                               double orglat, double orglon, double fe, double fn );
void gnomic_geod( GnomicProjection *gp, double ce, double cn, double *lat, double *lon);
void geod_gnomic( GnomicProjection *gp, double lat, double lon, double *ce, double *cn );

#endif
