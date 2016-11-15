#include "snapconfig.h"

#include <stdio.h>

#include "coordsys/coordsys.h"
#include "geoid/geoid.h"
#include "network/network.h"
#include "util/errdef.h"

// #define NW_HGTFIXEDOPT_DEFAULT 0
// #define NW_HGTFIXEDOPT_ELLIPSOIDAL 1
// #define NW_HGTFIXEDOPT_ORTHOMETRIC 2

int calculate_network_coordsys_geoid( network *nw, int errlevel )
{
    if( ! coordsys_heights_orthometric(nw->crdsys) ) return OK;
    if( nw->options & NW_EXPLICIT_GEOID ) return OK;
    return calc_station_geoid_info_from_coordsys( nw, nw->crdsys, NW_HGTFIXEDOPT_DEFAULT, errlevel );
}

int calc_station_geoid_info_from_coordsys( network *nw, coordsys *cs, int fixed_height_type, int errlevel )
{
    int ellipsoidal_heights;
    double llh[3];
    
    if( fixed_height_type == NW_HGTFIXEDOPT_ELLIPSOIDAL )
    {
        ellipsoidal_heights=1;
    }
    else if( fixed_height_type == NW_HGTFIXEDOPT_ORTHOMETRIC )
    {
        ellipsoidal_heights=0;
    }
    else if( network_height_coord_is_ellipsoidal( nw ) )
    {
        ellipsoidal_heights=1;
    }
    else if( network_has_geoid_info( nw ) )
    {
        ellipsoidal_heights=0;
    }
    else
    {
        ellipsoidal_heights=1;
    }


    if( errlevel != OK && errlevel != INFO_ERROR ) errlevel=INCONSISTENT_DATA;

    /* Check that the height ref conversion is possible */
    /* Mainly checking grid file can be opened */

    int sts = coordsys_geoid_exu( cs, llh, NULL, NULL );
    if( sts != OK ) return sts;

    /* Conversion to and from height ref coordsys */

    coord_conversion to_geoid;
    coord_conversion from_geoid;

    if( define_ellipsoidal_coord_conversion_epoch( &to_geoid, nw->geosys, cs, DEFAULT_CRDSYS_EPOCH ) != OK ||
            define_ellipsoidal_coord_conversion_epoch( &from_geoid, cs, nw->geosys, DEFAULT_CRDSYS_EPOCH ) != OK )
    {
        handle_error( INVALID_DATA,
                      "Cannot relate height reference and network coordinate systems",NO_MESSAGE );
        return INVALID_DATA;
    }
    /* Add the undulations - ignore individual errors */

    int maxstn = number_of_stations( nw  );
    int ninvalid = 0;
    int ncalc = 0;
    errhandler_type saved_error_handler=set_error_handler(  null_error_handler );

    for( int istn = 0; istn++ < maxstn; )
    {
        double exu[3];
        double inputUndulation;
        station *st = station_ptr(nw,istn);
        if( !st ) continue;
        llh[CRD_LON] = st->ELon;
        llh[CRD_LAT] = st->ELat;
        llh[CRD_HGT] = 0.0;
        exu[CRD_LON] = 0.0;
        exu[CRD_LAT] = 0.0;
        exu[CRD_HGT] = 0.0;

        ninvalid++;

        if( convert_coords( &to_geoid, llh, NULL, llh, NULL ) != OK ) continue;
        if( coordsys_geoid_exu( cs, llh, NULL, exu ) != OK ) continue;
        if( convert_coords( &from_geoid, llh, exu, llh, exu ) != OK ) continue;

        inputUndulation = st->GUnd;
        st->GUnd = exu[CRD_HGT];
        st->GXi = exu[CRD_LAT];
        st->GEta = exu[CRD_LON];
        // If network is based on ellipsoidal heights, then changing the geoid
        // shouldn't change the ellipsoidal height...
        if( ellipsoidal_heights ) st->OHgt += (inputUndulation - st->GUnd);
        ninvalid--;
        ncalc++;
    }

    set_error_handler(  saved_error_handler );

    /* Set up the network height flags */

    nw->options |= NW_GEOID_HEIGHTS | NW_DEFLECTIONS;

    if( ninvalid && errlevel != OK )
    {
        char buffer[80];
        sprintf( buffer, "Error calculating geoid for %d network stations",ninvalid);
        handle_error( errlevel, buffer, NO_MESSAGE );
        return errlevel;
    }
    return OK;
}

