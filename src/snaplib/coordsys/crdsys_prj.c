#include "snapconfig.h"
/* crdsysp0.c:  Projection management for coordinate system routines */

/*
   $Log: crdsysp0.c,v $
   Revision 1.1  1995/12/22 16:39:34  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include "coordsys/crdsys_prj.h"
#include "util/chkalloc.h"

static char rcsid[]="$Id: crdsysp0.c,v 1.1 1995/12/22 16:39:34 CHRIS Exp $";

projection *create_projection(  projection_type *type )
{
    projection *prj;
    if( !type ) return NULL;
    prj = (projection *) check_malloc( sizeof(projection) );
    prj->type = type;
    prj->data = NULL;
    if( prj->type->create)
    {
        prj->data = (*prj->type->create)();
    }
    else if ( prj->type->size )
    {
        prj->data = check_malloc( prj->type->size );
    }
    return prj;
}

void set_projection_ellipsoid( projection *prj, ellipsoid *el )
{
    if( prj && prj->type && prj->type->bind_ellipsoid && el )
    {
        (*prj->type->bind_ellipsoid)( prj->data, el );
    }
}

void delete_projection( projection *prj )
{
    if( !prj ) return;
    if( prj->type->destroy && prj->data )
    {
        (*prj->type->destroy)( prj->data );
    }
    else if( prj->type->size && prj->data )
    {
        check_free( prj->data );
    }
    check_free( prj );
}

projection *copy_projection( projection *prj )
{
    projection *newprj;

    if( prj == NULL ) return NULL;

    newprj = create_projection( prj->type );
    if( !newprj ) return newprj;

    if( prj->type->copy )
    {
        (*prj->type->copy)( newprj->data, prj->data );
    }
    else if( prj->type->size )
    {
        memcpy( newprj->data, prj->data, prj->type->size );
    }
    return newprj;
}


int identical_projections( projection *prj1, projection *prj2 )
{
    if( prj1->type != prj2->type ) return 0;
    if( prj1->type->identical )
    {
        return (*prj1->type->identical)( prj1->data, prj2->data );
    }
    return memcmp(prj1->data, prj2->data, prj1->type->size ) ? 0 : 1;
}

