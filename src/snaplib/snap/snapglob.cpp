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
#include <array>
#include <filesystem>
#include <string>
#include <string_view>

#define _SNAPGLOB_C
#include "util/binfile.h"
#include "snap/snapglob.h"
#include "snap/survfile.h"
#include "snap/stnadj.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/fileutil.h"
#include "util/get_date.h"

std::optional<CommandFile> command_file;
std::optional<std::string> config_file;
std::optional<std::string> snap_user;

static bool initialised=false;

void init_snap_globals()
{
    if( initialised ) return;
    for( const char *const variable : { "SNAPUSER", "USERNAME", "USER" } )
    {
        const char *const value = getenv( variable );
        if( value )
        {
            snap_user = value;
            break;
        }
    }
    run_time = get_date();

    job_title.clear();
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
    deformation = nullptr;
    have_obs_ids = 0;
    for( int i=0; i<NOBSTYPE; i++ )
    {
        obs_usage[i] = 0;
        obs_errfct[i] = 1.0;
        obstypecount[i] = 0;
        obs_precision[i] = datatype[i].dfltndp;
    }
    obs_modifications=nullptr;
    converged=1;
    last_iteration_max_adjustment=0.0;
    initialised=true;
}


std::string CommandFile::_locate( const std::string &name )
{
    if( path_exists( name ) ) return name;

    constexpr std::array<std::string_view, 3> extensions{ DFLTCOMMAND_EXT, DFLTCOMMAND_EXT2, DFLTCOMMAND_EXT3 };
    for( const std::string_view extension : extensions )
    {
        const std::string candidate = std::string(name).append(extension);
        if( path_exists(candidate) ) return candidate;
    }
    return name;
}

CommandFile::CommandFile( const std::string &name )
    : path( _locate( name ) ),
      dir( std::filesystem::path( native_path( path ) ).remove_filename().string() ),
      root( std::filesystem::path( native_path( path ) ).replace_extension().string() )
{
}

void set_snap_command_file( const std::string &cmd_file )
{
    if( ! initialised ) init_snap_globals();
    command_file.emplace( cmd_file );
    push_file_context( command_file->dir );
}


void set_snap_config_file( const std::string &cfg_file )
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

/// Writes the job title as a JOBTITLELEN+1 byte field, padded with NUL bytes.
static void write_job_title_field( FILE *const f )
{
    std::string field = job_title.substr( 0, JOBTITLELEN );
    field.resize( JOBTITLELEN+1, '\0' );
    fwrite( field.data(), field.size(), 1, f );
}

/// Reads the job title from a JOBTITLELEN+1 byte field, up to the first NUL byte.
/// Returns false, leaving job_title unchanged, if the field cannot be read.
static bool read_job_title_field( FILE *const f )
{
    std::array<char,JOBTITLELEN+1> field{};
    if( fread( field.data(), field.size(), 1, f ) != 1 ) return false;
    const std::string_view text( field.data(), field.size() );
    job_title = std::string( text.substr( 0, text.find( '\0' ) ) );
    return true;
}

void dump_snap_globals( BINARY_FILE *b )
{
    if( ! initialised ) init_snap_globals();
    create_section( b, "SNAP_GLOBALS" );

    write_job_title_field( b->f );
    write_run_date_field( b->f, run_time );
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

    read_job_title_field( b->f );
    read_run_date_field( b->f, run_time );
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
    obs_classes.dump( b->f );
    end_section(b);

}

int reload_obs_classes( BINARY_FILE *b )
{
    if( find_section( b, "OBS_CLASSES") != OK ) return MISSING_DATA;
    obs_classes.reload( b->f );
    return check_end_section(b);
}

