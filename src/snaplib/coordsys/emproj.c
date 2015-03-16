#include "snapconfig.h"
/* Code for the Equatorial Mercator projection */

/*
   $Log: emproj.c,v $
   Revision 1.1  1995/12/22 16:50:38  CHRIS
   Initial revision

*/

#include <math.h>

#include "coordsys/emproj.h"
#include "coordsys/isometrc.h"
#include "util/pi.h"

void define_EMProjection( EMProjection *em, double a, double rf, double cm, double rlat )
{
    double e2;
    double slt;
    em->a = a;
    em->rf = rf;
    em->cm = cm;
    em->rlat = rlat;

    if( rf > 0 )
    {
        double f;
        f = 1.0/rf;
        e2 = 2.0*f - f*f;
    }
    else
    {
        e2 = 0.0;
    }
    em->e = sqrt(e2);
    slt = sin(rlat);
    em->c = a * cos(rlat) / sqrt(1.0-e2*slt*slt);
}

/* ================================================================== */

void geod_em( EMProjection *em, double lon, double lat, double *ce, double *cn )
{

    double q;
    double dlam;

    q = isometric_from_geodetic( lat, em->e );
    dlam = lon - em->cm;

    while( dlam > PI ) dlam -= TWOPI;
    while( dlam < -PI ) dlam += TWOPI;

    *cn = em->c * q;
    *ce=em->c * dlam;
}


/* ================================================================== */

void em_geod( EMProjection *em, double ce, double cn, double *lon, double *lat )
{

    double dlam;
    double q;

    q=cn/em->c;
    dlam=ce/em->c + em->cm;

    *lat = geodetic_from_isometric( q, em->e );

    while( dlam > PI ) dlam -= TWOPI;
    while( dlam < -PI ) dlam += TWOPI;

    *lon = dlam;

}

