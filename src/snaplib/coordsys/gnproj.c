#include "snapconfig.h"
/* Code for the gnomic projection */
/* NOTE: This projection applies on a sphere, not an ellipsoid */

/*
   $Log: gnproj.c,v $
   Revision 1.1  1995/12/22 16:52:54  CHRIS
   Initial revision

*/

#include <math.h>

#include "coordsys/gnproj.h"
#include "coordsys/isometrc.h"
#include "util/pi.h"


static char rcsid[]="$Id: gnproj.c,v 1.1 1995/12/22 16:52:54 CHRIS Exp $";

#pragma warning (disable : 4100)

void define_gnomic_projection( GnomicProjection *gp, double a, double rf,
                               double orglat, double orglon, double fe, double fn )
{
    gp->orglat = orglat;
    gp->orglon = orglon;
    gp->fe = fe;
    gp->fn = fn;
    gp->a = a;

    gp->csolt = cos( orglat );
    gp->snolt = sin( orglat );
}

void gnomic_geod( GnomicProjection *gp, double ce, double cn, double *lon, double *lat)
{

    double psi, u, a1, a2, a3, a4, root, dlam, csth, snth;

    ce -= gp->fe;
    cn -= gp->fn;

    if( ce == 0.0 && cn == 0.0 )
    {
        *lat = gp->orglat;
        *lon = gp->orglon;
        return;
    }

    csth=_hypot(cn,ce);
    snth=cn/csth;
    csth=ce/csth;

    if( ce == 0.0 )
    {
        psi=atan(cn/(gp->a*snth));
    }
    else
    {
        psi=atan(ce/(gp->a*csth));
    }

    u=sin(psi);
    a4=cos(psi);

    a1 = a4*a4;

    a2=gp->csolt*gp->csolt * (csth*csth * u*u  -  1.0);

    a3 = gp->snolt*gp->snolt*a1;

    a4 *= gp->snolt;

    u *= csth;

    root=sqrt(a3-a2-a1);

    if( cn > 0.0 )
    {
        *lat = asin( a4 + root );
    }
    else
    {
        *lat = asin( a4 - root );
    }

    dlam = asin(u/cos(*lat));
    dlam += gp->orglon;
    while( dlam > PI ) dlam -= TWOPI;
    while( dlam < -PI ) dlam += TWOPI;

    *lon = dlam;
}


void geod_gnomic( GnomicProjection *gp, double lon, double lat, double *ce, double *cn )
{

    double snp, snpo, csp, cspo, snl, csl, bot;

    snp = sin(lat);
    snpo= gp->snolt;
    csp = cos(lat);
    cspo= gp->csolt;
    lon -= gp->orglon;
    snl = sin(lon);
    csl = cos(lon);
    bot = snpo*snp + cspo*csp*csl;

    *ce = gp->a * csp * snl / bot + gp->fe;
    *cn = gp->a *(cspo*snp - snpo*csp*csl)/bot + gp->fn;
}

