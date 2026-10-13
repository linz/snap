#include "snapconfig.h"
/* Conversion geodetic <=> isometric latitude */

/*
   $Log: isometrc.c,v $
   Revision 1.1  1995/12/22 16:54:17  CHRIS
   Initial revision

*/

#include <cmath>
#include "util/pi.h"
#include "coordsys/isometrc.h"

/* =================================================================== */
/* Converts geodetic latitude to isometric                             */

double isometric_from_geodetic(double lat, double e) {
    double xx;
    double yy;
    double slt;

    xx = std::tan(PI / 4.0 + lat / 2.0);
    slt = std::sin(lat);
    yy = std::pow((1.0 - e * slt) / (1.0 + e * slt), e / 2.0);
    return std::log(xx * yy);
}

/* =================================================================== */
/* Converts isometric latitude to geodetic by Newton Raphson iteration */

double geodetic_from_isometric(double q, double e) {
    static const double tol = 4.848132e-11;
    double lat;
    double zz;
    double diff;
    double ww;
    double top;
    double bot;
    double phi1;
    double slt2;
    int it = 0;

    zz = std::exp(q);
    lat = 2.0 * (std::atan(zz) - PI / 4.0);
    do {
        slt2 = e * std::sin(lat);
        ww = std::pow(((1.0 - slt2) / (1.0 + slt2)), (e / 2.0));
        slt2 *= slt2;
        slt2 = 1.0 - slt2;
        top = std::log(std::tan(PI / 4.0 + (lat) / 2.0) * ww) - q;
        bot = (1.0 - e * e) / (slt2 * std::cos(lat));
        phi1 = lat - top / bot;
        diff = std::fabs(phi1 - (lat));
        lat = phi1;
    } while (diff >= tol && it++ < 10);
    return lat;
}

#ifdef TEST_ISOMETRIC

#include <cstdio>
#include "util/pi.h"

int main() {
    double rf, e, lat;
    rf = 297.0;
    e = 1.0 / rf;
    e = 2.0 * e - e * e;

    for (;;) {
        std::printf("\nEnter geodetic latitude: ");
        if (std::scanf("%lf", &lat) != 1)
            break;
        std::printf("\nGeodetic:  %11.6lf\n", lat);
        lat = RTOD * isometric_from_geodetic(lat * DTOR, e);
        std::printf("\nIsometric: %11.6lf\n", lat);
        lat = RTOD * geodetic_from_isometric(lat * DTOR, e);
        std::printf("\nGeodetic:  %11.6lf\n\n", lat);
    }
}
#endif
