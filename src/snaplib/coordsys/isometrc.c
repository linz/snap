#include "snapconfig.h"
/* Conversion geodetic <=> isometric latitude */

/*
   $Log: isometrc.c,v $
   Revision 1.1  1995/12/22 16:54:17  CHRIS
   Initial revision

*/

#include <math.h>
#include "util/pi.h"
#include "coordsys/isometrc.h"

/* =================================================================== */
/* Converts geodetic latitude to isometric                             */


double isometric_from_geodetic( double lat, double e )
{
    double xx;
    double yy;
    double slt;

    xx=tan(PI/4.0+lat/2.0);
    slt = sin(lat);
    yy=pow((1.0-e*slt)/(1.0+e*slt),e/2.0);
    return log(xx*yy);
}


/* =================================================================== */
/* Converts isometric latitude to geodetic by Newton Raphson iteration */

double geodetic_from_isometric( double q, double e )
{

    static double tol=4.848132e-11;
    double lat;
    double zz, diff;
    double ww, top, bot, phi1;
    double slt2;
    int it = 0;

    zz=exp(q);
    lat=2.0*(atan(zz)-PI/4.0);
    do
    {
        slt2 = e*sin(lat);
        ww=pow(((1.0-slt2)/(1.0+slt2)),(e/2.0));
        slt2 *= slt2;
        slt2 = 1.0-slt2;
        top=log(tan(PI/4.0+(lat)/2.0)*ww)-q;
        bot=(1.0-e*e)/(slt2*cos(lat));
        phi1=lat-top/bot;
        diff=fabs(phi1-(lat));
        lat = phi1;
    }
    while (diff>=tol && it++ < 10);
    return lat;
}

#ifdef TEST_ISOMETRIC

#include <stdio.h>
#include "util/pi.h"

int main()
{
    double rf, e, lat;
    rf = 297.0;
    e  = 1.0/rf;
    e  = 2.0*e - e*e;

    for(;;)
    {
        printf("\nEnter geodetic latitude: ");
        if( scanf("%lf",&lat) != 1 ) break;
        printf("\nGeodetic:  %11.6lf\n",lat);
        lat = RTOD * isometric_from_geodetic( lat * DTOR, e );
        printf("\nIsometric: %11.6lf\n",lat);
        lat = RTOD * geodetic_from_isometric( lat * DTOR, e );
        printf("\nGeodetic:  %11.6lf\n\n",lat);
    }
}
#endif


