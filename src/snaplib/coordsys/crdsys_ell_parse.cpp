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
#include "coordsys/crdsys_parse_field.h"

namespace
{
/// read_param_list's fill target - a and rf are read before ellipsoid's
/// code/name are known, and ellipsoid itself has no default constructor
/// (it's always fully-formed once it exists), so this plain struct holds
/// them until the real ellipsoid can be constructed.
struct ellipsoid_axes
{
    double a;
    double rf;
};
}

static param_def ell_params[] =
{
    {
        "Semi-major axis","a",OFFSET_OF(a,ellipsoid_axes),
        double_from_string, print_double3, print_double3
    },
    {
        "Reciprocal flattening","rf",OFFSET_OF(rf,ellipsoid_axes),
        double_from_string, print_double6, print_double6
    }
};

ellipsoid *parse_ellipsoid_def( input_string_def &is, int embedded )
{
    std::string elcode;
    std::string elname;
    ellipsoid_axes axes;
    std::string_view bad;
    int sts = OK;

    sts = read_crdsys_string( is.scanner, sts, elcode, CRDSYS_CODE_LEN, "code", bad );
    sts = read_crdsys_string( is.scanner, sts, elname, CRDSYS_NAME_LEN, "name", bad );
    if( sts != OK )
    {
        const std::string errmess = ( sts == MISSING_DATA ? "Missing ellipsoid " : "Invalid ellipsoid " ) +
                                    std::string( bad );
        report_string_error( is, sts, errmess.c_str() );
        return nullptr;
    }
    sts =  read_param_list( is, ell_params, COUNT_OF(ell_params), &axes );

    if( sts == OK && ! embedded )
    {
        std::string test;
        sts = read_string_field( is.scanner, test, 31 ) == FieldResult::NoMoreData ? OK : TOO_MUCH_DATA;
        if( sts != OK )
        {
            const std::string errmsg = "Extraneous data \"" + test +
                                       "\" in definition of ellipsoid \"" + elcode + "\"";
            report_string_error( is, sts, errmsg.c_str() );
        }
    }

    if( sts == OK )
    {
        return new ellipsoid( elcode, elname, axes.a, axes.rf );
    }
    return nullptr;
}
