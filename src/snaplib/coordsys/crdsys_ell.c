#include "snapconfig.h"
/* crdsysel.c:  Ellipsoid management for coordinate system routines */

/*
   $Log: crdsyse0.c,v $
   Revision 1.1  1995/12/22 16:32:47  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include "coordsys/coordsys.h"
#include "util/chkalloc.h"
#include "util/dstring.h"


static char rcsid[]="$Id: crdsyse0.c,v 1.1 1995/12/22 16:32:47 CHRIS Exp $";

void init_ellipsoid( ellipsoid *el, double a, double rf )
{
    el->a = a;
    el->rf = rf;
    el->b = rf==0.0 ? a : a-a/rf;
    el->a2 = a*a;
    el->b2 = el->b * el->b;
    el->a2b2 = el->a2 - el->b2;
}

ellipsoid *create_ellipsoid( const char *code, const char *name, double a, double rf )
{
    ellipsoid *el;
    el = (ellipsoid *) check_malloc( sizeof(ellipsoid) );
    el->code = copy_string( code );
    _strupr( el->code );
    el->name = copy_string( name );
    init_ellipsoid( el, a, rf );
    return el;
}


ellipsoid *copy_ellipsoid( ellipsoid *el )
{
    if( el == NULL ) return NULL;
    return create_ellipsoid( el->code, el->name, el->a, el->rf );
}

void delete_ellipsoid( ellipsoid *el )
{
    if( !el ) return;
    check_free( el->code );
    check_free( el->name );
    check_free( el );
}

int identical_ellipsoids( ellipsoid *el1, ellipsoid *el2 )
{
    if( el1->a == el2->a && el1->rf == el2->rf ) return 1;
    return 0;
}

