/*
   $Log: geoid.c,v $
   Revision 1.5  2004/04/22 02:34:33  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.4  1998/05/21 03:56:11  ccrook
   Minor bug fixes

   Revision 1.3  1996/05/20 14:02:31  CHRIS
   Added function get_geoid_model.

   Revision 1.2  1996/05/17 22:24:40  CHRIS
   Added function to get geoid coordinate system code.
   (get_geoid_coordsys)

   Revision 1.1  1995/12/22 17:21:30  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "util/fileutil.h"
#include "string.h"
#include "util/chkalloc.h"
#include "util/errdef.h"
#include "util/dstring.h"
#include "geoid/geoid.h"
#include "geoid/griddata.h"
#include "util/geodetic.h"
#include "util/pi.h"



static const char *get_geoid_filename( const char *geoidname )
{
    const char *geoid = "geoid";
    const char *filename = NULL;

    /* If name explicitely given, then use that. */
    if( geoidname )
    {
        geoid = geoidname;
        filename = find_file( geoid, GEOID_GRID_EXTENSION, 0, FF_TRYALL, COORDSYS_CONFIG_SECTION );
    }
    /* Else if a specific geoid file is defined */
    else if( getenv("GEOIDBIN") )
    {
        filename = getenv("GEOIDBIN" );
    }
    /* Else look for a file named according to the GEOID env variable,
       or failing that, the "geoid.grd" */
    else
    {
        if( getenv("GEOID") ) geoid = getenv("GEOID");
        filename = find_file( geoid, GEOID_GRID_EXTENSION, 0, FF_TRYALL, COORDSYS_CONFIG_SECTION );
    }

    /* Return just the geoid name if a file isn't found */
    if( ! filename ) filename = geoid;
    return filename;
}

const char *create_geoid_filename( const char *geoidname )
{
    const char *filename = get_geoid_filename( geoidname );
    if( ! file_exists(filename) ) return NULL;
    return copy_string( filename );
}

void delete_geoid_filename( const char *filename )
{
    check_free( (void *) filename );
}

geoid_def *create_geoid_grid( const char *source )
{
    int status;
    const char *filename;
    geoid_def *gd = NULL;
    grid_def *grd;
    coordsys *cs = 0;

    filename = get_geoid_filename( source );
    status = grd_open_grid_file( filename, 1, &grd );

    if( status != OK )
    {
        handle_error( status, "Unable to load geoid grid model", source );
    }
    else
    {
        const char *cscode = grd_coordsys_def( grd );
        cs = load_coordsys( cscode );
        if( ! cs )
        {
            grd_delete_grid( grd );
            status = INVALID_DATA;
            handle_error( status, "Invalid coordinate system in geoid definition", source );
        }
        else if( ! is_geodetic( cs ) )
        {
            delete_coordsys( cs );
            status = INVALID_DATA;
            handle_error( status,"Geoid coordinate system must be geodetic", source );
        }
    }

    if( status == OK )
    {
        double dlon, dlat;
        gd = (geoid_def *) check_malloc( sizeof( geoid_def ) );
        gd->cs = cs;
        gd->grd = grd;
        grd_grid_spacing( grd, &dlon, &dlat );
        gd->gridsize = dlat;
    }

    return gd;
}

void delete_geoid_grid( geoid_def *gd )
{
    if( gd )
    {
        if( gd->grd ) grd_delete_grid( gd->grd );
        gd->grd = 0;
        if( gd->cs )delete_coordsys( gd->cs );
        gd->cs = 0;
    }
    check_free( gd );
}

const char *get_geoid_model( geoid_def *gd )
{
    if( !gd ) return NULL;
    return grd_title( gd->grd, 1 );
}

coordsys *get_geoid_coordsys(  geoid_def *gd )
{
    if( !gd ) return NULL;
    return gd->cs;
}

void print_geoid_header( geoid_def *gd, FILE *out, int width, const char *prefix )
{
    int i, nblank;
    if( !gd->grd ) return;
    for( i = 0; i++ < 3; )
    {
        const char *s = grd_title(gd->grd, i);
        if( !s ) continue;
        if( prefix ) fputs( prefix, out );
        if( width > 0 )
        {
            nblank = width - strlen(s)/2;
            if( nblank > 0 ) fprintf( out, "%*s", nblank, "" );
        }
        fprintf(out,"%s\n", s );
    }
}

void print_geoid_data( geoid_def *gd, FILE *out, char showGrid )
{
    if( !gd->grd )
    {
        fprintf(out,"\n\nNo geoid file loaded\n\n");
        return;
    }
    fprintf(out,"\n\nDefinition of geoid model\n");
    grd_print_grid_data( gd->grd, out, showGrid);
}


int calculate_geoid_undulation( geoid_def *gd, double lat, double lon, double *undulation )
{
    if( !gd ) return INCONSISTENT_DATA;
    return grd_calc_cubic( gd->grd, lon*RTOD, lat*RTOD, undulation );
}

int calculate_geoid_exu( geoid_def *gd, double lat, double lon, double exu[3] )
{
    int sts;
    coordsys *cs;
    ellipsoid *el;
    double xyz[3];
    double xyzp[3];
    double dxyz[3];
    double u1, u2;

    double llh[3];
    rotmat toporot;

    sts = calculate_geoid_undulation( gd, lat, lon, &exu[CRD_HGT] );
    if( sts != OK ) return sts;

    cs = gd->cs;
    el = cs->rf->el;

    llh[CRD_LAT] = lat;
    llh[CRD_LON] = lon;
    llh[CRD_HGT] = 0.0;

    llh_to_xyz( el, llh, xyz, NULL, NULL );
    init_toprot( llh[CRD_LAT], llh[CRD_LON], &toporot );

    /* Calculate geoid heights half a grid spacing south (u1) and north (u2)
       of the reference point, and determine the deflection north from
       these */

    llh[CRD_LAT] -= fabs( gd->gridsize/2.0 ) * DTOR;
    sts = calculate_geoid_undulation( gd, llh[CRD_LAT], llh[CRD_LON], &u1 );
    if( sts != OK ) return sts;

    llh_to_xyz( el, llh, xyzp, NULL, NULL );
    vecdif( xyzp, xyz, dxyz );
    vecdif( xyz, dxyz, xyzp );
    xyz_to_llh( el, xyzp, llh );

    sts = calculate_geoid_undulation( gd, llh[CRD_LAT], llh[CRD_LON], &u2 );
    if( sts != OK ) return sts;

    exu[CRD_LAT] = (u1 - u2)/(veclen(dxyz)*2);

    /* Convert the north vector dxyz to an west vector by taking a dot product with
       the local vertical */

    rot_vertical( &toporot, xyzp );
    vecprd( dxyz, xyzp, dxyz );

    /* Calculate the west and east undulations and use this to derive deflection east */

    vecadd( xyz, dxyz, xyzp );
    xyz_to_llh( el, xyzp, llh );

    sts = calculate_geoid_undulation( gd, llh[CRD_LAT], llh[CRD_LON], &u1 );
    if( sts != OK ) return sts;

    vecdif( xyz, dxyz, xyzp );
    xyz_to_llh( el, xyzp, llh );

    sts = calculate_geoid_undulation( gd, llh[CRD_LAT], llh[CRD_LON], &u2 );
    if( sts != OK ) return sts;

    exu[CRD_LON] = (u1 - u2)/(veclen(dxyz)*2);

    return OK;
}
