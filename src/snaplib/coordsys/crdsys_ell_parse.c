#include "snapconfig.h"
/*
   $Log: crdsyse3.c,v $
   Revision 1.1  1995/12/22 16:34:25  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>

#include "util/errdef.h"
#include "coordsys/paramdef.h"
#include "coordsys/coordsys.h"

static char rcsid[]="$Id: crdsyse3.c,v 1.1 1995/12/22 16:34:25 CHRIS Exp $";

static param_def ell_params[] =
{
    {
        "Semi-major axis","a",OFFSET_OF(a,ellipsoid),
        double_from_string, print_double3, print_double3
    },
    {
        "Reciprocal flattening","rf",OFFSET_OF(rf,ellipsoid),
        double_from_string, print_double6, print_double6
    }
};

ellipsoid *parse_ellipsoid_def( input_string_def *is, int embedded )
{
    char elcode[CRDSYS_CODE_LEN + 1];
    char elname[CRDSYS_NAME_LEN + 1];
    ellipsoid el;
    char *bad;
    int sts;

    bad = "code";
    sts = next_string_field( is, elcode, CRDSYS_CODE_LEN );
    if( sts == OK )
    {
        bad = "name";
        sts = next_string_field( is, elname, CRDSYS_NAME_LEN );
    }
    if( sts != OK )
    {
        char errmess[40];
        if( sts == MISSING_DATA )
        {
            sprintf( errmess,"Missing ellipsoid %s",bad);
        }
        else
        {
            sprintf( errmess,"Invalid ellipsoid %s",bad);
        }
        report_string_error( is, sts, errmess );
        return NULL;
    }
    sts =  read_param_list( is, ell_params, COUNT_OF(ell_params), &el );

    if( sts == OK && ! embedded )
    {
        char test[32];
        sts = next_string_field( is, test, 32-1 ) == NO_MORE_DATA ? OK : TOO_MUCH_DATA;
        if( sts != OK )
        {
            char errmsg[100+CRDSYS_CODE_LEN];
            sprintf(errmsg,"Extraneous data \"%s\" in definition of ellipsoid \"%s\"",
                    test,elcode);
            report_string_error(is,sts,errmsg);
        }
    }

    if( sts == OK )
    {
        return create_ellipsoid( elcode, elname, el.a, el.rf );
    }
    return NULL;
}
