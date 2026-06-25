#include "snapconfig.h"
/* Routines to register New Zealand Map Grid as a projection */

/*
   $Log: tmprojr.c,v $
   Revision 1.1  1995/12/22 17:01:58  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "coordsys/crdsys_prj.h"
#include "util/errdef.h"
#include "coordsys/tmproj.h"
#include "coordsys/tmprojr.h"

static param_def tmparams[]  =
{
    {
        "Central meridian","CM",OFFSET_OF(meridian,tmprojection),
        read_radians, print_radians, print_longitude
    },
    {
        "Origin of latitude","LO",OFFSET_OF(orglat,tmprojection),
        read_radians, print_radians, print_latitude
    },
    {
        "Scale factor","SF", OFFSET_OF(scalef,tmprojection),
        double_from_string, print_double6, print_double6
    },
    {
        "False easting","FE", OFFSET_OF(falsee,tmprojection),
        double_from_string, print_double3, print_double3
    },
    {
        "False northing", "FN", OFFSET_OF(falsen,tmprojection),
        double_from_string, print_double3, print_double3
    },
    {
        "Unit to metres conversion","UTOM",OFFSET_OF(utom,tmprojection),
        double_from_string, print_double6, print_double6
    }
};

static projection_type *tm_type = NULL;


static int tm_bind_ellipsoid( void *data, ellipsoid *el )
{
    tmprojection *tm;
    tm = (tmprojection *) data;
    define_tmprojection( tm, el->a, el->rf,
                         tm->meridian, tm->scalef, tm->orglat, tm->falsee, tm->falsen, tm->utom );
    return OK;
}

static int tm_proj_to_geog( void *data, double e, double n, double *ln, double *lt )
{
    tm_geod( (tmprojection *) data, e, n, ln, lt );
    return OK;
}

static int tm_geog_to_proj( void *data, double ln, double lt, double *e, double *n )
{
    geod_tm( (tmprojection *) data, ln, lt, e, n );
    return OK;
}

void register_tm_projection( void )
{
    const char *code = "TM";
    const char *name = "Transverse Mercator";

    projection_type tm;

    if( tm_type ) return;

    tm.code = code;
    tm.name = name;
    tm.size = sizeof(tmprojection);
    tm.params = tmparams;
    tm.nparams = COUNT_OF(tmparams);
    tm.create = NULL;
    tm.destroy = NULL;
    tm.copy = NULL;
    tm.identical = NULL;
    tm.bind_ellipsoid = tm_bind_ellipsoid;
    tm.geog_to_proj = tm_geog_to_proj;
    tm.proj_to_geog = tm_proj_to_geog;
    tm.calc_sf_cv = NULL;

    tm_type = register_projection_type( &tm );
}

projection *create_tm_projection(  double cm, double sf, double lto,
                                   double fe, double fn, double utom )
{
    projection *prj;

    if( !tm_type ) register_tm_projection();
    if( !tm_type ) return NULL;

    prj = create_projection( tm_type );
    if( !prj ) return NULL;

    define_tmprojection( (tmprojection *) prj->data, 6378388.0, 297.0,
                         cm, sf, lto, fe, fn, utom );

    return prj;
}

