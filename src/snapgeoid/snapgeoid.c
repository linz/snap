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

#include "coordsys/coordsys.h"
#include "network/network.h"
#include "geoid/geoid.h"
#include "util/chkalloc.h"
#include "util/fileutil.h"
#include "util/errdef.h"
#include "snap/filenames.h"
#include "util/getversion.h"

enum
{
    CALC_NONE=0,
    CALC_CRDSYS,
    CALC_HGTREF,
    CALC_GEOID,
    CALC_REMOVE
};

enum
{
    SET_HGTTYPE_DEFAULT=0,
    SET_HGTTYPE_ELLIPSOIDAL,
    SET_HGTTYPE_ORTHOMETRIC,
};

int main( int argc, char *argv[] )
{
    char *oldfn, *newfn;
    int readopt;
    int nch;
    network net;
    char list_only=0;
    char keep_existing = 0;
    char syntax_error = 0;
    char quiet = 0;
    char set_height_type = SET_HGTTYPE_DEFAULT;
    char calc_geoid_opt = CALC_NONE;
    char calc_crdsys_geoid = 0;
    int orthometric_fixed=NW_HGTFIXEDOPT_ELLIPSOIDAL;
    char geoid_msg[120];
    char remove_csyshrs = 0;
    char *csyshrs = NULL;
    char *hrscode = NULL;
    char *geoid = NULL;
    char **argptr=NULL;
    int errlevel=WARNING_ERROR;
    geoid_def *gd = NULL;

    set_error_file( stdout );
    geoid_msg[0] = 0;

    while( argc > 1 && argv[1][0] == '-' )
    {
        argptr=0;
        switch( argv[1][1] )
        {
        case 'v':
        case 'V':
            list_only = 1;
            break;

        case 'e':
        case 'E':
            if( set_height_type != SET_HGTTYPE_DEFAULT ) syntax_error = 1;
            set_height_type = SET_HGTTYPE_ELLIPSOIDAL;
            break;

        case 'o':
        case 'O':
            if( set_height_type != SET_HGTTYPE_DEFAULT ) syntax_error = 1;
            set_height_type = SET_HGTTYPE_ORTHOMETRIC;
            break;

        case 'p':
        case 'P':
            orthometric_fixed=NW_HGTFIXEDOPT_ORTHOMETRIC;
            break;

        case 'i':
        case 'I':
            errlevel=INFO_ERROR;
            break;

        case 'z':
        case 'Z':
            if( calc_geoid_opt != CALC_NONE ) syntax_error = 1;
            calc_geoid_opt = CALC_REMOVE;
            break;

        case 'c':
        case 'C':
            if( calc_geoid_opt != CALC_NONE ) syntax_error = 1;
            calc_geoid_opt = CALC_CRDSYS;
            break;

        case 'g':
        case 'G':
            if( calc_geoid_opt != CALC_NONE ) syntax_error = 1;
            calc_geoid_opt = CALC_GEOID;
            argptr=&geoid;
            break;

        case 'h':
        case 'H':
            if( calc_geoid_opt != CALC_NONE ) syntax_error = 1;
            calc_geoid_opt = CALC_HGTREF;
            argptr=&hrscode;
            break;

        case 'k':
        case 'K':
            keep_existing=1;
            break;

        case 'a':
        case 'A':
            if( remove_csyshrs ) syntax_error=1;
            argptr=&csyshrs;
            break;

        case 'd':
        case 'D':
            if( csyshrs ) syntax_error=1;
            remove_csyshrs=1;
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
        if( argptr )
        {
            if( argc > 1  && ! *argptr )
            {
                (*argptr) = argv[1];
                argv++;
                argc--;
            }
            else
            {
                syntax_error = 1;
            }
        }
    }

    if( ! quiet )
    {
        printf("%s version %s\n\nCalculates geoid undulations on a SNAP coordinate file\n\n",
               PROGRAM_NAME, PROGRAM_VERSION);
    }

    if( syntax_error )
    {
        printf("Error in snapgeoid command\n\n");
    }

    if( (argc < 2 && ! list_only) || syntax_error )
    {
        printf("Syntax:  snapgeoid  [options] station_file_name [new_station_file_name]\n\n");
        printf("Options can include:\n");
        printf(" -h vd_code     calculate geoid from vertical datum (-v to list)\n");
        printf(" -g geoid_file  calculate geoid height from gridded geoid model file\n");
        printf(" -c             calculate geoid height from coordinate system vertical datum\n");
        printf(" -z             remove explicit geoid information from coordinate file\n");
        printf(" -k             keep existing geoid information - only add if not already in file\n");
        printf(" -p             preserves orthometric heights when the geoid height is changed\n");
        printf(" -i             ignore errors calculating geoid heights for specific stations\n");
        printf(" -a vd_code     add a vertical datum to coordinate system\n");
        printf(" -d             remove vertical datum from coordinate system\n");
        printf(" -e             set the height coordinate type to ellipsoidal\n");
        printf(" -o             set the height coordinate type to orthometric\n");
        printf(" -v             list the available vertical datums and exit\n");
        printf(" -q             miminizes output comments\n");
        printf("\nOnly one of -h, -g, -c -z can be selected.\n");
        printf("\nOnly one of -e and -o can be selected.\n");
        return 1;
    }

    install_default_crdsys_file();
    if( list_only )
    {
        int i;
        int nhrf=vdatum_list_count();
        for( i=0; i < nhrf; i++)
        {
            const char *code=vdatum_list_code(i);
            const char *name=vdatum_list_desc(i);
            printf("  %-*s %s\n",CRDSYS_CODE_LEN,code,name);
        }
        exit(0);
    }
    /* Load the station file */

    init_network( &net );
    readopt = NW_READOPT_CALCHGTREF;
    oldfn = argv[1];

    if( argc > 2 && _stricmp(argv[2],"gb")==0 )
    {
        argc--;
        argv++;
        readopt |= NW_READOPT_GBFORMAT;
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

    if( read_network( &net, oldfn, readopt ) != OK )
    {
        printf("Unable to load station file %s\n",oldfn);
        return 2;
    }

    if( keep_existing && network_has_explicit_geoid_info(&net) && calc_geoid_opt != CALC_REMOVE )
    {
        calc_geoid_opt=CALC_NONE;
    }

    if( ! network_has_geoid_info(&net) && 
        ( set_height_type == SET_HGTTYPE_ORTHOMETRIC ||
            ( set_height_type == SET_HGTTYPE_DEFAULT && 
                ! network_height_coord_is_ellipsoidal( &net ) )
        )
      )
    {
        orthometric_fixed = NW_HGTFIXEDOPT_ORTHOMETRIC;
    }

    if( errlevel == INFO_ERROR && quiet ) errlevel=OK;

    if( csyshrs )
    {
        vdatum *hrs=load_vdatum( csyshrs );
        if( ! hrs )
        {
            printf("Unable to load vertical datum %s\n",csyshrs);
            return 2;
        }
        int sts=set_coordsys_vdatum( net.crdsys, hrs );
        if( sts != OK )
        {
            return 2;
        }
        if( ! network_has_explicit_geoid_info(&net) )
        {
            calc_crdsys_geoid = 1;
        }
    }

    if( calc_crdsys_geoid || calc_geoid_opt == CALC_CRDSYS )
    {
        int sts=calc_station_geoid_info_from_coordsys( &net, net.crdsys,
                orthometric_fixed, errlevel );
        if( sts != OK && sts != INFO_ERROR ) return 2;
        vdatum *hrs = coordsys_vdatum( net.crdsys );
        if( hrs )
        {
            sprintf(geoid_msg,"Geoid undulations from %.80s",hrs->name);
        }
    }

    if( calc_geoid_opt == CALC_HGTREF )
    {
        vdatum *hrs=load_vdatum( hrscode );
        if( ! hrs )
        {
            printf("Unable to load vertical datum %s\n",hrscode);
            return 2;
        }
        ref_frame *rf=vdatum_ref_frame(hrs);
        if( ! rf )
        {
            printf("Unable to load reference frame for vertical datum %s\n",
                    hrscode);
            return 2;
        }
        coordsys *cs=create_coordsys(rf->code,"",CSTP_GEODETIC,rf,0);
        if( ! cs )
        {
            printf("Unable to create reference coordinate system for vertical datum %s\n",
                    hrscode);
            return 2;
        }
        int sts=set_coordsys_vdatum( cs, hrs );
        if( sts != OK )
        {
            printf("Unable to assign vertical datum %s to base coordinate system",
                    hrscode);
            return 2;
        }
        sts=calc_station_geoid_info_from_coordsys( &net, cs,
                orthometric_fixed, errlevel );
        if( sts != OK && sts != INFO_ERROR ) return 2;
        sprintf(geoid_msg,"Geoid undulations from %.80s",hrs->name);
    }

    else if( calc_geoid_opt == CALC_GEOID )
    {
        if( errlevel == INFO_ERROR && quiet ) errlevel=OK;
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
        int sts = set_network_geoid_def( &net, gd, orthometric_fixed, errlevel );
        if( sts != OK && sts != INFO_ERROR ) return 2;
    }

    if( calc_geoid_opt == CALC_REMOVE ) 
    {
        clear_network_explicit_geoid_info( &net );
    }
    else if ( calc_geoid_opt != CALC_NONE )
    {
        set_network_explicit_geoid_info( &net, 0 );
    }


    if( set_height_type == SET_HGTTYPE_ELLIPSOIDAL )
    {
        set_network_height_coord_ellipsoidal( &net );
    }
    else if( set_height_type == SET_HGTTYPE_ORTHOMETRIC )
    {
        set_network_height_coord_orthometric( &net );
    }

    /* And write the file out again */

    if( write_network( &net, newfn,
                       geoid_msg[0] ? geoid_msg : NULL,
                       0,NULL) != OK )
    {
        return 2;
    }

    if( ! quiet )
    {
        printf("\nThe updated station file is %s\n",newfn);
    }


    return 0;
}
