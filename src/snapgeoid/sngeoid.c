#include "snapconfig.h"
/* Program snapgeoid:  Calculates geoid undulations on a SNAP coordinate file
   given a binary file of gridded geoid data */

/*
   $Log: sngeoid.c,v $
   Revision 1.2  1996/05/17 22:53:14  CHRIS
   Updated to use coordinate system defined in geoid.bin file instead
   of default WGS84.

   Revision 1.1  1996/01/03 22:56:37  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network/network.h"
#include "geoid/geoid.h"
#include "util/chkalloc.h"
#include "util/fileutil.h"
#include "util/errdef.h"
#include "util/versioninfo.h"
#include "snap/filenames.h"

static char rcsid[]="$Id: sngeoid.c,v 1.2 1996/05/17 22:53:14 CHRIS Exp $";

int main( int argc, char *argv[] )
{
    char *oldfn, *newfn;
    int gbformat;
    int nch;
    network net;
    char remove_geoid = 0;
    char calc_geoid = 0;
    char syntax_error = 0;
    char quiet = 0;
    char change_height_type = 0;
    char output_ellipsoidal = 0;
    char geoid_msg[120];
    char *geoid = NULL;
    geoid_def *gd = NULL;

    geoid_msg[0] = 0;

    while( argc > 1 && argv[1][0] == '-' )
    {
        switch( argv[1][1] )
        {
        case 'e':
        case 'E':
            if( change_height_type ) syntax_error = 1;
            change_height_type = 1;
            output_ellipsoidal = 1;
            break;

        case 'o':
        case 'O':
            if( change_height_type ) syntax_error = 1;
            change_height_type = 1;
            output_ellipsoidal = 0;
            break;

        case 'x':
        case 'X':
        case 'r':
        case 'R':
            remove_geoid = 1;
            break;

        case 'c':
        case 'C':
            calc_geoid = 1;
            break;

        case 'g':
        case 'G':
            if( argc > 2  && ! geoid )
            {
                geoid = argv[2];
                argv++;
                argc--;
            }
            else
            {
                syntax_error = 1;
            }
            break;

        case 'q':
        case 'Q': quiet = 1;
            break;
        default:
            syntax_error = 1;
            break;
        }
        argv++;
        argc--;
    }

    if( ! quiet )
    {
        printf("\n%s version %s\n\nCalculates geoid undulations on a SNAP coordinate file\n\n",
               ProgramVersion.program, ProgramVersion.version);
    }

    if( syntax_error )
    {
        printf("Error in snapgeoid command\n\n");
    }
    if( argc < 2 || syntax_error )
    {
        printf("Syntax:  snapgeoid  [options] station_file_name [new_station_file_name]\n\n");
        printf("Options can include:\n");
        printf(" -g geoid_name  selects the geoid to use\n");
        printf(" -c             calculates geoid heights even if the station file already\n");
        printf("                includes geoid information\n");
        printf(" -x             excludes geoid information from the output coordinate file\n");
        printf(" -e             sets the output heights to be ellipsoidal\n");
        printf(" -o             sets the output heights to be orthometric\n");
        printf(" -q             miminimes output comments\n");
        return 1;
    }

	set_user_config_from_env( SNAPENV );
    install_default_crdsys_file();
    /* Load the station file */

    init_network( &net );
    gbformat = 0;
    oldfn = argv[1];

    if( argc > 2 && _stricmp(argv[2],"gb")==0 )
    {
        argc--;
        argv++;
        gbformat = 1;
    }

    if( argc > 2 )
    {
        newfn = argv[2];
    }
    else
    {
        nch = path_len( argv[1], 1 );
        newfn = (char *) check_malloc( nch + 5 );
        strncpy( newfn, argv[1], nch );
        strcpy( newfn+nch, ".new");
    }

    if( read_network( &net, oldfn, gbformat ) != OK )
    {
        printf("Unable to load station file %s\n",oldfn);
        return 2;
    }

    if( ! (net.options & NW_GEOID_HEIGHTS) ) calc_geoid = 1;

    if( calc_geoid )
    {
        gd = create_geoid_grid( geoid );
        if( !gd )
        {
            printf("Unable to load geoid model\n");
            return 2;
        }

        if( ! quiet )
        {
            printf("\nGeoid model definition\n");
            print_geoid_header( gd, stdout, 0, "   " );
        }

        sprintf(geoid_msg,"Geoid undulations from %.80s",get_geoid_model( gd ));        
        int sts = set_network_geoid_def( &net, gd );
        if( sts != OK ) return 2;
    }

    if( remove_geoid ) net.options &= ~( NW_GEOID_HEIGHTS | NW_DEFLECTIONS);

    if( change_height_type )
    {
        if( output_ellipsoidal )
        {
            net.options |= NW_ELLIPSOIDAL_HEIGHTS;
        }
        else
        {
            net.options &= ~ NW_ELLIPSOIDAL_HEIGHTS;
        }
    }

    /* And write the file out again */

    if( write_network( &net, newfn,
                       calc_geoid ? geoid_msg : NULL,
                       3,NULL) != OK )
    {
        return 2;
    }

    if( ! quiet )
    {
        printf("\nThe revised station file is %s\n",newfn);
    }


    return 0;
}
