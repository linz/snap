#include "snapconfig.h"
/* Management of a list of coordinate types */

/*
   $Log: crdsys_prj.c,v $
   Revision 1.1  1995/12/22 16:43:40  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "coordsys/crdsys_prj.h"
#include "util/chkalloc.h"
#include "util/dstring.h"

typedef struct prj_list_s
{
    struct prj_list_s *next;
    projection_type prj_type;
} prj_list;

static prj_list *prj_types = NULL;


projection_type *find_projection_type( const char *code )
{
    prj_list *pt;
    for( pt = prj_types; pt; pt = pt->next )
    {
        if( _stricmp( code, pt->prj_type.code ) == 0 ) return &pt->prj_type;
    }
    return NULL;
}

projection_type *register_projection_type( projection_type *prj )
{
    projection_type *pt;
    prj_list *pl;
    pt = find_projection_type( prj->code );
    if( pt ) return pt;
    pl = (prj_list *) check_malloc( sizeof( prj_list ) );
    if( !pl ) return NULL;
    pl->next = prj_types;
    prj_types = pl;
    memcpy( &pl->prj_type, prj, sizeof(projection_type) );
    pl->prj_type.code = copy_string( prj->code );
    pl->prj_type.name = copy_string( prj->name );
    return &pl->prj_type;
}



