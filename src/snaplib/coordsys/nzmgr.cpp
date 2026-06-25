#include "snapconfig.h"
/* Routines to register New Zealand Map Grid as a projection */

/*
   $Log: nzmgr.c,v $
   Revision 1.1  1995/12/22 16:57:25  CHRIS
   Initial revision

*/

#include <stdio.h>
#include "util/errdef.h"
#include "coordsys/crdsys_prj.h"
#include "coordsys/nzmg.h"
#include "coordsys/nzmgr.h"

static projection_type *nzmg_type = NULL;

// #pragma warning (disable : 4100)

static int nzmg_proj_to_geog( void *, double e, double n, double *ln, double *lt )
{
    nzmg_geod( e, n, ln, lt );
    return OK;
}

static int nzmg_geog_to_proj( void *, double ln, double lt, double *e, double *n )
{
    geod_nzmg( ln, lt, e, n );
    return OK;
}

void register_nzmg_projection( void )
{
    const char *code = "NZMG";
    const char *name = "New Zealand Map Grid";
    projection_type nzmg;

    if( nzmg_type ) return;

    nzmg.code = code;
    nzmg.name = name;
    nzmg.size = 0;
    nzmg.params = 0;
    nzmg.nparams = 0;
    nzmg.create = 0;
    nzmg.destroy = 0;
    nzmg.copy = 0;
    nzmg.identical = 0;
    nzmg.bind_ellipsoid = 0;
    nzmg.geog_to_proj = nzmg_geog_to_proj;
    nzmg.proj_to_geog = nzmg_proj_to_geog;
    nzmg.calc_sf_cv = 0;

    nzmg_type = register_projection_type( &nzmg );
}

projection *create_nzmg_projection( void )
{
    if( !nzmg_type ) register_nzmg_projection();
    if( !nzmg_type ) return NULL;
    return create_projection( nzmg_type );
}


