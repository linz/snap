#include "snapconfig.h"
/* A program to look at data used in a SNAP adjustment, either before an
   adjustment is done, using the configuration file, or after the
   adjustment is done, using the binary file.  In the latter case the
   program can also show error ellipses at selected exaggeration (and at
   a selected confidence level

*/


/*
   $Log: snapplot.c,v $
   Revision 1.4  1999/05/20 10:59:01  ccrook
   Changed DOSLI to LINZ

   Revision 1.3  1998/06/15 02:14:24  ccrook
   Updated version number to 2.13

   Revision 1.2  1996/07/12 20:34:33  CHRIS
   Modified to support hidden stations.

   Revision 1.1  1996/01/03 22:31:17  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "util/errdef.h"

#define MAIN

#include "snap/snapglob.h"
#include "snap/gpscvr2.h"
#include "coordsys/coordsys.h"
#include "snap/stnadj.h"
#include "snap/survfile.h"
#include "util/fileutil.h"
#include "util/versioninfo.h"
#include "plotbin.h"
#include "loadplot.h"
#include "plotscal.h"
#include "plotstns.h"
#include "plotpens.h"
#include "plotconn.h"
#include "plotcmd.h"
#include "backgrnd.h"

#include "snapplot_load.h"
#include "snapplot_util.h"

#define VERSION ProgramVersion.version

/* static void print_header( void ); */

static void print_help( void );

#define MAXFILES 20

