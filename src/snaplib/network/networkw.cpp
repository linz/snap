#include "snapconfig.h"
#include "coordsys/coordsys.h"
/*
   $Log: networkw.c,v $
   Revision 1.1  1995/12/22 17:38:10  CHRIS
   Initial revision

*/

#include <boost/algorithm/string/predicate.hpp>
#include <cstdio>
#include <string>
#include <optional>
#include <string_view>
#include "network/network.h"
#include "util/dms.h"
#include "util/pi.h"
#include "util/errdef.h"

/*=============================================================*/
/* Basic routine to write a station data file                  */

int write_network(network* nw, const std::string& fname, const std::string_view comment, int coord_precision,
                  int (*select)(station* st)) {
    std::FILE* stf;
    station* st;
    double northing;
    double easting;
    char projection_coords;
    char geocentric_coords;
    int cp;
    int nclass;
    int ellipsoidal_heights;
    char degrees = 0;
    const char explicit_geoid = nw->options & NW_EXPLICIT_GEOID;

    if (!nw || !nw->stnlist || !nw->crdsys) {
        return MISSING_DATA;
    }

    stf = std::fopen(fname.c_str(), "w");
    if (stf == nullptr) {
        handle_error(FILE_OPEN_ERROR, "Unable to create new coordinate file", fname);
        return FILE_OPEN_ERROR;
    }

    /* Print the header information */

    projection_coords = is_projection(nw->crdsys);
    geocentric_coords = is_geocentric(nw->crdsys);
    ellipsoidal_heights = nw->options & NW_ELLIPSOIDAL_HEIGHTS ? 1 : 0;

    std::fprintf(stf, "%s\n", nw->name ? nw->name->c_str() : "Unnamed network");
    std::fprintf(stf, "%s\n", nw->crdsysdef.c_str());
    std::fputs("options", stf);

    if (!geocentric_coords) {
        if (nw->options & NW_ELLIPSOIDAL_HEIGHTS) {
            std::fputs(" ellipsoidal_heights", stf);
        } else {
            std::fputs(" orthometric_heights", stf);
        }
    }

    if (explicit_geoid && nw->options & NW_DEFLECTIONS) {
        std::fputs(" deflections", stf);
    } else {
        std::fputs(" no_deflections", stf);
    }

    if (explicit_geoid && nw->options & NW_GEOID_HEIGHTS) {
        std::fputs(" geoid_heights", stf);
    } else {
        std::fputs(" no_geoid_heights", stf);
    }

    if (nw->options & NW_DEC_DEGREES) {
        std::fputs(" degrees", stf);
        degrees = 1;
    }

    nclass = nw->classification_count();
    if (nclass) {
        int i;
        for (i = 0; i++ < nclass;) {
            const std::string name = nw->class_name(i);
            if (nclass == 1 && boost::algorithm::iequals(name, STATION_ORDER_CLASS_NAME)) {
                std::fputs(" station_orders", stf);
            } else {
                std::fputs(" c=", stf);
                std::fputs(name.c_str(), stf);
            }
        }
    }

    std::fputs("\n", stf);

    /* Print details of the the program creating the file */

    if (!comment.empty()) {
        std::fprintf(stf, "! %s\n", std::string(comment).c_str());
    }

    std::fprintf(stf, "\n");

    /* Write a station header comment */

    std::fprintf(stf, "!Code");
    if (geocentric_coords) {
        std::fprintf(stf, " %12s %12s %12s", "X   ", "Y    ", "Z    ");
    } else {
        if (projection_coords) {
            std::fprintf(stf, " %12s %12s", "Easting", "Northing");
        } else {
            std::fprintf(stf, " %18s %18s", "Latitude    ", "Longitude    ");
        }
        std::fprintf(stf, " %10s", ellipsoidal_heights ? "Ell.Hgt" : "Orth.Hgt");
    }
    if (explicit_geoid) {
        if (nw->options & NW_DEFLECTIONS) {
            std::fprintf(stf, " %5s %5s", "Xi", "Eta");
        }
        if (nw->options & NW_GEOID_HEIGHTS) {
            std::fprintf(stf, " %7s", "G.Hgt");
        }
    }
    if (nclass) {
        int i;
        for (i = 0; i++ < nclass;) {
            const std::string name = nw->class_name(i);
            std::fprintf(stf, " %-5s", name.c_str());
        }
    }
    std::fprintf(stf, " Name\n");

    /* Now write the details of the stations */

    reset_station_list(nw, 0);

    const DmsFormat latitudeFormat(3, 6, 0, std::nullopt, std::nullopt, std::nullopt, " N", " S");
    const DmsFormat longitudeFormat(3, 6, 0, std::nullopt, std::nullopt, std::nullopt, " E", " W");

    cp = coord_precision;
    if (cp <= 0 || cp > 10) {
        cp = 4;
    }

    while (nullptr != (st = next_station(nw))) {
        if (select && !(*select)(st)) {
            continue;
        }
        std::fprintf(stf, "%-5s", st->Code.c_str());

        if (projection_coords) {
            geog_to_proj(nw->crdsys->prj, st->ELon, st->ELat, &easting, &northing);
            std::fprintf(stf, " %12.*lf %12.*lf", cp, easting, cp, northing);
            std::fprintf(stf, " %10.*lf", cp, st->OHgt + ellipsoidal_heights * st->GUnd);
        } else if (geocentric_coords) {
            double llh[3];
            double xyz[3];
            llh[CRD_LAT] = st->ELat;
            llh[CRD_LON] = st->ELon;
            llh[CRD_HGT] = st->OHgt + st->GUnd;
            llh_to_xyz(nw->crdsys->rf->el, llh, xyz, nullptr, nullptr);
            std::fprintf(stf, " %12.*lf %12.*lf %12.*lf", cp, xyz[0], cp, xyz[1], cp, xyz[2]);
        } else {
            if (degrees) {
                std::fprintf(stf, " %18.*lf %18.*lf", cp + 7, st->ELat / DTOR, cp + 7, st->ELon / DTOR);
            } else {
                std::fprintf(stf, " %s", dms_string(st->ELat / DTOR, latitudeFormat).c_str());
                std::fprintf(stf, " %s", dms_string(st->ELon / DTOR, longitudeFormat).c_str());
            }
            std::fprintf(stf, " %10.*lf", cp, st->OHgt + ellipsoidal_heights * st->GUnd);
        }

        if (explicit_geoid) {
            if (nw->options & NW_DEFLECTIONS) {
                std::fprintf(stf, " %5.1lf %5.1lf", st->GXi * 3600.0 / DTOR, st->GEta * 3600.0 / DTOR);
            }
            if (nw->options & NW_GEOID_HEIGHTS) {
                std::fprintf(stf, " %7.*lf", cp, st->GUnd);
            }
        }

        if (nclass) {
            int i;
            for (i = 0; i++ < nclass;) {
                const int clsid = st->get_class(i);
                const std::string cval = nw->class_value(i, clsid);
                std::fprintf(stf, " %-5s", cval.empty() ? "-" : cval.c_str());
            }
        }

        std::fprintf(stf, " %s\n", st->Name.c_str());
    }

    std::fclose(stf);
    return OK;
}
