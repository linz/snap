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
#include "coordsys/coordsys.h"
#include "coordsys/crdsys_parse_field.h"

ref_frame  *parse_ref_frame_def ( input_string_def &is,
                                  ellipsoid *(*getel)(std::string_view code ),
                                  ref_frame *(*getrf)(std::string_view code, int loadref ),
                                  int embedded, int loadref )
{
    std::string refcode;
    std::string refname;
    std::string elcode;
    std::string stdcode;
    std::optional<std::string> stdfrm;
    double sf, txyz[3], rxyz[3];
    double dsf, dtxyz[3], drxyz[3];
    double refdate=0.0;
    int iersunits=0;
    ellipsoid *el = 0;
    ref_frame *rf = 0;
    ref_frame_func *rff = 0;
    ref_deformation *rdf = 0;
    int sts;
    std::string_view bad;
    int reported;

    sts = OK;
    reported = 0;

    sts = read_crdsys_string( is.scanner, sts, refcode, CRDSYS_CODE_LEN, "code", bad );
    sts = read_crdsys_string( is.scanner, sts, refname, CRDSYS_NAME_LEN, "name", bad );
    auto loc = is.scanner.remainder();
    sts = read_crdsys_string( is.scanner, sts, elcode, CRDSYS_CODE_LEN, "ellipsoid code", bad );

    if( sts == OK && compare_ignoring_case(elcode, "ELLIPSOID") != 0 )
    {
        is.scanner = FieldScanner(loc);
        el = parse_ellipsoid_def( is, 1 );
        if( !el ) return NULL;
    }
    else
    {
        sts = read_crdsys_string( is.scanner, sts, elcode, CRDSYS_CODE_LEN, "ellipsoid_code", bad );
        el = NULL;
    }

    sf = 0.0;
    txyz[0] = txyz[1] = txyz[2] = 0.0;
    rxyz[0] = rxyz[1] = rxyz[2] = 0.0;

    dsf = 0.0;
    dtxyz[0] = dtxyz[1] = dtxyz[2] = 0.0;
    drxyz[0] = drxyz[1] = drxyz[2] = 0.0;

    rff = NULL;
    rdf = NULL;

    if( sts == OK )
    {
        sts = read_crdsys_string( is.scanner, sts, stdcode, CRDSYS_CODE_LEN, "base frame code", bad );
        if( sts == MISSING_DATA )
        {
            sts = OK;
            stdcode = "NONE";
        }
    }

    if( sts == OK )
    {
        if( compare_ignoring_case( stdcode, "NONE" ) == 0 )
        {
            stdfrm.reset();
        }
        else
        {
            int ierstsr;
            int iersrates;
            stdfrm = stdcode;

            iersunits=0;
            ierstsr=0;
            iersrates=0;
            if( test_next_string_field( is.scanner, "IERS") )
            {
                iersunits=1;
            }
            else if( test_next_string_field( is.scanner, "IERS_TSR" ) )
            {
                iersunits=1;
                ierstsr=1;
            }
            else if( test_next_string_field( is.scanner, "IERS_ETSR" ) )
            {
                iersunits=1;
                ierstsr=1;
                iersrates=1;
                sts = read_crdsys_double( is.scanner, sts, refdate, "reference date", bad );
            }

            sts = read_crdsys_double( is.scanner, sts, txyz[0], "x translation", bad );
            sts = read_crdsys_double( is.scanner, sts, txyz[1], "y translation", bad );
            sts = read_crdsys_double( is.scanner, sts, txyz[2], "z translation", bad );

            if( ierstsr ) { sts = read_crdsys_double( is.scanner, sts, sf, "scale factor", bad ); }

            sts = read_crdsys_double( is.scanner, sts, rxyz[0], "x rotation", bad );
            sts = read_crdsys_double( is.scanner, sts, rxyz[1], "y rotation", bad );
            sts = read_crdsys_double( is.scanner, sts, rxyz[2], "z rotation", bad );

            if( ! ierstsr ) { sts = read_crdsys_double( is.scanner, sts, sf, "scale factor", bad ); }

            if( sts == OK && (iersrates || test_next_string_field( is.scanner, "RATES" )))
            {
                if( ! iersrates ) { sts = read_crdsys_double( is.scanner, sts, refdate, "reference date", bad ); }
                sts = read_crdsys_double( is.scanner, sts, dtxyz[0], "x translation rate", bad );
                sts = read_crdsys_double( is.scanner, sts, dtxyz[1], "y translation rate", bad );
                sts = read_crdsys_double( is.scanner, sts, dtxyz[2], "z translation rate", bad );

                if( ierstsr ) { sts = read_crdsys_double( is.scanner, sts, dsf, "scale factor rate", bad ); }

                sts = read_crdsys_double( is.scanner, sts, drxyz[0], "x rotation rate", bad );
                sts = read_crdsys_double( is.scanner, sts, drxyz[1], "y rotation rate", bad );
                sts = read_crdsys_double( is.scanner, sts, drxyz[2], "z rotation rate", bad );

                if( ! ierstsr ) { sts = read_crdsys_double( is.scanner, sts, dsf, "scale factor rate", bad ); }

            }

            if( iersunits )
            {
                int i;
                for( i=0; i<3; i++ )
                {
                    txyz[i]*=0.001;
                    dtxyz[i]*=0.001;
                    rxyz[i]*=-0.001;
                    drxyz[i]*=-0.001;
                }
                sf*=0.001;
                dsf*=0.001;
            }
        }

        if( sts == OK )
        {
            sts = parse_ref_frame_func_def( is, &rff );
        }

        /* If the base frame code is the same as the reference frame, then
         * the transformation parameters must all be 0
         */
        if( sts == OK && compare_ignoring_case(stdcode,refcode) == 0 )
        {
            int i;
            int ok = 1;

            stdfrm.reset();

            for( i=0; i<3; i++ )
            {
                if( txyz[i] != 0.0 ) ok = 0;
                if( dtxyz[i] != 0.0 ) ok = 0;
                if( rxyz[i] != 0.0 ) ok = 0;
                if( drxyz[i] != 0.0 ) ok = 0;
            }
            if( sf != 0.0 || dsf != 0.0 ) ok=0;
            if( rff != 0 ) ok=0;
            if( ! ok )
            {
                const std::string errmsg = "Reference frame " + refcode +
                                           " cannot have a non-null transformation to itself";
                report_string_error( is, INVALID_DATA, errmsg.c_str() );
                sts = INVALID_DATA;
                reported = 1;
            }
        }
    }
    if( sts == OK )
    {
        bad = std::string_view();
        sts = parse_ref_deformation_def( is, &rdf );
    }


    if( sts == OK && !el )
    {
        if( !getel )
        {
            sts = MISSING_DATA;
            bad = "ellipsoid definition";
        }
        else
        {
            el = (*getel)(elcode);
            if( !el )
            {
                const std::string errmsg = "Cannot load ellipsoid " + elcode;
                report_string_error( is, INVALID_DATA, errmsg.c_str() );
                sts = INVALID_DATA;
                reported = 1;
            }
        }
    }
    if( sts == OK && ! embedded )
    {
        std::string test;
        sts = read_string_field( is.scanner, test, 31 ) == FieldResult::NoMoreData ? OK : TOO_MUCH_DATA;
        if( sts != OK )
        {
            const std::string errmsg = "Extraneous data \"" + test +
                                       "\" in definition of ref frame \"" + refcode + "\"";
            report_string_error( is, sts, errmsg.c_str() );
            reported = 1;
        }
    }

    if( sts == OK )
    {
        rf = new ref_frame( refcode, refname, el, stdfrm,
                             txyz, rxyz, sf, refdate, dtxyz, drxyz, dsf,
                             rff, rdf, iersunits );
    }

    /* If we are loading the base reference frame ... */
    if( sts == OK && loadref && getrf )
    {
        ref_frame *base=rf;
        while( sts==OK && base->refcode )
        {
            /* Check we are not creating a cyclic dependency */
            ref_frame *check=rf;
            ref_frame *newbase=0;
            while( check )
            {
                if( compare_ignoring_case(check->code,*base->refcode) == 0 )
                {
                    const std::string errmsg = "Reference frame " + check->code +
                                               " has a cyclic base reference frame dependency";
                    report_string_error( is, INVALID_DATA, errmsg.c_str() );
                    sts = INVALID_DATA;
                    reported = 1;
                    break;
                }
                check=check->refrf;
            }
            if( sts != OK ) break;
            /* Get the base reference frame.  If this is null, then exit.  It
             * will be NULL if there is no corresponding reference frame (ie an
             * arbitrary base system) or if the last base reference frame is
             * based on itself
             *
             * Invalid definitions of the base system are not reported correctly,
             * the reference frame is just ignored.
             */
            newbase=getrf(*base->refcode,0);
            if( ! newbase ) break;
            base->refrf=newbase;
            base=newbase;
        }
    }

    if( sts != OK && ! bad.empty() )
    {
        std::string errmess;
        if( sts == MISSING_DATA )
        {
            errmess = std::string( bad ) + " is missing";
        }
        else if (! reported )
        {
            errmess = "Invalid value for " + std::string( bad );
        }
        if( ! errmess.empty() ) report_string_error( is, sts, errmess.c_str() );
    }

    if( ! rf )
    {
        delete el;
        delete rff;
        delete rdf;
    }
    else if( sts != OK )
    {
        delete rf;
        rf=0;
    }
    return rf;
}