int snapplot_load( int argc, char *argv[] )
{

    int sts;
    int binary_data;
    char *firstfile = NULL;
    char *filelist[MAXFILES];
    char *cfgfile[MAXFILES];
    char *projection = NULL;
    coordsys *projcs = NULL;
    int nfiles = 0;
    int ncfgfiles = 0;
    int syntax_error = 0;
    int narg;
    int use_command_file = 1;
    int use_binary_file = 1;
    int i;

    /* print_header(); */

    for( narg = 1; narg < argc; narg++ )
    {
        char *arg = argv[narg];
        if( arg[0] == '-' ) switch( arg[1] )
            {

            case 'i':
            case 'I': use_binary_file = 0; break;

            case 'f':
            case 'F': use_command_file = 0; break;

            case 'c':
            case 'C': if( ncfgfiles >= MAXFILES )
                {
                    print_log("\nToo many config files in command line\n");
                }
                else if( arg[2] )
                {
                    cfgfile[ncfgfiles++] = arg+2;
                }
                else if( ++narg < argc )
                {
                    cfgfile[ncfgfiles++] = argv[narg];
                }
                else
                {
                    print_log("\nMissing name of config file\n");
                    syntax_error = 1;
                }
                break;

            case 'b':
            case 'B': if( ncfgfiles >= MAXFILES )
                {
                    print_log("\nToo many config files in command line\n");
                }
                else if( arg[2] )
                {
                    add_background_file( arg+2, NULL, NULL );
                }
                else if( ++narg < argc )
                {
                    add_background_file( argv[narg], NULL, NULL );
                }
                else
                {
                    print_log("\nMissing name of config file\n");
                    syntax_error = 1;
                }
                break;

            case 'p':
            case 'P': if( arg[2] )
                {
                    projection = arg+2;
                }
                else if( ++narg < argc )
                {
                    projection = argv[narg];
                }
                else
                {
                    print_log("\nMissing projection name on command line\n");
                    syntax_error = 1;
                }
                break;

            default:  print_log("\nInvalid option %s in command line\n",arg);
                syntax_error = 1;
                break;


            }

        else if( !firstfile )
        {
            firstfile = arg;
        }

        else if(nfiles < MAXFILES)
        {
            filelist[nfiles++] = arg;
        }

        else
        {
            print_log("\nToo many file names in command line\n");
        }
    }


    if( !firstfile )
    {
        if( argc > 1 )
        {
            if( use_command_file )
            {
                print_log("\nMissing command file name\n");
            }
            else
            {
                print_log("\nMissing coordinate file name\n");
            }
        }
        syntax_error = 1;
    }

    if( syntax_error )
    {
        print_help();
        return 0;
    }

    init_snap_globals();
    init_snap_gps_covariance();


    /*
    { char helpfile[256];
      int nch;
      strncpy(helpfile, prog_dir, 255 );
      helpfile[255] = 0;
      nch = strlen(helpfile);
      strncpy(helpfile+nch,"SNAPPLOT.HLP",255-nch);
      helpfile[255] = 0;
      install_help_file( helpfile );
      }
      */

    install_default_crdsys_file();

    if( projection )
    {
        projcs = load_coordsys( projection );
        if( !projcs )
        {
            print_log("\n%s is not a valid projection code\n",projection);
            return 0;
        }
        if( !is_projection( projcs ) )
        {
            print_log("\n%s in not a projection coordinate system\n");
            return 0;
        }
    }

    /* Note: even if not using a command file we call set_snap_command_file
       as it ensures that default paths etc are defined */

    set_snap_command_file( firstfile );

    if( use_command_file )
    {
        sts = reload_binary_data();
        binary_data = sts == OK;
        if( sts == NO_MORE_DATA ) sts = OK;  /* No binary file */
        if( sts != OK ) return 0;
    }
    else
    {
        binary_data = 0;
    }

    set_confidence_limit();  /* Set error type to match binary file */

    add_default_configuration_files();

    if( use_command_file )
    {
        set_stnadj_init_network();
        sts = read_plot_command_file( command_file, binary_data );
        if( sts != OK )
        {
            print_log("\n%s command file %s\n",
                      sts == FILE_OPEN_ERROR ? "Cannot open" : "Errors in",
                      command_file);
            return 0;
        }
        for( i = 0; i < nfiles; i++ )
        {
            if( add_configuration_file( filelist[i] ) != OK )
            {
                handle_error( FILE_OPEN_ERROR, "Configuration file cannot be found",
                              filelist[i] );
            }
        }
    }
    else
    {
        print_log("\nReading coordinates from file %s\n",firstfile);
        /* Adding the following line as init function was removed from read_station_file.
         * Possibly not necessary?
         */
        set_stnadj_init_network();
        sts = read_station_file( firstfile, cmd_dir, STN_FORMAT_SNAP, 0 );
        if( sts == OK )
        {
            print_log("    %d stations read\n",number_of_stations(net));
        }
        else
        {
            print_log("\nErrors encountered reading coordinate file\n");
            return 0;
        }
        for( i = 0; i < nfiles; i++ )
        {
            add_data_file( filelist[i], SNAP_FORMAT, 0, 1.0, 0, 0 );
        }
    }


    for( i = 0; i < ncfgfiles; i++ )
    {
        const char *filename = find_file( cfgfile[i], SNAPPLOT_CONFIG_EXT, 0, FF_TRYLOCAL, SNAPPLOT_CONFIG_SECTION );
        if( add_configuration_file( filename ) != 0 )
        {
            handle_error( FILE_OPEN_ERROR, "Configuration file cannot be found", cfgfile[i] );
        }
    }

    if( projcs ) set_plot_projection( projcs );

    init_plotstns( binary_data );

    if( !binary_data )
    {
        open_data_source();
        load_connections();
    }

    /* Must be after init_plotstns so that plot projection is defined */
    load_background_files();

    init_plot_scales();

    /* Can't actually read the configuration file until everything is set up.. */

    /* setup_plot(); */

    // TODO: see if this is needed - may need setup_key somewhere here ... init_options();

    init_displayed_fields();

    setup_data_pens( DPEN_BY_TYPE );

    process_configuration_file_list();

    return get_error_count() ? 0 : 1;
}

void snapplot_unload()
{
    /* NOTE: SNAP wasn't set up for loading/unloading data, so this unload function doesn't undo
       most of allocations incurred by snapplot_load */

    free_connection_resources();
    free_station_resources();

    close_data_source();

    unload_stations();
    uninstall_crdsys_lists();
}

static void print_header( void )
{
    print_log("\n\n                              SNAPPLOT version %s\n", VERSION);
    print_log("                     Copyright: Land Information New Zealand\n");
}

static void print_help( void )
{
    print_log("\nSyntax:  SNAPPLOT [options] command_file_name [config_file_name ..]\n");
    print_log(  "    or   SNAPPLOT [options] -f coord_file [data_file ...]\n");
    print_log("\nOptions can include: \n");
    print_log("  -c config_file_name     Specifies a configuration file to use\n");
    print_log("  -b background_file      Specifies a background file to use\n");
    print_log("  -p projection_code      Specifies the plot projection\n");
    print_log("  -i                      Don't use binary file even if one exists\n");
    /* print_log("  -s#                     Specifies the video mode (# = 1,2,or 3)\n"); */
    print_log("\n");
}
