#include "snapconfig.h"
/* crdsysp0.c:  Projection management for coordinate system routines */

/*
   $Log: crdsysp0.c,v $
   Revision 1.1  1995/12/22 16:39:34  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <new>
#include "coordsys/crdsys_prj.h"

projection::projection( projection_type &projtype ) : type( &projtype ), data( nullptr )
{
    if( type->create )
    {
        data = (*type->create)();
    }
    else if ( type->size )
    {
        data = ::operator new( type->size );
    }
}

projection::projection( const projection &other ) : projection( *other.type )
{
    if( type->copy )
    {
        (*type->copy)( data, other.data );
    }
    else if( type->size )
    {
        memcpy( data, other.data, type->size );
    }
}

projection::~projection()
{
    if( type->destroy && data )
    {
        (*type->destroy)( data );
    }
    else if( type->size && data )
    {
        ::operator delete( data );
    }
}

void set_projection_ellipsoid( projection *prj, ellipsoid *el )
{
    if( prj && prj->type && prj->type->bind_ellipsoid && el )
    {
        (*prj->type->bind_ellipsoid)( prj->data, el );
    }
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

