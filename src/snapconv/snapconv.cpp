#include "snapconfig.h"
/*
   $Log: snapconv.c,v $
   Revision 1.6  2004/02/02 03:27:28  ccrook
   Removed now redundant -g option, fixed so that grid based reference frame
   transformations work.

   Revision 1.5  2000/04/03 22:47:02  ccrook
   Fixed snapconv to correctly handle grid files in terms of geodetic coordinates.

   Revision 1.4  2000/03/30 03:02:00  ccrook
   Added -q option to run more or less silently.

   Revision 1.3  1998/06/15 02:15:36  ccrook
   Fixed printing of version number (as string instead of number)

   Revision 1.2  1998/05/21 03:57:57  ccrook
   Added support for a grid based distortion model to be applied.

   Revision 1.1  1996/01/03 22:54:19  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "util/errdef.h"
#include "network/network.h"
#include "util/fileutil.h"
#include "util/dstring.h"
#include "util/chkalloc.h"
#include "snap/filenames.h"
#include "util/getversion.h"

enum
{
    SET_HGTTYPE_DEFAULT=0,
    SET_HGTTYPE_ELLIPSOIDAL,
    SET_HGTTYPE_ORTHOMETRIC
};

enum
{
    SET_DEGOPT_DEFAULT=0,
    SET_DEGOPT_DMS,
    SET_DEGOPT_DECIMAL
};

int main( int argc, char *argv[] )
{
    coordsys *cs;
    network *net;
    char msg[256];
    char quiet = 0;
    double epoch = 0.0;
    char *epochstr=0;
    char *netcrdsys=0;
    char syntax_error=0;
    int hgtfixopt=NW_HGTFIXEDOPT_ELLIPSOIDAL;
    int degopt = SET_DEGOPT_DEFAULT;
    int hgttype=SET_HGTTYPE_DEFAULT;

    CONFIGURE_RUNTIME();

    /* Crude fix to allow suppression of output */

    while( argc > 1 && argv[1][0] == '-' )
    {
        switch( argv[1][1] )
        {
        case 'Q':
        case 'q': quiet = 1; break;
        case 'D':
        case 'd': 
                  if( degopt != SET_DEGOPT_DEFAULT ) syntax_error=1;
                  degopt = SET_DEGOPT_DECIMAL;
                  break;
        case 'H':
        case 'h': 
                  if( degopt != SET_DEGOPT_DEFAULT ) syntax_error=1;
                  degopt = SET_DEGOPT_DMS;
                  break;
        case 'E':
        case 'e': 
                  if( hgttype != SET_HGTTYPE_DEFAULT ) syntax_error=1;
                  hgttype=SET_HGTTYPE_ELLIPSOIDAL;
                  break;
        case 'O':
        case 'o': 
                  if( hgttype != SET_HGTTYPE_DEFAULT ) syntax_error=1;
                  hgttype=SET_HGTTYPE_ORTHOMETRIC;
                  break;
        case 'P':
        case 'p': 
                  hgtfixopt=NW_HGTFIXEDOPT_ORTHOMETRIC;
                  break;
        case 'Y':
        case 'y':
            if( argv[1][2] ) { epochstr=argv[1]+2; }
            else if( argc >= 3 )
            {
                epochstr=argv[2];
                argc--;
                argv++;
            }
            else
            {
                printf("Missing value for conversion epoch (-Y)\n");
                return 1;
            }
            if( ! parse_crdsys_epoch( epochstr, &epoch ))
            {
                printf("Invalid value for conversion epoch (-Y %s)\n",epochstr);
                return 1;
            }
            break;
        default:
            argc = 0;
        }
        quiet = 1;
        argv++;
        argc--;
    }

    if( ! quiet )
    {
        printf("\n%s %s: Converts a SNAP coord file to a different coordinate system\n\n",
               PROGRAM_NAME, PROGRAM_VERSION);
    }

    if( syntax_error )
    {
        printf("\nInvalid command options specified to snapconv\n");
    }

    if( argc != 4 || syntax_error )
    {
        printf ("snapconv: Missing or invalid parameters\n\n");

        printf("Syntax: snapconv [-d][-h][-q][-y yyyymmdd] input_coord_file new_coordsys_code output_coord_file\n\n");
        printf("Options are:\n");
        printf("  -d   Output angles in decimal degrees\n");
        printf("  -h   Output angles in degrees/minutes/seconds\n");
        printf("  -e   Output heights in ellipsoidal\n");
        printf("  -o   Output heights in orthometric\n");
        printf("  -p   Preserve orthometric heights where geoid changing\n");
        printf("  -y yyyymmdd Specify a conversion date for conversions requiring it\n");
        printf("  -q   Operate quietly\n");
        printf("\nThe -d and -h options cannot both be used.\n");
        printf("\nThe -e and -o options cannot both be used.\n");

        return 1;
    }

    install_default_crdsys_file();

    cs = load_coordsys( argv[2] );
    if( !cs )
    {
        printf("\nUnable to find a definition of coordinate system %s\n",argv[2]);
        return 2;
    }

    net = new_network();
    if( read_network( net, argv[1], 0 ) != OK )
    {
        printf("Cannot open coordinate file %s\n",argv[1]);
        return 2;
    }

    netcrdsys=copy_string(net->crdsysdef);

    msg[0]=0;
    if( set_network_coordsys( net, cs, epoch, hgtfixopt, msg, 256 ) != OK )
    {
        printf("Unable to convert network coordinate system to %s\n%s\n",argv[2],msg);
        return 2;
    }

    msg[0]=0;
    if( netcrdsys )
    {
        sprintf(msg,"Converted from %.32s",netcrdsys);
        check_free( netcrdsys );
    }
    if( epochstr && msg[0] )
    {
        sprintf(msg+strlen(msg)," at epoch %.32s",epochstr);
    }

    if( degopt != SET_DEGOPT_DEFAULT )
    {
        net->options &= ~NW_DEC_DEGREES;
        net->options |= (degopt == SET_DEGOPT_DECIMAL) ? NW_DEC_DEGREES : 0;
    }

    if( hgttype == SET_HGTTYPE_ELLIPSOIDAL )
    {
        set_network_height_coord_ellipsoidal( net );
    }
    else if( hgttype == SET_HGTTYPE_ORTHOMETRIC )
    {
        set_network_height_coord_orthometric( net );
    }

    if( write_network( net, argv[3], msg, 0, 0 ) != OK )
    {
        return 2;
    }

    return 0;
}


