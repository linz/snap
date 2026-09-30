#include "snapconfig.h"
/*
   $Log: crdsysp3.c,v $
   Revision 1.1  1995/12/22 16:40:53  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>

#include "util/errdef.h"
#include "util/fieldscanner.hpp"
#include "coordsys/crdsys_prj.h"

projection *parse_projection_def( input_string_def &is )
{
    std::string typecode;
    int sts;
    projection_type *pt;
    projection *prj;

    prj = nullptr;

    if( read_string_field( is.scanner, typecode, CRDSYS_CODE_LEN ) != FieldResult::Ok )
    {
        report_string_error( is, MISSING_DATA, "Projection code missing" );
        return prj;
    }

    pt = find_projection_type( typecode );
    if( !pt )
    {
        const std::string errmess = "Invalid projection code " + typecode;
        report_string_error( is, INVALID_DATA, errmess.c_str() );
        return nullptr;
    }

    prj = create_projection( pt );
    if( !prj ) return nullptr;   /* Not a string error, so don't report here */

    sts = read_param_list( is, pt->params, pt->nparams, prj->data );

    if( sts != OK )
    {
        delete_projection( prj );
        prj = nullptr;
    }

    return prj;
}


