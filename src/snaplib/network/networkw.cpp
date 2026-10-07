#include "snapconfig.h"
/*
   $Log: networkw.c,v $
   Revision 1.1  1995/12/22 17:38:10  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include <boost/algorithm/string/predicate.hpp>

#include "network/network.h"
#include "util/datafile.h"
#include "util/dms.h"
#include "util/pi.h"
#include "util/errdef.h"


/*=============================================================*/
/* Basic routine to write a station data file                  */


int write_network( network *nw, const std::string &fname, const std::string_view comment,
                   int coord_precision, int (*select)(station *st) )
{
    FILE *stf;
    station *st;
    double northing, easting;
    char projection_coords;
    char geocentric_coords;
    int cp;
    int nclass;
    int ellipsoidal_heights;
    char degrees = 0;
    char explicit_geoid=nw->options & NW_EXPLICIT_GEOID;

    if( !nw || !nw->stnlist || !nw->crdsys ) return MISSING_DATA;

    stf = fopen( fname.c_str(), "w" );
    if( stf == NULL )
    {
        handle_error( FILE_OPEN_ERROR, "Unable to create new coordinate file",
                      fname);
        return FILE_OPEN_ERROR;
    }

    /* Print the header information */

    projection_coords = is_projection( nw->crdsys );
    geocentric_coords = is_geocentric( nw->crdsys );
    ellipsoidal_heights = nw->options & NW_ELLIPSOIDAL_HEIGHTS ? 1 : 0;

    fprintf(stf,"%s\n", nw->name ? nw->name->c_str() : "Unnamed network" );
    fprintf(stf,"%s\n", nw->crdsysdef.c_str());
    fputs("options",stf);

    if( ! geocentric_coords )
    {
        if( nw->options & NW_ELLIPSOIDAL_HEIGHTS )
        {
            fputs(" ellipsoidal_heights",stf);
        }
        else
        {
            fputs(" orthometric_heights",stf);
        }
    }

    if( explicit_geoid && nw->options & NW_DEFLECTIONS )
    {
        fputs(" deflections", stf);
    }
    else
    {
        fputs(" no_deflections", stf);
    }

    if( explicit_geoid && nw->options & NW_GEOID_HEIGHTS )
    {
        fputs(" geoid_heights", stf );
    }
    else
    {
        fputs(" no_geoid_heights", stf );
    }

    if( nw->options & NW_DEC_DEGREES )
    {
        fputs(" degrees", stf );
        degrees = 1;
    }

    nclass = nw->classification_count();
    if( nclass )
    {
        int i;
        for( i = 0; i++ < nclass; )
        {
            std::string name = nw->class_name( i );
            if( nclass==1 && boost::algorithm::iequals(name,STATION_ORDER_CLASS_NAME))
            {
                fputs(" station_orders",stf);
            }
            else
            {
                fputs(" c=",stf);
                fputs(name.c_str(),stf);
            }
        }
    }

    fputs("\n", stf);

    /* Print details of the the program creating the file */

    if( ! comment.empty() ) fprintf( stf,"! %s\n", std::string( comment ).c_str() );

    fprintf(stf,"\n");

    /* Write a station header comment */

    fprintf( stf,"!Code");
    if( geocentric_coords )
    {
        fprintf( stf, " %12s %12s %12s","X   ","Y    ","Z    ");
    }
    else
    {
        if( projection_coords )
        {
            fprintf( stf," %12s %12s","Easting","Northing" );
        }
        else
        {
            fprintf( stf," %18s %18s","Latitude    ","Longitude    ");
        }
        fprintf( stf," %10s", ellipsoidal_heights ? "Ell.Hgt" : "Orth.Hgt" );
    }
    if( explicit_geoid )
    {
        if( nw->options & NW_DEFLECTIONS ) fprintf(stf," %5s %5s","Xi","Eta" );
        if( nw->options & NW_GEOID_HEIGHTS ) fprintf(stf," %7s","G.Hgt" );
    }
    if( nclass )
    {
        int i;
        for( i = 0; i++ < nclass; )
        {
            std::string name = nw->class_name( i );
            fprintf( stf, " %-5s", name.c_str());
        }
    }
    fprintf( stf, " Name\n");

    /* Now write the details of the stations */

    reset_station_list( nw, 0 );

    const DmsFormat latitudeFormat( 3, 6, 0, std::nullopt, std::nullopt, std::nullopt, " N", " S" );
    const DmsFormat longitudeFormat( 3, 6, 0, std::nullopt, std::nullopt, std::nullopt, " E", " W" );

    cp = coord_precision;
    if( cp <= 0 || cp > 10 ) cp = 4;

    while( NULL != (st = next_station(nw) ) )
    {
        if( select && !(*select)(st)) continue;
        fprintf(stf,"%-5s",st->Code.c_str());

        if( projection_coords )
        {
            geog_to_proj( nw->crdsys->prj, st->ELon, st->ELat, &easting, &northing );
            fprintf(stf," %12.*lf %12.*lf",cp,easting,cp,northing);
            fprintf(stf," %10.*lf",cp, st->OHgt + ellipsoidal_heights * st->GUnd );
        }
        else if( geocentric_coords )
        {
            double llh[3];
            double xyz[3];
            llh[CRD_LAT] = st->ELat;
            llh[CRD_LON] = st->ELon;
            llh[CRD_HGT] = st->OHgt + st->GUnd;
            llh_to_xyz( nw->crdsys->rf->el, llh, xyz, NULL, NULL );
            fprintf(stf," %12.*lf %12.*lf %12.*lf",cp,xyz[0],cp,xyz[1],cp,xyz[2] );
        }
        else
        {
            if( degrees )
            {
                fprintf(stf," %18.*lf %18.*lf",cp+7,st->ELat/DTOR,cp+7,st->ELon/DTOR);
            }
            else
            {
                fprintf(stf," %s",dms_string( st->ELat/DTOR, latitudeFormat ).c_str());
                fprintf(stf," %s",dms_string( st->ELon/DTOR, longitudeFormat ).c_str());
            }
            fprintf(stf," %10.*lf",cp, st->OHgt + ellipsoidal_heights * st->GUnd );
        }


        if( explicit_geoid )
        {
            if( nw->options & NW_DEFLECTIONS )
            {
                fprintf(stf," %5.1lf %5.1lf",st->GXi*3600.0/DTOR,
                        st->GEta*3600.0/DTOR );
            }
            if( nw->options & NW_GEOID_HEIGHTS )
            {
                fprintf(stf," %7.*lf",cp,st->GUnd );
            }
        }

        if( nclass )
        {
            int i;
            for( i = 0; i++ < nclass; )
            {
                int clsid = st->get_class( i );
                std::string cval = nw->class_value( i, clsid );
                fprintf( stf, " %-5s", cval.empty() ? "-" : cval.c_str() );
            }
        }

        fprintf(stf," %s\n", st->Name.c_str() );
    }

    fclose( stf );
    return OK;
}
