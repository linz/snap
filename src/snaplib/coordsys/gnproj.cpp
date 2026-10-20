#include "snapconfig.h"
/* Code for the gnomic projection */
/* NOTE: This projection applies on a sphere, not an ellipsoid */

/*
   $Log: gnproj.c,v $
   Revision 1.1  1995/12/22 16:52:54  CHRIS
   Initial revision

*/

#include <cmath>
#include "coordsys/gnproj.h"
#include "util/pi.h"

// #pragma warning (disable : 4100)

void define_gnomic_projection(GnomicProjection* gp, double a, double, double orglat, double orglon, double fe,
                              double fn) {
    gp->orglat = orglat;
    gp->orglon = orglon;
    gp->fe = fe;
    gp->fn = fn;
    gp->a = a;

    gp->csolt = std::cos(orglat);
    gp->snolt = std::sin(orglat);
}

void gnomic_geod(GnomicProjection* gp, double ce, double cn, double* lon, double* lat) {
    double psi;
    double u;
    double a1;
    double a2;
    double a3;
    double a4;
    double root;
    double dlam;
    double csth;
    double snth;

    ce -= gp->fe;
    cn -= gp->fn;

    if (ce == 0.0 && cn == 0.0) {
        *lat = gp->orglat;
        *lon = gp->orglon;
        return;
    }

    csth = std::hypot(cn, ce);
    snth = cn / csth;
    csth = ce / csth;

    if (ce == 0.0) {
        psi = std::atan(cn / (gp->a * snth));
    } else {
        psi = std::atan(ce / (gp->a * csth));
    }

    u = std::sin(psi);
    a4 = std::cos(psi);

    a1 = a4 * a4;

    a2 = gp->csolt * gp->csolt * (csth * csth * u * u - 1.0);

    a3 = gp->snolt * gp->snolt * a1;

    a4 *= gp->snolt;

    u *= csth;

    root = std::sqrt(a3 - a2 - a1);

    if (cn > 0.0) {
        *lat = std::asin(a4 + root);
    } else {
        *lat = std::asin(a4 - root);
    }

    dlam = std::asin(u / std::cos(*lat));
    dlam += gp->orglon;
    while (dlam > PI) {
        dlam -= TWOPI;
    }
    while (dlam < -PI) {
        dlam += TWOPI;
    }

    *lon = dlam;
}

void geod_gnomic(GnomicProjection* gp, double lon, double lat, double* ce, double* cn) {
    double snp;
    double snpo;
    double csp;
    double cspo;
    double snl;
    double csl;
    double bot;

    snp = std::sin(lat);
    snpo = gp->snolt;
    csp = std::cos(lat);
    cspo = gp->csolt;
    lon -= gp->orglon;
    snl = std::sin(lon);
    csl = std::cos(lon);
    bot = snpo * snp + cspo * csp * csl;

    *ce = gp->a * csp * snl / bot + gp->fe;
    *cn = gp->a * (cspo * snp - snpo * csp * csl) / bot + gp->fn;
}