int set_network_geoid( network *nw, const char *geoid, int fixed_height_type, int errlevel )
{
    geoid_def *gd = create_geoid_grid( geoid );
    if( !gd )
    {
        printf("Unable to load geoid model\n");
        return INVALID_DATA;
    }
    int sts = set_network_geoid_def( nw, gd, fixed_height_type, errlevel );
    delete_geoid_grid( gd );
    return sts;
}

int set_network_geoid_def( network *nw, geoid_def *gd, int fixed_height_type, int errlevel )
{

    int ellipsoidal_heights;
    
    if( fixed_height_type == NW_HGTFIXEDOPT_ELLIPSOIDAL )
    {
        ellipsoidal_heights=1;
    }
    else if( fixed_height_type == NW_HGTFIXEDOPT_ORTHOMETRIC )
    {
        ellipsoidal_heights=0;
    }
    else if( network_height_coord_is_ellipsoidal( nw ) )
    {
        ellipsoidal_heights=1;
    }
    else if( network_has_geoid_info( nw ) )
    {
        ellipsoidal_heights=0;
    }
    else
    {
        ellipsoidal_heights=1;
    }

    if( errlevel != OK && errlevel != INFO_ERROR ) errlevel=INCONSISTENT_DATA;

    /* Load the definition of the geoid */

    coordsys *geoid_crdsys = get_geoid_coordsys( gd );

    /* Define the conversion to and from the geoid coordinate system */

    /* Note that the coordinate conversion does not need to be very precise, so
       use a default epoch of 2000 to allow conversion between different
       dynamically related coordinate systems (eg 14 param bursa wolf or
       deformation model */

    coord_conversion to_geoid;
    coord_conversion from_geoid;

    if( define_ellipsoidal_coord_conversion_epoch( &to_geoid, nw->geosys, geoid_crdsys, DEFAULT_CRDSYS_EPOCH ) != OK ||
            define_ellipsoidal_coord_conversion_epoch( &from_geoid, geoid_crdsys, nw->geosys, DEFAULT_CRDSYS_EPOCH ) != OK )
    {
        handle_error( INVALID_DATA,
                      "Cannot relate geoid and network coordinate systems",NO_MESSAGE );
        return INVALID_DATA;
    }

    /* Add the undulations */

    int maxstn = number_of_stations( nw  );
    int ninvalid = 0;
    int ncalc = 0;
    for( int istn = 0; istn++ < maxstn; )
    {
        double llh[3], exu[3];
        double inputUndulation;
        station *st = station_ptr(nw,istn);
        if( !st ) continue;
        llh[CRD_LON] = st->ELon;
        llh[CRD_LAT] = st->ELat;
        llh[CRD_HGT] = 0.0;
        exu[CRD_LON] = 0.0;
        exu[CRD_LAT] = 0.0;
        exu[CRD_HGT] = 0.0;

        ninvalid++;

        if( convert_coords( &to_geoid, llh, NULL, llh, NULL ) != OK ) continue;
        if( calculate_geoid_exu( gd, llh[1], llh[0], exu ) != OK ) continue;
        if( convert_coords( &from_geoid, llh, exu, llh, exu ) != OK ) continue;
        inputUndulation = st->GUnd;
        st->GUnd = exu[CRD_HGT];
        st->GXi = exu[CRD_LAT];
        st->GEta = exu[CRD_LON];
        // If network is based on ellipsoidal heights, then changing the geoid
        // shouldn't change the ellipsoidal height...
        if( ellipsoidal_heights ) st->OHgt += (inputUndulation - st->GUnd);
        ninvalid--;
        ncalc++;
    }

    /* Set up the network height flags */

    nw->options |= NW_GEOID_HEIGHTS | NW_DEFLECTIONS | NW_EXPLICIT_GEOID;

    if( ninvalid && errlevel != OK )
    {
        char buffer[80];
        sprintf( buffer, "Error calculating geoid for %d network stations",ninvalid);
        handle_error( errlevel, buffer, NO_MESSAGE );
        return errlevel;
    }
    return OK;
}


