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

#define READ_STRING( name, str, len ) \
	 if( sts == OK ) { \
        bad = name; \
        sts = next_string_field( is, str, len ); \
        }

#define READ_DOUBLE( name, pdouble ) \
     if( sts == OK ) { \
         bad = name; \
         sts = double_from_string( is, pdouble ); \
         }

ref_frame  *parse_ref_frame_def ( input_string_def *is,
                                  ellipsoid *(*getel)(const char *code ),
                                  ref_frame *(*getrf)(const char *code, int loadref ),
                                  int embedded, int loadref )
{
    char refcode[CRDSYS_CODE_LEN+1];
    char refname[CRDSYS_NAME_LEN+1];
    char elcode[CRDSYS_CODE_LEN+1];
    char stdcode[CRDSYS_CODE_LEN+1];
    char *stdfrm = 0;
    double sf, txyz[3], rxyz[3];
    double dsf, dtxyz[3], drxyz[3];
    double refdate=0.0;
    int iersunits=0;
    ellipsoid *el = 0;
    ref_frame *rf = 0;
    ref_frame_func *rff = 0;
    ref_deformation *rdf = 0;
    int sts;
    const char *bad;
    long loc;
    int reported;

    bad = NULL;
    sts = OK;
    reported = 0;

    READ_STRING( "code",refcode,CRDSYS_CODE_LEN );
    READ_STRING( "name",refname,CRDSYS_NAME_LEN );
    loc = get_string_loc( is );
    READ_STRING( "ellipsoid code",elcode,CRDSYS_CODE_LEN );

    if( sts == OK && _stricmp(elcode, "ELLIPSOID") != 0 )
    {
        set_string_loc( is, loc );
        el = parse_ellipsoid_def( is, 1 );
        if( !el ) return NULL;
    }
    else
    {
        READ_STRING( "ellipsoid_code",elcode,CRDSYS_CODE_LEN );
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
        READ_STRING( "base frame code", stdcode, CRDSYS_CODE_LEN );
        if( sts == MISSING_DATA )
        {
            sts = OK;
            strcpy( stdcode, "NONE");
        }
    }

    if( sts == OK )
    {
        if( _stricmp( stdcode, "NONE" ) == 0 )
        {
            stdfrm = NULL;
        }
        else
        {
            stdfrm = stdcode;

            iersunits=test_next_string_field( is, "IERS");

            READ_DOUBLE( "x translation", &txyz[0] );
            READ_DOUBLE( "y translation", &txyz[1] );
            READ_DOUBLE( "z translation", &txyz[2] );

            READ_DOUBLE( "x rotation", &rxyz[0] );
            READ_DOUBLE( "y rotation", &rxyz[1] );
            READ_DOUBLE( "z rotation", &rxyz[2] );

            READ_DOUBLE( "scale factor", &sf );

            if( sts == OK && test_next_string_field( is, "RATES" ))
            {
                READ_DOUBLE( "reference date", &refdate );
                READ_DOUBLE( "x translation rate", &dtxyz[0] );
                READ_DOUBLE( "y translation rate", &dtxyz[1] );
                READ_DOUBLE( "z translation rate", &dtxyz[2] );

                READ_DOUBLE( "x rotation rate", &drxyz[0] );
                READ_DOUBLE( "y rotation rate", &drxyz[1] );
                READ_DOUBLE( "z rotation rate", &drxyz[2] );

                READ_DOUBLE( "scale factor rate", &dsf );
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
         * the tranformation parameters must all be 0
         */
        if( sts == OK && _stricmp(stdcode,refcode) == 0 )
        {
            int i;
            int ok = 1;

            stdfrm=0;

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
                char errmsg[80+CRDSYS_CODE_LEN];
                strcpy( errmsg, "Reference frame ");
                strcat( errmsg, refcode );
                strcat( errmsg, " cannot have a non-null transformation to itself");
                report_string_error( is, INVALID_DATA, errmsg );
                sts = INVALID_DATA;
                reported = 1;
            }
        }
    }
    if( sts == OK )
    {
		bad=0;
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
                char errmsg[80];
                strcpy( errmsg, "Cannot load ellipsoid ");
                strcat( errmsg, elcode );
                report_string_error( is, INVALID_DATA, errmsg );
                sts = INVALID_DATA;
                reported = 1;
            }
        }
    }
    if( sts == OK && ! embedded )
    {
        char test[32];
        sts = next_string_field( is, test, 32-1 ) == NO_MORE_DATA ? OK : TOO_MUCH_DATA;
        if( sts != OK )
        {
            char errmsg[100+CRDSYS_CODE_LEN];
            sprintf(errmsg,"Extraneous data \"%s\" in definition of ref frame \"%s\"",
                    test,refcode);
            report_string_error(is,sts,errmsg);
            reported = 1;
        }
    }

    if( sts == OK )
    {
        rf = create_ref_frame(  refcode, refname, el, stdfrm, txyz, rxyz, sf,
                                refdate, dtxyz, drxyz, dsf );
        rf->func = rff;
        rf->def = rdf;
        rf->use_iersunits=iersunits;
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
                if( _stricmp(check->code,base->refcode) == 0 )
                {
                    char errmsg[80+CRDSYS_CODE_LEN];
                    strcpy( errmsg, "Reference frame ");
                    strcat( errmsg, check->code );
                    strcat( errmsg, " has a cyclic base reference frame dependency");
                    report_string_error( is, INVALID_DATA, errmsg );
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
             * Invalid defintions of the base system are not reported correctly,
             * the reference frame is just ignored.
             */
            newbase=getrf(base->refcode,0);
            if( ! newbase ) break;
            base->refrf=newbase;
            base=newbase;
        }
    }

    if( sts != OK && bad )
    {
        char errmess[80]= {0};
        if( sts == MISSING_DATA )
        {
            strcpy( errmess, bad);
            strcat( errmess, " is missing" );
        }
        else if (! reported )
        {
            strcpy( errmess, "Invalid value for " );
            strcat( errmess, bad );
        }
        if( errmess[0] ) report_string_error( is, sts, errmess );
    }

    if( ! rf )
    {
        if( el ) delete_ellipsoid( el );
        if( rff ) delete_ref_frame_func( rff );
        if( rdf ) delete_ref_deformation( rdf );
    }
    else if( sts != OK )
    {
        delete_ref_frame( rf );
        rf=0;
    }
    return rf;
}

