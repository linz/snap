#include "snapconfig.h"
/*
   $Log: networkr.c,v $
   Revision 1.2  1998/05/21 04:00:27  ccrook
   Added geodetic coordinate system to network object and facilitated getting and setting
   coordinates in the network coordinate system.

   Revision 1.1  1995/12/22 20:01:49  CHRIS
   Initial revision

   Revision 1.1  1995/12/22 17:36:53  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network/network.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/datafile.h"
#include "util/dms.h"
#include "util/errdef.h"
#include "util/pi.h"

#define INRECLEN 256


/*=============================================================*/
/* Basic routine to read a station data file                   */
/* Reads a SNAP format file, or the very similar geodetic      */
/* database format file.                                       */

int read_network( network *nw, const char *fname, int gbformat )
{
    DATAFILE *stf;
    char stcode[STNCODELEN+1];
    char inrec[INRECLEN];
    double lat, lon, hgt, xi, eta, und, easting, northing;
    char lth[2],lnh[2];
    station *st;
    char *stname;
    int sts, dfsts;
    char projection_coords;
    char geocentric_coords;
    char options;
    char degrees;
    int nclass;
    int *clsids;
    int i;
    coordsys *cs;

    dfsts = OK;

    clear_network( nw );

    stf = df_open_data_file( fname, "station coordinate file" );
    if( stf == NULL ) return FILE_OPEN_ERROR;

    /* Read in the name of the network */

    df_read_data_file( stf);
    if( df_read_rest( stf, inrec, INRECLEN ) )
    {
        nw->name = copy_string( inrec );
    }

    /* Read in the coordinate system definition */

    df_read_data_file( stf );
    df_read_rest( stf, inrec, INRECLEN);

    cs = load_coordsys( inrec);
    if( !cs )
    {
        df_data_file_error( stf, INVALID_DATA,
                            "Invalid or missing definition of coordinate system");
        df_close_data_file( stf );
        clear_network( nw );
        return INVALID_DATA;
    }

    set_network_coordsys( nw, cs, 0.0, 0, 0 );
    delete_coordsys( cs );
    nw->crdsysdef = copy_string( inrec );
    projection_coords = is_projection( nw->crdsys );
    geocentric_coords = is_geocentric( nw->crdsys );

    /* If geodetic branch format then skip over comments section */

    if( gbformat )
    {
        options = 0;
        df_skip_to_blank_line( stf );
        df_read_data_file( stf );
    }

    else
    {
        int options_record = 0;

        /* Otherwise check if the file includes geoid perturbations */

        df_read_data_file( stf );
        df_read_field( stf, inrec, INRECLEN );

        if( _stricmp(inrec,"options") == 0 || _stricmp(inrec,"no_geoid") == 0 ) options_record = 1;
        if( _stricmp(inrec,"options") == 0 )
        {
            options = 0;
        }
        else
        {
            /* Previous version used default options of NW_GEOID_HEIGHTS and NW_UNDULATIONS */
            options = NW_GEOID_HEIGHTS | NW_DEFLECTIONS;
            df_reread_field( stf );
        }

        if( options_record )
        {
            while( df_read_field( stf, inrec, INRECLEN ))
            {
                if( _stricmp(inrec,"no_geoid") == 0 )
                {
                    options &=  ~NW_GEOID_INFO;
                }
                else if( _stricmp(inrec,"geoid") == 0 )
                {
                    options |= NW_GEOID_INFO;
                }
                else if( _stricmp(inrec,"geoid_heights") == 0 )
                {
                    options |= NW_GEOID_HEIGHTS;
                }
                else if( _stricmp(inrec,"no_geoid_heights") == 0 )
                {
                    options &= ~NW_GEOID_HEIGHTS;
                }
                else if( _stricmp(inrec,"deflections") == 0 )
                {
                    options |= NW_DEFLECTIONS;
                }
                else if( _stricmp(inrec,"no_deflections") == 0 )
                {
                    options &= ~ NW_DEFLECTIONS;
                }
                else if (_stricmp(inrec,"ellipsoidal_heights") == 0 )
                {
                    options |= NW_ELLIPSOIDAL_HEIGHTS;
                }
                else if (_stricmp(inrec,"orthometric_heights") == 0 )
                {
                    options &= ~NW_ELLIPSOIDAL_HEIGHTS;
                }
                else if( _stricmp( inrec, "degrees" ) == 0 )
                {
                    options |= NW_DEC_DEGREES;
                }
                else if( strlen(inrec) > 2 && _strnicmp( inrec, "c=", 2) == 0 )
                {
                    /* Get the class id, which creates the classification */
                    network_class_id( nw, inrec+2, 1 );
                }
                else if( _stricmp( inrec, "station_orders" ) == 0 )
                {
                    network_class_id( nw, STATION_ORDER_CLASS_NAME, 1 );
                }
                else if( _stricmp( inrec, "no_station_orders" ) == 0 )
                {
                    /* do nothing - option no longer possible but kept to avoid errors */
                }
                else
                {
                    if( options_record )
                    {
                        df_data_file_error(stf, INVALID_DATA,"Invalid coordinate file options definition");
                        dfsts = INVALID_DATA;
                    }
                    break;
                }
            }

            df_read_data_file( stf );
        }
    }

    /* Geocentric coordinates always have ellipsoidal heights */

    if( geocentric_coords ) options |= NW_ELLIPSOIDAL_HEIGHTS;
    nw->options = options;

    /* Now on to the station coordinates themselves */

    nw->stnlist = new_station_list();
    xi = 0.0;
    eta = 0.0;
    und = 0.0;
    lat = 0.0;
    lon = 0.0;
    hgt = 0.0;
    easting = 0.0;
    northing = 0.0;
    degrees = options & NW_DEC_DEGREES;
    nclass = network_classification_count(nw);
    clsids = 0;
    if( nclass ) clsids = (int *) check_malloc( (nclass+1) * sizeof(int));

    do
    {

        sts =  df_read_code( stf, stcode, STNCODELEN+1 );
        if( ! sts ) continue;

        if( sl_find_station( nw->stnlist, stcode ) > 0 )
        {
            char errmsg [30+STNCODELEN];
            sprintf(errmsg,"Duplicate station code %s",stcode);
            df_data_file_error(stf, INVALID_DATA,errmsg);
            dfsts = INVALID_DATA;
            continue;
        }

        if( sts )
        {
            if( projection_coords )
            {
                sts = df_read_double( stf, &easting ) && df_read_double( stf, &northing );
                if( sts ) proj_to_geog(nw->crdsys->prj,easting,northing,&lon,&lat);
                if( sts ) sts = df_read_double( stf, &hgt );
            }
            else if( geocentric_coords )
            {
                double xyz[3],llh[3];
                sts = df_read_double( stf, &xyz[0] ) &&
                      df_read_double( stf, &xyz[1] ) &&
                      df_read_double( stf, &xyz[2] );
                xyz_to_llh( nw->crdsys->rf->el, xyz, llh );
                lat = llh[CRD_LAT];
                lon = llh[CRD_LON];
                hgt = llh[CRD_HGT];
            }
            else if( degrees )
            {
                sts = df_read_double( stf, &lat ) &&
                      df_read_double( stf, &lon ) &&
                      df_read_double( stf, &hgt );
                if( sts ) { lat *= DTOR; lon *= DTOR; }
            }
            else
            {
                sts = df_read_dmsangle( stf, &lat ) && df_read_field( stf, lth, 2 ) &&
                      df_read_dmsangle( stf, &lon ) && df_read_field( stf, lnh, 2 );
                if( lth[0] == 's' || lth[0] == 'S' ) lat = -lat;
                if( lnh[0] == 'w' || lnh[0] == 'W' ) lon = -lon;
                if( sts ) sts = df_read_double( stf, &hgt );
            }
        }

        if( sts && options & NW_DEFLECTIONS )
        {
            sts = df_read_double( stf, &xi ) && df_read_double( stf, &eta );
            xi = xi*DTOR/3600.0;
            eta = eta*DTOR/3600.0;
        }

        if( sts && options & NW_GEOID_HEIGHTS )
        {
            sts = df_read_double( stf, &und );
            if( options & NW_ELLIPSOIDAL_HEIGHTS ) hgt -= und;
        }

        if( sts && nclass > 0 )
        {
            int i;
            for( i = 0; i++ < nclass; )
            {
                int clsid;
                sts = df_read_field( stf,inrec, INRECLEN );
                if( ! sts ) break;
                clsid = network_class_value_id(nw,i,inrec,1);
                clsids[i] = clsid;
            }
        }

        stname = stcode;
        if( sts && df_read_rest(stf,inrec, INRECLEN ) ) stname = inrec;

        if( !sts )
        {
            df_data_file_error(stf, INVALID_DATA,"Invalid station coordinate definition");
            dfsts = INVALID_DATA;
            continue;
        }

        st = new_network_station( nw, stcode, stname, lat, lon, hgt, xi, eta, und );
        if( nclass )
        {
            for( i = 0; i++ < nclass; )
            {
                set_station_class( st, i, clsids[i]);
            }
        }
    }
    while( df_read_data_file(stf) == OK );

    /*  Removing this check as can allow empty coordinate files ...
    if( number_of_stations(nw) <= 0 ) {
       df_data_file_error( stf, MISSING_DATA, "No stations defined in the coordinate file");
       if( dfsts == OK ) dfsts = MISSING_DATA;
       clear_network( nw );
       }
    */

    if( clsids ) check_free( clsids );

    df_close_data_file( stf );

    return dfsts;
}
