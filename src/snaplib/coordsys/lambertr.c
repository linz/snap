#include "snapconfig.h"
/* Routines to register Lambert Conformal Conic as a projection */

/*
   $Log: lambertr.c,v $
   Revision 1.1  1995/12/22 16:56:01  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "util/errdef.h"
#include "coordsys/crdsys_prj.h"
#include "coordsys/lambert.h"
#include "coordsys/lambertr.h"
#include "util/pi.h"

static char rcsid[]="$Id: lambertr.c,v 1.1 1995/12/22 16:56:01 CHRIS Exp $";

static projection_type *LCC_type = NULL;

static param_def LCCparams[] =
{
    {
        "First standard parallel","SP1",OFFSET_OF(sp1,LCCProjection),
        read_radians, print_radians, print_latitude
    },
    {
        "Second standard parallel","SP2",OFFSET_OF(sp2,LCCProjection),
        read_radians, print_radians, print_latitude
    },
    {
        "Origin of latitude","LT",OFFSET_OF(lt0,LCCProjection),
        read_radians, print_radians, print_latitude
    },
    {
        "Origin of longitude","LN",OFFSET_OF(ln0,LCCProjection),
        read_radians, print_radians, print_longitude
    },
    {
        "False easting","FE", OFFSET_OF(e0,LCCProjection),
        double_from_string, print_double3, print_double3
    },
    {
        "False northing", "FN", OFFSET_OF(n0,LCCProjection),
        double_from_string, print_double3, print_double3
    }
};


static int LCC_bind_ellipsoid( void *data, ellipsoid *el )
{
    LCCProjection *lp;
    lp = (LCCProjection *) data;
    defineLCCProjection( lp, el->a, el->rf, lp->sp1, lp->sp2,
                         lp->lt0, lp->ln0, lp->e0, lp->n0 );
    return OK;
}

static int LCC_proj_to_geog( void *data, double e, double n, double *ln, double *lt )
{
    convertLCCToGeog( (LCCProjection *) data, e, n, ln, lt );
    return OK;
}

static int LCC_geog_to_proj( void *data, double ln, double lt, double *e, double *n )
{
    convertGeogToLCC( (LCCProjection *) data, ln, lt, e, n, NULL, NULL );
    return OK;
}

void register_lcc_projection( void )
{
    char *code = "LCC";
    char *name = "Lambert Conformal Conic";
    projection_type lcc;

    if( LCC_type ) return;

    lcc.code = code;
    lcc.name = name;
    lcc.size = sizeof(LCCProjection);
    lcc.params = LCCparams;
    lcc.nparams = COUNT_OF(LCCparams);
    lcc.create = 0;
    lcc.destroy = 0;
    lcc.copy = 0;
    lcc.identical = 0;
    lcc.bind_ellipsoid = LCC_bind_ellipsoid;
    lcc.geog_to_proj = LCC_geog_to_proj;
    lcc.proj_to_geog = LCC_proj_to_geog;
    lcc.calc_sf_cv = 0;

    LCC_type = register_projection_type( &lcc );
}

projection *create_lcc_projection(  double sp1, double sp2,
                                    double lt0, double ln0, double e, double n )
{
    projection *prj;

    if( !LCC_type ) register_lcc_projection();
    if( !LCC_type ) return NULL;

    prj = create_projection( LCC_type );
    if( !prj ) return NULL;

    defineLCCProjection( (LCCProjection *) prj->data, 6378388.0, 297.0,
                         sp1, sp2, lt0, ln0, e, n );

    return prj;
}

