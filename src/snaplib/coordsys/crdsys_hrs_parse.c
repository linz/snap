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


vdatum *parse_vdatum_def ( input_string_def *is, 
                                  ref_frame *(*getrf)(const char *code, int loadref ),
                                  vdatum *(*gethrs)(const char *code, int loadref )
                                  )
{
    char hrscode[CRDSYS_CODE_LEN+1];
    char hrsname[CRDSYS_NAME_LEN+1];
    char basecode[CRDSYS_CODE_LEN+1];
    char stdcode[CRDSYS_CODE_LEN+1];
    char geoidname[MAX_FILENAME_LEN+1];
    const char *geoidfile;
    double offset;
    const char *bad;
    int isgeoid;
    int isgrid;
    vdatum *hrs = 0;
    ref_frame *baserf = 0;
    vdatum *basehrs = 0;
    vdatum_func *hrf = 0;

    char *stdfrm = 0;
    double sf, txyz[3], rxyz[3];
    double dsf, dtxyz[3], drxyz[3];
    double refdate=0.0;
    int iersunits=0;
    int sts;
    long loc;
    int reported;

    bad = NULL;
    sts = OK;
    reported = 0;
    isgeoid = 0;
    isgrid = 0;

    READ_STRING( "code",hrscode,CRDSYS_CODE_LEN );
    READ_STRING( "name",hrsname,CRDSYS_NAME_LEN );
    READ_STRING( "base height surface code",basecode,CRDSYS_CODE_LEN );
    if( test_next_string_field(is,"geoid") )
    {
        isgeoid=1;
        isgrid=1;
        READ_STRING("geoid name",geoidname,MAX_FILENAME_LEN);
    }
    else if( test_next_string_field(is,"grid") )
    {
        isgrid=1;
        READ_STRING("offset grid name",geoidname,MAX_FILENAME_LEN);
    }
    else
    {
        /* Skip optional string "offset" - as originally implemented with 
         * offset assumed and just a float value 
         */
        test_next_string_field(is, "offset");
        READ_DOUBLE("offset",&offset);
    }

    if( isgrid )
    {
        geoidfile = find_relative_file( is->sourcename, geoidname, ".grd" );
        if( ! geoidfile )
        {
            char errmess[255];
            sprintf(errmess,"Cannot locate geoid file %.120s for vertical datum %.20s",
                    geoidname,hrscode);
            report_string_error( is, INVALID_DATA, errmess );
            sts = INVALID_DATA;
        }
        else
        {
            hrf=create_grid_vdatum_func( geoidfile, isgeoid );
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
                char errmess[255];
                sprintf(errmess,"Cannot load reference datum %.20s for vertical datum %.20s",
                        basecode,hrscode);
                report_string_error( is, INVALID_DATA, errmess );
                sts = INVALID_DATA;
            }
        }
        else if( gethrs )
        {
            basehrs=gethrs(basecode,1);
            if( ! basehrs )
            {
                char errmess[255];
                sprintf(errmess,"Cannot load underlying vertical datum %.20s for %.20s",
                        basecode,hrscode);
                report_string_error( is, INVALID_DATA, errmess );
                sts = INVALID_DATA;
            }
            else
            {
                vdatum *base=basehrs;
                while( base )
                {
                    if( _stricmp(base->code,hrscode) == 0 )
                    {
                        char errmess[80+CRDSYS_CODE_LEN];
                        strcpy( errmess, "Vertical datum ");
                        strcat( errmess, hrscode );
                        strcat( errmess, " has a cyclic dependency");
                        report_string_error( is, INVALID_DATA, errmess );
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
        hrs=create_vdatum( hrscode, hrsname, basehrs, baserf, hrf );
        if( ! hrs )
        {
            char errmess[80+CRDSYS_CODE_LEN];
            strcpy( errmess, "Cannot create vertical datum ");
            strcat( errmess, hrscode );
            report_string_error( is, INVALID_DATA, errmess );
            sts=INVALID_DATA;
        }
    }

    if( ! hrs )
    {
        if( basehrs ) delete_vdatum( basehrs );
        if( baserf ) delete_ref_frame( baserf );
        if( hrf ) delete_vdatum_func( hrf );
    }

    return hrs;
}

