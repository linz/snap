#ifndef _LAMBERT_H
#define _LAMBERT_H

/*
   $Log: lambert.h,v $
   Revision 1.1  1995/12/22 16:55:39  CHRIS
   Initial revision

*/

#ifndef LAMBERT_H_RCSID
#define LAMBERT_H_RCSID "$Id: lambert.h,v 1.1 1995/12/22 16:55:39 CHRIS Exp $"
#endif

/* Header file for Lambert Conformal Conic projection code */

typedef struct
{
    double sp1, sp2;    /* Standard parallels of conic projection (rad) */
    double lt0, ln0;    /* Origin of map lat/long (rad) */
    double e0, n0;      /* False easting/northing (metres) */
    double a, rf, e;        /* Ellipsoid semi-major axis, flattening */
    double n, F;        /* Intermediate values used to facilitate calcs */
    double r0;
    int rev;            /* 1 or -1 .. inverts earth if mean of std parallels is south of equator */
} LCCProjection;


void defineLCCProjection( LCCProjection *lp,
                          double a, double rf,
                          double sp1, double sp2,
                          double lt0, double ln0,
                          double e0, double n0 );

void convertGeogToLCC( LCCProjection *lp,
                       double lt, double ln,
                       double *ce, double *cn,
                       double *sf, double *cnv );

void convertLCCToGeog( LCCProjection *lp,
                       double ce, double cn, double *lt, double *ln );


#endif

