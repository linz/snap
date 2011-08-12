#include "snapconfig.h"
/* Routines to register New Zealand Map Grid as a projection */

/*
   $Log: psprojr.c,v $
   Revision 1.1  1995/12/22 17:00:20  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "coordsys/crdsys_prj.h"
#include "util/errdef.h"
#include "coordsys/psproj.h"
#include "coordsys/psprojr.h"

static char rcsid[]="$Id: psprojr.c,v 1.1 1995/12/22 17:00:20 CHRIS Exp $";

static int read_north_south( input_string_def *is, void *address );
static int print_north_south( output_string_def *os, void *address );

static param_def psparams[]  =
{
    {
        "Pole","POLE",OFFSET_OF(south,PSProjection),
        read_north_south, print_north_south, print_north_south
    },
    {
        "Central meridian","CM",OFFSET_OF(cm,PSProjection),
        read_radians, print_radians, print_longitude
    },
    {
        "Scale factor","SF", OFFSET_OF(sf,PSProjection),
        double_from_string, print_double3, print_double3
    },
    {
        "False easting","FE", OFFSET_OF(fe,PSProjection),
        double_from_string, print_double3, print_double3
    },
    {
        "False northing", "FN", OFFSET_OF(fn,PSProjection),
        double_from_string, print_double3, print_double3
    },
};

static projection_type *ps_type = NULL;


static int ps_bind_ellipsoid( void *data, ellipsoid *el )
{
    PSProjection *ps;
    ps = (PSProjection *) data;
    define_PSProjection( ps, el->a, el->rf,
                         ps->cm, ps->sf, ps->fe, ps->fn, ps->south );
    return OK;
}

static int ps_proj_to_geog( void *data, double e, double n, double *ln, double *lt )
{
    psp_geod( (PSProjection *) data, e, n, ln, lt );
    return OK;
}

static int ps_geog_to_proj( void *data, double ln, double lt, double *e, double *n )
{
    geod_psp( (PSProjection *) data, ln, lt, e, n );
    return OK;
}

void register_ps_projection( void )
{
    char *code = "PS";
    char *name = "Polar Stereographic";

    projection_type ps;

    if( ps_type ) return;

    ps.code = code;
    ps.name = name;
    ps.size = sizeof(PSProjection);
    ps.params = psparams;
    ps.nparams = COUNT_OF(psparams);
    ps.create = NULL;
    ps.destroy = NULL;
    ps.copy = NULL;
    ps.identical = NULL;
    ps.bind_ellipsoid = ps_bind_ellipsoid;
    ps.geog_to_proj = ps_geog_to_proj;
    ps.proj_to_geog = ps_proj_to_geog;
    ps.calc_sf_cv = NULL;

    ps_type = register_projection_type( &ps );
}

projection *create_ps_projection(  double cm, double sf,
                                   double fe, double fn, char south )
{
    projection *prj;

    if( !ps_type ) register_ps_projection();
    if( !ps_type ) return NULL;

    prj = create_projection( ps_type );
    if( !prj ) return NULL;

    define_PSProjection( (PSProjection *) prj->data, 6378388.0, 297.0,
                         cm, sf, fe, fn, south);

    return prj;
}

static int read_north_south( input_string_def *is, void *address )
{
    char def[11];
    int sts;
    sts = next_string_field( is, def, 10 );
    if( sts != OK ) return sts;
    if( _stricmp(def,"north") == 0 )
    {
        *(char *)address = 0;
    }
    else if( _stricmp(def,"south") == 0 )
    {
        *(char *) address = 1;
    }
    else
    {
        sts = INVALID_DATA;
    }
    return sts;
}

static int print_north_south( output_string_def *os, void *address )
{
    int sts;
    if( *(char *)address )
    {
        sts = write_output_string( os, "South" );
    }
    else
    {
        sts = write_output_string( os, "North" );
    }
    return sts;
}

