#include "snapconfig.h"
/*
   $Log: networkw.c,v $
   Revision 1.1  1995/12/22 17:38:10  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network/network.h"
#include "util/datafile.h"
#include "util/chkalloc.h"
#include "util/dms.h"
#include "util/pi.h"
#include "util/errdef.h"


/*=============================================================*/
/* Basic routine to write a station data file                  */


int write_network( network *nw, const char *fname, const char *comment,
                   int coord_precision, int (*select)(station *st) )
{
    FILE *stf;
    station *st;
    double northing, easting;
    char projection_coords;
    char geocentric_coords;
    void *latfmt, *lonfmt;
    int cp;
    int nclass;
    int ellipsoidal_heights;
    char degrees = 0;

    if( !nw || !nw->stnlist || !nw->crdsys ) return MISSING_DATA;

    if( !fname ) return MISSING_DATA;

    stf = fopen( fname, "w" );
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

    fprintf(stf,"%s\n", nw->name ? nw->name : "Unnamed network" );
    fprintf(stf,"%s\n", nw->crdsysdef);
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

    if( nw->options & NW_DEFLECTIONS )
    {
        fputs(" deflections", stf);
    }
    else
    {
        fputs(" no_deflections", stf);
    }

    if( nw->options & NW_GEOID_HEIGHTS )
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

    nclass = network_classification_count(nw);
    if( nclass )
    {
        int i;
        for( i = 0; i++ < nclass; )
        {
            const char *name = network_class_name( nw, i );
            if( nclass==1 && _stricmp(name,STATION_ORDER_CLASS_NAME)==0)
            {
                fputs(" station_orders",stf);
            }
            else
            {
                fputs(" c=",stf);
                fputs(name,stf);
            }
        }
    }

    fputs("\n", stf);

    /* Print details of the the program creating the file */

    if( comment && strlen(comment) > 0 ) fprintf( stf,"! %s\n", comment );

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
    if( nw->options & NW_DEFLECTIONS ) fprintf(stf," %5s %5s","Xi","Eta" );
    if( nw->options & NW_GEOID_HEIGHTS ) fprintf(stf," %7s","G.Hgt" );
    if( nclass )
    {
        int i;
        for( i = 0; i++ < nclass; )
        {
            const char *name = network_class_name( nw, i );
            fprintf( stf, " %-5s", name);
        }
    }
    fprintf( stf, " Name\n");

    /* Now write the details of the stations */

    reset_station_list( nw, 0 );

    if( !projection_coords && !geocentric_coords && !degrees )
    {
        latfmt = create_dms_format(3,6,0,NULL,NULL,NULL," N"," S");
        lonfmt = create_dms_format(3,6,0,NULL,NULL,NULL," E"," W");
    }
    else
    {
        latfmt = lonfmt = 0;
    }

    cp = coord_precision;
    if( cp <= 0 || cp > 10 ) cp = 4;

    while( NULL != (st = next_station(nw) ) )
    {
        if( select && !(*select)(st)) continue;
        fprintf(stf,"%-5s",st->Code);

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
                fprintf(stf," %s",dms_string( st->ELat/DTOR, latfmt, NULL ));
                fprintf(stf," %s",dms_string( st->ELon/DTOR, lonfmt, NULL ));
            }
            fprintf(stf," %10.*lf",cp, st->OHgt + ellipsoidal_heights * st->GUnd );
        }


        if( nw->options & NW_DEFLECTIONS )
        {
            fprintf(stf," %5.1lf %5.1lf",st->GXi*3600.0/DTOR,
                    st->GEta*3600.0/DTOR );
        }
        if( nw->options & NW_GEOID_HEIGHTS )
        {
            fprintf(stf," %7.*lf",cp,st->GUnd );
        }

        if( nclass )
        {
            int i;
            for( i = 0; i++ < nclass; )
            {
                int clsid = get_station_class( st, i );
                const char *cval = network_class_value( nw, i, clsid );
                fprintf( stf, " %-5s", cval ? cval : "-" );
            }
        }

        fprintf(stf," %s\n", st->Name );
    }

    if( latfmt ) check_free( latfmt );
    if( lonfmt ) check_free( lonfmt );

    fclose( stf );
    return OK;
}
