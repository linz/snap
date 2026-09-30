#include "snapconfig.h"
/*
   $Log: crdsysc3.c,v $
   Revision 1.2  2003/05/26 22:53:58  ccrook
   Fixed bug with trying to delete uninitialised data when the coordsys file isn't formatted
   correctly

   Revision 1.1  1996/01/03 23:37:17  CHRIS
   Initial revision

   Revision 1.1  1995/12/22 16:26:40  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>

#include "util/errdef.h"
#include "coordsys/coordsys.h"
#include "coordsys/crdsys_parse_field.h"

coordsys *parse_coordsys_def  ( input_string_def &is,
                                ref_frame *(*getrf)(std::string_view code, int loadref ))
{
    std::string cscode;
    std::string csname;
    std::string rfcode;
    std::string typecode;
    int cstype = CSTP_CARTESIAN;
    ref_frame *rf = NULL;
    projection *prj = NULL;
    coordsys *cs = NULL;
    char got_range;
    double range[4];
    std::string_view bad;
    int sts = OK;

    sts = read_crdsys_string( is.scanner, sts, cscode, CRDSYS_CODE_LEN, "Coordinate system code", bad );
    sts = read_crdsys_string( is.scanner, sts, csname, CRDSYS_NAME_LEN, "Coordinate system name", bad );
    auto savedBeforeRfCode = is.scanner.remainder();
    sts = read_crdsys_string( is.scanner, sts, rfcode, CRDSYS_CODE_LEN, "Reference frame code", bad );

    if( sts == OK )
    {
        if( compare_ignoring_case( rfcode, "REF_FRAME" ) != 0 )
        {
            is.scanner = FieldScanner(savedBeforeRfCode);
            rf = parse_ref_frame_def( is, 0, 0, 1, 1 );
            if( !rf ) return NULL;
        }
        else
        {
            sts = read_crdsys_string( is.scanner, sts, rfcode, CRDSYS_CODE_LEN, "Reference frame code", bad );
            rf = NULL;
        }
    }

    sts = read_crdsys_string( is.scanner, sts, typecode, CRDSYS_CODE_LEN, "Coordinate system type", bad );
    if( sts == OK )
    {
        if( compare_ignoring_case( typecode, "GEOCENTRIC" ) == 0 )
        {
            cstype = CSTP_CARTESIAN;
        }
        else if( compare_ignoring_case( typecode, "GEODETIC" ) == 0 )
        {
            cstype = CSTP_GEODETIC;
        }
        else if( compare_ignoring_case( typecode, "PROJECTION" ) == 0 )
        {
            cstype = CSTP_PROJECTION;
        }
        else
        {
            sts = INVALID_DATA;
        }
    }

    if( sts == OK && cstype == CSTP_PROJECTION )
    {
        prj = parse_projection_def( is );
        if( !prj )
        {
            delete rf;
            return NULL;
        }
    }
    else
    {
        prj = NULL;
    }

    got_range = 0;
    if( sts == OK )
    {
        auto savedBeforeTypecode = is.scanner.remainder();
        sts = read_crdsys_string( is.scanner, sts, typecode, CRDSYS_CODE_LEN, "", bad );
        if( sts != OK || compare_ignoring_case(typecode,"RANGE") != 0 )
        {
            is.scanner = FieldScanner(savedBeforeTypecode);
            sts = OK;
        }
        else
        {
            sts = read_crdsys_double( is.scanner, sts, range[0], "valid range", bad );
            sts = read_crdsys_double( is.scanner, sts, range[1], "valid range", bad );
            sts = read_crdsys_double( is.scanner, sts, range[2], "valid range", bad );
            sts = read_crdsys_double( is.scanner, sts, range[3], "valid range", bad );
            got_range = sts == OK;
        }
    }

    if( sts == OK )
    {
        std::string test;
        sts = read_string_field( is.scanner, test, 31 ) == FieldResult::NoMoreData ? OK : TOO_MUCH_DATA;
        if( sts != OK )
        {
            const std::string errmsg = "Extraneous data \"" + test +
                                       "\" in definition of crdsys \"" + cscode + "\"";
            report_string_error( is, sts, errmsg.c_str() );
        }
    }

    if( sts == OK && !rf )
    {
        if( getrf ) rf = (*getrf)(rfcode,1);
        if( !rf )
        {
            const std::string errmess = "Cannot load reference frame " + rfcode;
            report_string_error( is, INVALID_DATA, errmess.c_str() );
            sts = MISSING_DATA;
        }
    }
    else if( sts != OK )
    {
        const std::string errmess = sts == MISSING_DATA ? std::string( bad ) + " is missing"
                                                        : "Invalid value for " + std::string( bad );
        report_string_error( is, sts, errmess.c_str() );
    }

    if( sts != OK )
    {
        if( prj ) delete_projection( prj );
        delete rf;
        return NULL;
    }

    cs = new coordsys( cscode, csname, cstype, rf, prj, "file:" + is.sourcename );
    if( got_range ) define_coordsys_range( cs, range[0], range[1], range[2], range[3] );

    return cs;
}

