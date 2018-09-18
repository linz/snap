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
#include "snap/survfile.h"
#include "snap/stnadj.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/fileutil.h"
#include "util/get_date.h"

static bool initiallized=false;

void init_snap_globals()
{
    int i;
    if( initiallized ) return;
    command_file = NULL;
    config_file = NULL;
    root_name = NULL;
    cmd_dir = NULL;
    snap_user = getenv("SNAPUSER");
    if( ! snap_user ) snap_user = getenv("USERNAME");
    if( ! snap_user ) snap_user = getenv("USER");
    get_date( run_time );

    job_title[0] = 0;
    dimension = 2;
    program_mode = ADJUST;
    min_iterations = 0;
    max_iterations = 5;
    max_adjustment = 1000.0;
    convergence_tol = 0.0001;
    maxworst = 10;
    apriori = 1;
    errconflim = 0;
    errconfval = 1.0;
    flag_level[0] = 95.0;
    flag_level[1] = 99.0;
    mde_power = 80.0;
    redundancy_flag_level = 0.1;
    taumax[0] = taumax[1] = 0;
    file_location_frequency = 10;
    stn_name_width = 5;
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
    obs_modifications=0;
    converged=1;
    last_iteration_max_adjustment=0.0;
    initiallized=true;
}


void set_snap_command_file( char *cmd_file )
{
    if( ! initiallized ) init_snap_globals();
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

    cmd_dir=copy_string_nch( command_file, path_len(command_file,0));
    root_name=copy_string_nch( command_file, path_len(command_file,1));
    set_project_dir( cmd_dir );
}


void set_snap_config_file( char *cfg_file )
{
    if( ! initiallized ) init_snap_globals();
    config_file = cfg_file;
}

void *snap_obs_modifications( bool create )
{
    if( ! initiallized ) init_snap_globals();
    if( (! obs_modifications) && create)
    {
        obs_modifications=new_obs_modifications( net, &obs_classes );
        set_obs_modifications_file_func( obs_modifications, survey_data_file_id );
    }
    return obs_modifications;
}

void dump_snap_globals( BINARY_FILE *b )
{
    if( ! initiallized ) init_snap_globals();
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
    /* TODO : Codeguard complains attempting to access 4 bytes from 2 byte block.  Possibly getting sizeof address rather than sizeof addressee */
    DUMP_BIN(taumax[0], b);
    DUMP_BIN(taumax[1], b);
    DUMP_BIN(coord_precision, b);
    DUMP_BIN(have_obs_ids, b);
    DUMP_BIN(errconflim, b);
    DUMP_BIN(errconfval, b);
    end_section( b );
}


int reload_snap_globals( BINARY_FILE *b )
{
    if( ! initiallized ) init_snap_globals();

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
    if( check_end_section(b) == OK ) return OK;
    RELOAD_BIN(errconflim, b);
    RELOAD_BIN(errconfval, b);
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

