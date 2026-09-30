#include "snapconfig.h"


/*
   $Log: crdsysr3.c,v $
   Revision 1.3  2004/01/27 21:16:34  ccrook
   Changed parsing of reference frames to always require translation parameters
   unless the reference frame code is 'NONE'.

   Revision 1.2  2003/11/28 01:59:25  ccrook
   Updated to be able to use grid transformation for datum changes (ie to
   support official NZGD49-NZGD2000 conversion)

   Revision 1.1  1996/01/04 00:05:41  CHRIS
   Initial revision

   Revision 1.1  1995/12/22 16:48:39  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>

#include "util/errdef.h"
#include "util/fileutil.h"
#include "coordsys/coordsys.h"
#include "coordsys/crdsys_hrs_func.h"
#include "coordsys/crdsys_parse_field.h"

vdatum *parse_vdatum_def ( input_string_def &is,
                                  ref_frame *(*getrf)(std::string_view code, int loadref ),
                                  vdatum *(*gethrs)(std::string_view code, int loadref )
                                  )
{
    std::string hrscode;
    std::string hrsname;
    std::string basecode;
    std::string geoidname;
    std::optional<std::string> geoidfile;
    double offset;
    int isgeoid;
    int isgrid;
    vdatum *hrs = 0;
    ref_frame *baserf = 0;
    vdatum *basehrs = 0;
    vdatum_func *hrf = 0;

    int sts;

    sts = OK;
    isgeoid = 0;
    isgrid = 0;

    sts = read_crdsys_string( is.scanner, sts, hrscode, CRDSYS_CODE_LEN );
    sts = read_crdsys_string( is.scanner, sts, hrsname, CRDSYS_NAME_LEN );
    sts = read_crdsys_string( is.scanner, sts, basecode, CRDSYS_CODE_LEN );
    if( test_next_string_field(is.scanner,"geoid") )
    {
        isgeoid=1;
        isgrid=1;
        sts = read_crdsys_string( is.scanner, sts, geoidname, MAX_FILENAME_LEN );
    }
    else if( test_next_string_field(is.scanner,"grid") )
    {
        isgrid=1;
        sts = read_crdsys_string( is.scanner, sts, geoidname, MAX_FILENAME_LEN );
    }
    else
    {
        /* Skip optional string "offset" - as originally implemented with
         * offset assumed and just a float value
         */
        test_next_string_field(is.scanner, "offset");
        sts = read_crdsys_double( is.scanner, sts, offset );
    }

    if( isgrid )
    {
        geoidfile = find_relative_file( is.sourcename, geoidname, ".grd" );
        if( ! geoidfile )
        {
            const std::string errmess = "Cannot locate geoid file " + geoidname +
                                        " for vertical datum " + hrscode;
            report_string_error( is, INVALID_DATA, errmess.c_str() );
            sts = INVALID_DATA;
        }
        else
        {
            hrf=create_grid_vdatum_func( *geoidfile, isgeoid );
        }
    }
    else
    {
        hrf=create_offset_vdatum_func( offset );
    }

    if( sts == OK )
    {
        if( isgeoid && getrf )
        {
            baserf=getrf(basecode,1);
            if( ! baserf )
            {
                const std::string errmess = "Cannot load reference datum " + basecode +
                                            " for vertical datum " + hrscode;
                report_string_error( is, INVALID_DATA, errmess.c_str() );
                sts = INVALID_DATA;
            }
        }
        else if( gethrs )
        {
            basehrs=gethrs(basecode,1);
            if( ! basehrs )
            {
                const std::string errmess = "Cannot load underlying vertical datum " + basecode +
                                            " for " + hrscode;
                report_string_error( is, INVALID_DATA, errmess.c_str() );
                sts = INVALID_DATA;
            }
            else
            {
                vdatum *base=basehrs;
                while( base )
                {
                    if( compare_ignoring_case(base->code,hrscode) == 0 )
                    {
                        const std::string errmess = "Vertical datum " + hrscode + " has a cyclic dependency";
                        report_string_error( is, INVALID_DATA, errmess.c_str() );
                        sts = INVALID_DATA;
                        break;
                    }
                    base=base->basehrs;
                }
            }
        }
    }

    if( sts == OK )
    {
        std::string source = "file:" + is.sourcename;
        hrs = baserf ? new vdatum( hrscode, hrsname, baserf, hrf, source )
                     : new vdatum( hrscode, hrsname, basehrs, hrf, source );
    }

    if( ! hrs )
    {
        delete basehrs;
        delete baserf;
        delete hrf;
    }

    return hrs;
}

