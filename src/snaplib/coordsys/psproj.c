#include "snapconfig.h"
/* Code for the Polar Stereographic projection */

/*
   $Log: psproj.c,v $
   Revision 1.1  1995/12/22 16:59:38  CHRIS
   Initial revision

*/

#include <math.h>

#include "coordsys/psproj.h"
#include "coordsys/isometrc.h"
#include "util/pi.h"


static char rcsid[]="$Id: psproj.c,v 1.1 1995/12/22 16:59:38 CHRIS Exp $";

void define_PSProjection( PSProjection *psp, double a, double rf,
                          double cm, double sf, double fe, double fn, char south )
{
    psp->a = a;
    psp->rf = rf;
    psp->cm = cm;
    psp->sf = sf;
    psp->fe = fe;
    psp->fn = fn;
    psp->south = south;
    if( rf != 0.0 )
    {
        double f = 1.0/rf;
        psp->e2 = 2.0*f - f*f;
        psp->e = sqrt( psp->e2 );
    }
    else
    {
        psp->e = psp->e2 = 0.0;
    }
    psp->k = 2.0*psp->sf * psp->a / sqrt(1.0-psp->e2) *
             pow((1.0-psp->e)/(1.0+psp->e),psp->e/2.0);
}

void psp_geod( PSProjection *psp, double ce, double cn, double *slam, double *sphi )
{

    double deast, dnorth;
    double dlam, r, q;

    deast=ce-psp->fe;
    dnorth=cn-psp->fn;

    if( !psp->south ) dnorth = -dnorth;
    dlam=atan2(deast,dnorth);
    r=_hypot(deast,dnorth);
    q=log(psp->k/r);

    *sphi = geodetic_from_isometric(q,psp->e);

    *slam=psp->cm+dlam;
    while (*slam > PI) *slam -= TWOPI;
    while (*slam < -PI ) *slam += TWOPI;
    if( psp->south ) *sphi = -*sphi;
}


void geod_psp(PSProjection *psp, double slam, double sphi, double *cee, double *cnn)
{

    double diff, power, a1, a2, a3, r;

    /* Is this point very close to the pole - within 0.00015" */

    diff = PI/2.0 - fabs(sphi);

    if( diff < 0.00015*STOR )
    {
        *cee = psp->fe;
        *cnn = psp->fn;
    }
    else
    {
        power=psp->e/2.0;
        a1=psp->e*sin(sphi);
        if (psp->south)
        {
            a2=tan(PI/4.0-sphi/2.0);
            a3=pow((1.0+a1)/(1.0-a1),power);
        }
        else
        {
            a2=tan(PI/4.0+sphi/2.0);
            a3=pow((1.0-a1)/(1.0+a1),power);
        }

        r=psp->k*(1.0/(a2*a3));

        slam -= psp->cm;
        *cee=r*sin(slam)+psp->fe;
        *cnn=r*cos(slam);

        if (!psp->south ) *cnn = -*cnn;
    }

    *cnn += psp->fn;
}

