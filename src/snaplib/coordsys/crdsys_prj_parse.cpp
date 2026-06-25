#include "snapconfig.h"
/*
   $Log: crdsysp3.c,v $
   Revision 1.1  1995/12/22 16:40:53  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>

#include "util/errdef.h"
#include "coordsys/crdsys_prj.h"

projection *parse_projection_def( input_string_def *is )
{
    char typecode[CRDSYS_CODE_LEN+1];
    char errmess[128];
    int sts;
    projection_type *pt;
    projection *prj;

    prj = NULL;

    if( next_string_field( is, typecode, CRDSYS_CODE_LEN ) != OK )
    {
        report_string_error( is, MISSING_DATA, "Projection code missing" );
        return prj;
    }

    pt = find_projection_type( typecode );
    if( !pt )
    {
        sprintf(errmess,"Invalid projection code %s",typecode);
        report_string_error( is, INVALID_DATA, errmess );
        return NULL;
    }

    prj = create_projection( pt );
    if( !prj ) return NULL;   /* Not a string error, so don't report here */

    sts = read_param_list( is, pt->params, pt->nparams, prj->data );

    if( sts != OK )
    {
        delete_projection( prj );
        prj = NULL;
    }

    return prj;
}


