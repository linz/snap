#include "snapconfig.h"
/* Routines to register New Zealand Map Grid as a projection */

/*
   $Log: emprojr.c,v $
   Revision 1.1  1995/12/22 16:51:42  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "coordsys/crdsys_prj.h"
#include "util/errdef.h"
#include "coordsys/emproj.h"
#include "coordsys/emprojr.h"

static char rcsid[]="$Id: emprojr.c,v 1.1 1995/12/22 16:51:42 CHRIS Exp $";

static param_def emparams[]  =
{
    {
        "Central meridian","CM",OFFSET_OF(cm,EMProjection),
        read_radians, print_radians, print_longitude
    },
    {
        "Standard parallel","CM",OFFSET_OF(rlat,EMProjection),
        read_radians, print_radians, print_latitude
    },
};

static projection_type *em_type = NULL;


static int em_bind_ellipsoid( void *data, ellipsoid *el )
{
    EMProjection *em;
    em = (EMProjection *) data;
    define_EMProjection( em, el->a, el->rf, em->cm, em->rlat );
    return OK;
}

static int em_proj_to_geog( void *data, double e, double n, double *ln, double *lt )
{
    em_geod( (EMProjection *) data, e, n, ln, lt );
    return OK;
}

static int em_geog_to_proj( void *data, double ln, double lt, double *e, double *n )
{
    geod_em( (EMProjection *) data, ln, lt, e, n );
    return OK;
}

void register_em_projection( void )
{
    char *code = "EM";
    char *name = "Equatorial Mercator";

    projection_type em;

    if( em_type ) return;

    em.code = code;
    em.name = name;
    em.size = sizeof(EMProjection);
    em.params = emparams;
    em.nparams = COUNT_OF(emparams);
    em.create = NULL;
    em.destroy = NULL;
    em.copy = NULL;
    em.identical = NULL;
    em.bind_ellipsoid = em_bind_ellipsoid;
    em.geog_to_proj = em_geog_to_proj;
    em.proj_to_geog = em_proj_to_geog;
    em.calc_sf_cv = NULL;

    em_type = register_projection_type( &em );
}

projection *create_em_projection(  double cm, double sp )
{
    projection *prj;

    if( !em_type ) register_em_projection();
    if( !em_type ) return NULL;

    prj = create_projection( em_type );
    if( !prj ) return NULL;

    define_EMProjection( (EMProjection *) prj->data, 6378388.0, 297.0,
                         cm, sp );

    return prj;
}

