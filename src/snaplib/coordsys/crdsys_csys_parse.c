#include "snapconfig.h"
/*
   $Log: crdsysc3.c,v $
   Revision 1.2  2003/05/26 22:53:58  ccrook
   Fixed bug with trying to delete unitiallised data when the coordsys file isn't formatted
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

static char rcsid[]="$Id: crdsysc3.c,v 1.2 2003/05/26 22:53:58 ccrook Exp $";

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

coordsys *parse_coordsys_def  ( input_string_def *is,
                                ref_frame *(*getrf)(const char *code, int loadref ))
{
    char cscode[CRDSYS_CODE_LEN+1];
    char csname[CRDSYS_NAME_LEN+1];
    char rfcode[CRDSYS_CODE_LEN+1];
    char typecode[CRDSYS_CODE_LEN+1];
    int cstype = CSTP_CARTESIAN;
    ref_frame *rf = NULL;
    projection *prj = NULL;
    coordsys *cs = NULL;
    char got_range;
    double range[4];
    char *bad = "";
    int sts = OK;
    long loc;

    READ_STRING( "Coordinate system code",cscode,CRDSYS_CODE_LEN);
    READ_STRING( "Coordinate system name",csname,CRDSYS_NAME_LEN);
    loc = get_string_loc( is );
    READ_STRING( "Reference frame code", rfcode, CRDSYS_CODE_LEN);

    if( sts == OK )
    {
        if( _stricmp( rfcode, "REF_FRAME" ) != 0 )
        {
            set_string_loc( is , loc );
            rf = parse_ref_frame_def( is, 0, 0, 1, 1 );
            if( !rf ) return NULL;
        }
        else
        {
            READ_STRING( "Reference frame code",rfcode,CRDSYS_CODE_LEN);
            rf = NULL;
        }
    }

    READ_STRING( "Coordinate system type",typecode,CRDSYS_CODE_LEN);
    if( sts == OK )
    {
        if( _stricmp( typecode, "GEOCENTRIC" ) == 0 )
        {
            cstype = CSTP_CARTESIAN;
        }
        else if( _stricmp( typecode, "GEODETIC" ) == 0 )
        {
            cstype = CSTP_GEODETIC;
        }
        else if( _stricmp( typecode, "PROJECTION" ) == 0 )
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
            if( rf ) delete_ref_frame( rf );
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
        loc = get_string_loc( is );
        READ_STRING( "", typecode, CRDSYS_CODE_LEN );
        if( sts != OK || _stricmp(typecode,"RANGE") != 0 )
        {
            set_string_loc( is, loc );
            sts = OK;
        }
        else
        {
            READ_DOUBLE( "valid range", &range[0]);
            READ_DOUBLE( "valid range", &range[1]);
            READ_DOUBLE( "valid range", &range[2]);
            READ_DOUBLE( "valid range", &range[3]);
            got_range = sts == OK;
        }
    }

    if( sts == OK )
    {
        char test[32];
        sts = next_string_field( is, test, 32-1 ) == NO_MORE_DATA ? OK : TOO_MUCH_DATA;
        if( sts != OK )
        {
            char errmsg[100+CRDSYS_CODE_LEN];
            sprintf(errmsg,"Extraneous data \"%s\" in definition of crdsys \"%s\"",
                    test,cscode);
            report_string_error(is,sts,errmsg);
        }
    }

    if( sts == OK && !rf )
    {
        if( getrf ) rf = (*getrf)(rfcode,1);
        if( !rf )
        {
            char errmess[80];
            strcpy(errmess,"Cannot load reference frame ");
            strcat(errmess,rfcode);
            report_string_error(is,INVALID_DATA,errmess);
            sts = MISSING_DATA;
        }
    }
    else if( sts != OK )
    {
        char errmess[80];
        if( sts == MISSING_DATA )
        {
            strcpy( errmess, bad);
            strcat( errmess, " is missing" );
        }
        else
        {
            strcpy( errmess, "Invalid value for " );
            strcat( errmess, bad );
        }
        report_string_error( is, sts, errmess );
    }

    if( sts != OK )
    {
        if( prj ) delete_projection( prj );
        if( rf ) delete_ref_frame( rf );
        return NULL;
    }

    cs = create_coordsys( cscode, csname, cstype, rf, prj );
    if( !cs ) return NULL;
    if( got_range ) define_coordsys_range( cs, range[0], range[1], range[2], range[3] );

    return cs;
}

