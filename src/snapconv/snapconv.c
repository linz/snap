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
#include "util/versioninfo.h"
#include "snap/filenames.h"

#ifdef SNAPCONV_GRID
#include "geoid/griddata.h"
#endif


static char rcsid[]="$Id: snapconv.c,v 1.6 2004/02/02 03:27:28 ccrook Exp $";

#ifdef SNAPCONV_GRID
void apply_grid_distortion( network *net, grid_def *grid, int dim, int undo );
#endif


int main( int argc, char *argv[] )
{
    coordsys *cs;
    network *net;
    char *msg;
    char quiet = 0;
    char setformat = 0;
    char degoption = 0;
	double epoch = 0.0;
#ifdef SNAPCONV_GRID
    char *gridfile;
    char *gridname;
    int grid_dim;
    int undo_grid;
    grid_def *grid = NULL;
#endif

    /* Crude fix to allow suppression of output */

    while( argc > 1 && argv[1][0] == '-' )
    {
        switch( argv[1][1] )
        {
        case 'Q':
        case 'q': quiet = 1; break;
        case 'D':
        case 'd': setformat = 1; degoption = NW_DEC_DEGREES; break;
        case 'H':
        case 'h': setformat = 1; degoption = 0; break;
		case 'Y':
		case 'y': 
                if( argc < 3 || ! parse_crdsys_epoch(argv[2],&epoch) )
				{
					printf("Invalid value for conversion epoch (-Y %s)\n",argv[2]);
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
               ProgramVersion.program, ProgramVersion.version);
    }

#ifdef SNAPCONV_GRID
    if( argc == 6 && _strnicmp(argv[1],"-g",2)==0 )
    {
        undo_grid = argv[1][2] == '-';
        set_find_file_directories( exefile, NULL, NULL );
        gridname = argv[2];
        gridfile = find_file( gridname, ".grd", FF_TRYALL );

        if( gridfile )
        {
            gridfile = copy_string( gridfile );
            if( grd_open_grid_file(gridfile,2,&grid) == OK )
            {
                grid_dim = 2;
            }
            else if( grd_open_grid_file(gridfile,3,&grid) == OK )
            {
                grid_dim = 3;
            }
            else
            {
                grid = NULL;
            }
        }
        if( ! grid )
        {
            printf("Cannot open grid file %s\n",gridname);
            return 0;
        }
        argv += 2;
        argc -= 2;
    }
    else
#endif

        if( argc != 4 )
        {
            printf ("snapconv: Missing or invalid parameters\n\n");

#ifdef SNAPCONV_GRID
            printf("Syntax: snapconv [-g grid_file] input_coord_file new_coordsys_code output_coord_file\n");
            printf("\nThe -g grid_file option applies a distortion defined in the grid file to coordinates\n");
            printf("Use -g- gridfile to reverse the distortion\n");
#else
            printf("Syntax: snapconv [-d][-h][-q] input_coord_file new_coordsys_code output_coord_file\n\n");
            printf("Options are:\n");
            printf("  -d   Output angles in decimal degrees\n");
            printf("  -h   Output angles in degrees/minutes/seconds\n");
            printf("  -q   Operate quietly\n");

#endif
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

#ifdef SNAPCONV_GRID
    if( grid ) apply_grid_distortion( net, grid, grid_dim, undo_grid  );
#endif

    if( set_network_coordsys( net, cs, epoch ) != OK )
    {
        printf("Unable to convert network coordinate system to %s\n",argv[2]);
        return 2;
    }

    msg = NULL;
#ifdef SNAPCONV_GRID
    if( grid )
    {
        char *modname = grd_title( grid, 1 );
        if( modname )
        {
            msg = malloc( strlen(modname)+20 );
            if( msg )
            {
                if( undo_grid )
                {
                    strcpy(msg,"Removed: ");
                }
                else
                {
                    strcpy(msg,"Applied: ");
                }
                strcat( msg, modname );
            }
        }
    }
#endif

    if( setformat )
    {
        net->options &= ~NW_DEC_DEGREES;
        net->options |= degoption;
    }
    if( write_network( net, argv[3], msg, 0, 0 ) != OK )
    {
        return 2;
    }

    return 0;
}

#ifdef SNAPCONV_GRID
void apply_grid_distortion( network *net, grid_def *grid, int dim, int undo )
{
    coordsys *gcs;    /* Grid coordinate system */
    station *st;
    double factor;
    int sign;

    gcs = load_coordsys( grd_coordsys_def(grid) );
    if( !gcs )
    {
        printf("\nUnable to find a definition of grid coordinate system %s\n",
               grd_coordsys_def(grid) );
        exit(0);
    }

    if( set_network_coordsys( net, gcs, epoch ) != OK )
    {
        printf("\nUnable to convert coordinates to grid coordinate system %s\n",
               grd_coordsys_def(grid));
        exit(0);
    }

    /* Geodetic (lat/lon) coordinates are in radians, whereas grid files will
       be in degrees, so convert to degrees */

    if( is_geodetic(gcs) )
    {
        factor = M_PI/180.0;
    }
    else
    {
        factor = 1.0;
    }
    sign = undo ? -1 : 1;

    reset_station_list( net, 0 );

    while( NULL != (st=next_station(net)) )
    {
        double xyz[3], dxyz[3];
        get_network_coordinates( net, st, xyz );
        grd_calc_linear( grid, xyz[CRD_EAST]/factor, xyz[CRD_NORTH]/factor,dxyz );
        xyz[CRD_EAST] += dxyz[0]*sign*factor;
        xyz[CRD_NORTH] += dxyz[1]*sign*factor;

        /* Note: degrees to radians factor doesn't apply to the height ordinate */
        if( dim == 3 ) xyz[CRD_HGT] += dxyz[2]*sign;
        set_network_coordinates( net, st, xyz );
    }
}
#endif

