#include "snapconfig.h"
/*
   $Log: crdsyse2.c,v $
   Revision 1.1  1995/12/22 16:33:40  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>
#include "coordsys/coordsys.h"


static char rcsid[]="$Id: crdsyse2.c,v 1.1 1995/12/22 16:33:40 CHRIS Exp $";

/*------------------------------------------------------------------*/
/*  Conversions between geodetic coordinates (ellipsoidal heights)  */
/*  and geocentric coordinates.                                     */
/*                                                                  */
/*  llh[3]       contains latitude, longitude (radians), height (m) */
/*  xyz[3]       contains the geocentric x, y, z coordinates (m)    */
/*                                                                  */
/*  The routines are written such that the new values and overwrite */
/*  the old.                                                        */
/*  Formulae from any standard geodetic reference.                  */
/*------------------------------------------------------------------*/


double * llh_to_xyz( ellipsoid *el, double llh[3], double xyz[3],
                     double *dEdLn, double *dNdLt )
{
    double bsac,p,clt,slt,cln,sln,hgt;
    clt  = cos(llh[CRD_LAT]);
    slt  = sin(llh[CRD_LAT]);
    cln  = cos(llh[CRD_LON]);
    sln  = sin(llh[CRD_LON]);
    hgt = llh[CRD_HGT];
    bsac = _hypot( el->b*slt, el->a*clt );
    p    = el->a2*clt/bsac + hgt*clt;
    xyz[0] = p*cln;
    xyz[1] = p*sln;
    xyz[2] = el->b2*slt/bsac + llh[CRD_HGT]*slt;

    if( dEdLn ) *dEdLn = p;
    if( dNdLt ) *dNdLt = (el->a2 * el->b2)/(bsac*bsac*bsac) + hgt;

    return &xyz[0];
}


double * xyz_to_llh( ellipsoid *el, double xyz[3], double llh[3] )
{
    double bsac,p,slt,clt,lt0,ln,lt,hgt;
    int it;
    ln = atan2( xyz[1], xyz[0] );
    p      = _hypot( xyz[0], xyz[1] );
    lt = atan2( el->a2*xyz[2], el->b2*p );
    it = 0;
    do
    {
        lt0    = lt;
        slt    = sin(lt);
        clt    = cos(lt);
        bsac   = _hypot( el->b*slt, el->a*clt );
        lt     = atan2( xyz[2]+slt*el->a2b2/bsac, p );
        if ( fabs(lt-lt0) < 1.0e-10 ) break;
    }
    while( it++ < 10);

    hgt = p*clt+xyz[2]*slt - bsac;

    llh[CRD_LAT] = lt;
    llh[CRD_LON] = ln;
    llh[CRD_HGT] = hgt;
    return &llh[0];
}
