#include "snapconfig.h"
/* Routines to register New Zealand Map Grid as a projection */

/*
   $Log: gnprojr.c,v $
   Revision 1.1  1995/12/22 16:53:25  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "coordsys/crdsys_prj.h"
#include "util/errdef.h"
#include "coordsys/gnproj.h"
#include "coordsys/gnprojr.h"

static param_def gnparams[]  =
{
    {
        "Origin of latitude","LO",OFFSET_OF(orglat,GnomicProjection),
        read_radians, print_radians, print_latitude
    },
    {
        "Central meridian","CM",OFFSET_OF(orglon,GnomicProjection),
        read_radians, print_radians, print_longitude
    },
    {
        "False easting","FE", OFFSET_OF(fe,GnomicProjection),
        double_from_string, print_double3, print_double3
    },
    {
        "False northing", "FN", OFFSET_OF(fn,GnomicProjection),
        double_from_string, print_double3, print_double3
    },
};

static projection_type *gn_type = NULL;


static int gn_bind_ellipsoid( void *data, ellipsoid *el )
{
    GnomicProjection *gn;
    gn = (GnomicProjection *) data;
    define_gnomic_projection( gn, el->a, el->rf, gn->orglat, gn->orglon, gn->fe, gn->fn );
    return OK;
}

static int gn_proj_to_geog( void *data, double e, double n, double *ln, double *lt )
{
    gnomic_geod( (GnomicProjection *) data, e, n, ln, lt );
    return OK;
}

static int gn_geog_to_proj( void *data, double ln, double lt, double *e, double *n )
{
    geod_gnomic( (GnomicProjection *) data, ln, lt, e, n );
    return OK;
}

void register_gnomic_projection( void )
{
    if( gn_type ) return;

    gn_type = register_projection_type( new projection_type(
        "GN", "Gnomic", sizeof(GnomicProjection), gnparams, COUNT_OF(gnparams),
        nullptr, nullptr, nullptr, gn_bind_ellipsoid, nullptr,
        gn_geog_to_proj, gn_proj_to_geog, nullptr ) );
}

projection *create_gnomic_projection(  double orglat, double orglon,
                                       double fe, double fn )
{

    projection *prj;

    if( !gn_type ) register_gnomic_projection();
    if( !gn_type ) return NULL;

    prj = new projection( *gn_type );

    define_gnomic_projection( (GnomicProjection *) prj->data, 6378388.0, 297.0,
                              orglat, orglon, fe, fn );

    return prj;
}


