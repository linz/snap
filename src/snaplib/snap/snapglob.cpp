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

static bool initialised=false;

void init_snap_globals()
{
    int i;
    if( initialised ) return;
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
    initialised=true;
}


void set_snap_command_file( char *cmd_file )
{
    if( ! initialised ) init_snap_globals();
    if( file_exists( cmd_file ) )
    {
        command_file = copy_string( cmd_file );
    }
    else
    {
        char *cf;
        int nchmax;
        nchmax = strlen(DFLTCOMMAND_EXT);
        if( strlen(DFLTCOMMAND_EXT2) > nchmax )
        {
            nchmax=strlen(DFLTCOMMAND_EXT2);
        }
        if( strlen(DFLTCOMMAND_EXT3) > nchmax )
        {
            nchmax=strlen(DFLTCOMMAND_EXT3);
        }
        nchmax += strlen(cmd_file) + 1;
        cf = (char *) check_malloc(nchmax);
        strcpy(cf,cmd_file);
        strcat(cf,DFLTCOMMAND_EXT);
        if( ! file_exists(cf) )
        {
            strcpy(cf,cmd_file);
            strcat(cf,DFLTCOMMAND_EXT2);
        }
        if( ! file_exists(cf))
        {
            strcpy(cf,cmd_file);
            strcat(cf,DFLTCOMMAND_EXT3);            
        }
        if( ! file_exists(cf))
        {
            strcpy(cf,cmd_file);
        }
        command_file = cf;
    }

    cmd_dir=copy_string_nch( command_file, path_len(command_file,0));
    root_name=copy_string_nch( command_file, path_len(command_file,1));
    push_file_context( cmd_dir );
}


void set_snap_config_file( char *cfg_file )
{
    if( ! initialised ) init_snap_globals();
    config_file = cfg_file;
}

void *snap_obs_modifications( bool create )
{
    if( ! initialised ) init_snap_globals();
    if( (! obs_modifications) && create)
    {
        obs_modifications=new_obs_modifications( net, &obs_classes );
        set_obs_modifications_file_func( obs_modifications, survey_data_file_id, survey_data_file_name );
    }
    return obs_modifications;
}

void dump_snap_globals( BINARY_FILE *b )
{
    if( ! initialised ) init_snap_globals();
    create_section( b, "SNAP_GLOBALS" );

    fwrite( job_title, JOBTITLELEN+1, 1, b->f );
    fwrite( run_time, GETDATELEN, 1, b->f );
    dump_bin(b, dimension);
    dump_bin(b, program_mode);
    dump_bin_long32(b, nobs);
    dump_bin(b, nprm);
    dump_bin_long32(b, nschp);
    dump_bin_long32(b, ncon);
    dump_bin_long32(b, dof);
    dump_bin(b, ssr);
    dump_bin(b, seu);
    dump_bin(b, iterations);
    dump_bin(b, converged);
    dump_bin(b, apriori);
    dump_bin(b, flag_level[0]);
    dump_bin(b, flag_level[1]);
    /* TODO : Codeguard complains attempting to access 4 bytes from 2 byte block.  Possibly getting sizeof address rather than sizeof addressee */
    dump_bin(b, taumax[0]);
    dump_bin(b, taumax[1]);
    dump_bin(b, coord_precision);
    dump_bin(b, have_obs_ids);
    dump_bin(b, errconflim);
    dump_bin(b, errconfval);
    end_section( b );
}


int reload_snap_globals( BINARY_FILE *b )
{
    if( ! initialised ) init_snap_globals();

    if( find_section( b, "SNAP_GLOBALS" ) != OK ) return MISSING_DATA;

    fread( job_title, JOBTITLELEN+1, 1, b->f );
    fread( run_time, GETDATELEN, 1, b->f );
    reload_bin(b, dimension);
    reload_bin(b, program_mode);
    reload_bin_long32(b, nobs);
    reload_bin(b, nprm);
    reload_bin_long32(b, nschp);
    reload_bin_long32(b, ncon);
    reload_bin_long32(b, dof);
    reload_bin(b, ssr);
    reload_bin(b, seu);
    reload_bin(b, iterations);
    reload_bin(b, converged);
    reload_bin(b, apriori);
    reload_bin(b, flag_level[0]);
    reload_bin(b, flag_level[1]);
    reload_bin(b, taumax[0]);
    reload_bin(b, taumax[1]);
    reload_bin(b, coord_precision);
    reload_bin(b, have_obs_ids);
    if( check_end_section(b) == OK ) return OK;
    reload_bin(b, errconflim);
    reload_bin(b, errconfval);
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

