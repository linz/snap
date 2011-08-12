#include "snapconfig.h"
/*
   $Log: snapglob.c,v $
   Revision 1.5  2003/11/24 01:34:13  ccrook
   Updated to allow .snp as command file name

   Revision 1.4  2001/05/14 18:21:02  ccrook
   *** empty log message ***

   Revision 1.3  1998/05/21 04:01:57  ccrook
   Added support for deformation model to be applied in the adjustment.

   Revision 1.2  1996/02/23 16:57:13  CHRIS
   Adding mde_power to global variables - setting default value to 80%

   Revision 1.1  1995/12/22 17:47:56  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _SNAPGLOB_C
#include "util/binfile.h"
#include "snap/snapglob.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/fileutil.h"


static char rcsid[]="$Id: snapglob.c,v 1.5 2003/11/24 01:34:13 ccrook Exp $";

void init_snap_globals( char *prog_file )
{
    int nch;
    int i;

    program_file_name = find_image(prog_file);
    command_file = NULL;
    config_file = NULL;
    root_name = NULL;
    cmd_dir = NULL;
    user_dir = getenv( SNAPENV );
    if( user_dir )
    {
        nch = strlen(user_dir);
        if( user_dir[nch-1] != PATH_SEPARATOR )
        {
            char *u = user_dir;
            user_dir = (char *) check_malloc( nch+2 );
            strcpy( user_dir, u );
            user_dir[nch] = PATH_SEPARATOR;
            user_dir[nch+1] = '\0';
        }
    }
    prog_dir = NULL;
    nch = path_len( program_file_name, 0 );
    if( nch )
    {
        prog_dir = (char *) check_malloc( nch + 1 );
        strncpy( prog_dir, program_file_name, nch );
        prog_dir[nch] = 0;
    }

    job_title[0] = 0;
    dimension = 2;
    program_mode = ADJUST;
    max_iterations = 5;
    max_adjustment = 1000.0;
    convergence_tol = 0.001;
    maxworst = 10;
    apriori = 1;
    flag_level[0] = 95.0;
    flag_level[1] = 99.0;
    mde_power = 80.0;
    redundancy_flag_level = 0.1;
    taumax[0] = taumax[1] = 0;
    file_location_frequency = 10;
    stn_name_width = 5;
    use_distance_ratios = 1;
    coord_precision = 4;
    ignore_deformation = 0;
    deformation = NULL;
    have_obs_ids = 0;
    for( i=0; i<NOBSTYPE; i++ )
    {
        obs_usage[i] = 0;
        obs_errfct[i] = 1.0;
        obstypecount[i] = 0;
        obs_precision[i] = datatype[i].dfltndp;
    }
    init_classifications( &obs_classes );
    set_find_file_directories( program_file_name, 0, user_dir );
}


void set_snap_command_file( char *cmd_file )
{
    int nch;

    if( file_exists( cmd_file ) )
    {
        command_file = copy_string( cmd_file );
    }
    else
    {
        char *cf;
        int nchmax;
        nchmax = strlen(DFLTCOMMAND_EXT) > strlen(DFLTCOMMAND_EXT2) ?
                 strlen(DFLTCOMMAND_EXT) : strlen(DFLTCOMMAND_EXT2);
        nchmax += strlen(cmd_file) + 1;
        cf = (char *) check_malloc(nchmax);
        strcpy(cf,cmd_file);
        strcat(cf,DFLTCOMMAND_EXT);
        if( ! file_exists(cf) )
        {
            strcpy(cf,cmd_file);
            strcat(cf,DFLTCOMMAND_EXT2);
        }
        command_file = cf;
    }


    set_find_file_base_dir( command_file );

    nch = path_len( command_file, 0 );
    if( nch )
    {
        cmd_dir = (char *) check_malloc( nch+1);
        strncpy( cmd_dir, command_file, nch );
        cmd_dir[nch] = 0;
    }

    nch = path_len( cmd_file, 1 );
    root_name = (char *) check_malloc( nch+1 );
    if( nch > 0 ) memcpy( root_name, cmd_file, nch );
    root_name[nch] = 0;
}


void set_snap_config_file( char *cfg_file )
{
    config_file = cfg_file;
}

void set_snap_user_dir( char *usr_dir )
{
    set_find_file_home_dir( usr_dir );
    user_dir = usr_dir;
}



void dump_snap_globals( BINARY_FILE *b )
{

    create_section( b, "SNAP_GLOBALS" );

    fwrite( job_title, JOBTITLELEN+1, 1, b->f );
    fwrite( run_time, GETDATELEN, 1, b->f );
    DUMP_BIN(dimension, b);
    DUMP_BIN(program_mode, b);
    DUMP_BIN(nobs, b);
    DUMP_BIN(nprm, b);
    DUMP_BIN(nschp, b);
    DUMP_BIN(ncon, b);
    DUMP_BIN(dof, b);
    DUMP_BIN(ssr, b);
    DUMP_BIN(seu, b);
    DUMP_BIN(iterations, b);
    DUMP_BIN(converged, b);
    DUMP_BIN(apriori, b);
    DUMP_BIN(flag_level[0], b);
    DUMP_BIN(flag_level[1], b);
    /* TODO : Codeguard complains attempting to access 4 bytes from 2 byte block.  Possibly getting sizeof address rather than sizeof addressee */DUMP_BIN(taumax[0], b);
    DUMP_BIN(taumax[1], b);
    DUMP_BIN(coord_precision, b);
    DUMP_BIN(have_obs_ids, b);

    end_section( b );
}


int reload_snap_globals( BINARY_FILE *b )
{

    if( find_section( b, "SNAP_GLOBALS" ) != OK ) return MISSING_DATA;

    fread( job_title, JOBTITLELEN+1, 1, b->f );
    fread( run_time, GETDATELEN, 1, b->f );
    RELOAD_BIN(dimension, b);
    RELOAD_BIN(program_mode, b);
    RELOAD_BIN(nobs, b);
    RELOAD_BIN(nprm, b);
    RELOAD_BIN(nschp, b);
    RELOAD_BIN(ncon, b);
    RELOAD_BIN(dof, b);
    RELOAD_BIN(ssr, b);
    RELOAD_BIN(seu, b);
    RELOAD_BIN(iterations, b);
    RELOAD_BIN(converged, b);
    RELOAD_BIN(apriori, b);
    RELOAD_BIN(flag_level[0], b);
    RELOAD_BIN(flag_level[1], b);
    RELOAD_BIN(taumax[0], b);
    RELOAD_BIN(taumax[1], b);
    RELOAD_BIN(coord_precision, b);
    RELOAD_BIN(have_obs_ids, b);

    return check_end_section( b );
}

void dump_obs_classes( BINARY_FILE *b )
{
    create_section( b, "OBS_CLASSES" );
    dump_classifications( &obs_classes, b->f );
    end_section(b);

}

int reload_obs_classes( BINARY_FILE *b )
{
    if( find_section( b, "OBS_CLASSES") != OK ) return MISSING_DATA;
    reload_classifications( &obs_classes, b->f );
    return check_end_section(b);
}

