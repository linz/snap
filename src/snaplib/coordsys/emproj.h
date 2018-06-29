#ifndef EMPROJ_H
#define EMPROJ_H

/*
   $Log: emproj.h,v $
   Revision 1.1  1995/12/22 16:51:22  CHRIS
   Initial revision

*/

typedef struct
{
    double cm;
    double rlat;
    double a;
    double rf;
    double e;
    double c;
} EMProjection;

void define_EMProjection( EMProjection *em, double a, double rf, double cm, double rlat );
void geod_em( EMProjection *em, double lat, double lon, double *ce, double *cn );
void em_geod( EMProjection *em, double ce, double cn, double *lat, double *lon );


#endif
