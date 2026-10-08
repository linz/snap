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
    if( nzmg_type ) return;

    nzmg_type = register_projection_type( new projection_type(
        "NZMG", "New Zealand Map Grid", 0, nullptr, 0,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nzmg_geog_to_proj, nzmg_proj_to_geog, nullptr ) );
}

projection *create_nzmg_projection( void )
{
    if( !nzmg_type ) register_nzmg_projection();
    if( !nzmg_type ) return NULL;
    return new projection( *nzmg_type );
}


