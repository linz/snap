#include "snapconfig.h"
/* control.c - reads a snap command file using the configuration file
   routines in readcfg.
   */

/*
   $Log: control.c,v $
   Revision 1.9  2004/04/22 02:35:43  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.8  2003/10/23 01:29:04  ccrook
   Updated to support absolute accuracy tests

   Revision 1.7  2003/08/01 02:00:31  ccrook
   Fixed bug in handling a range of stations in a station list.

   Revision 1.6  1999/05/20 10:46:58  ccrook
   Added commands relating to testing relative accuracies.

   Revision 1.5  1998/06/15 02:17:59  ccrook
   Fixed handling of "include" command in command file.

   Revision 1.4  1998/05/21 14:40:06  CHRIS
   Added the deformation command.

   Revision 1.3  1996/10/25 21:48:41  CHRIS
   Fixed bug in handling of classification command for data_type

   Revision 1.2  1996/02/23 17:11:43  CHRIS
   Added mde_power command to SNAP command files.

   Revision 1.1  1996/01/03 21:58:02  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <boost/algorithm/string/predicate.hpp>
#include <charconv>
#include <string_view>
#include "util/fieldscanner.hpp"
#include "util/snapctype.h"

#include "control.h"
#include "snap/bearing.h"
#include "snap/cfgprocs.h"
#include "snap/deform.h"
#include "snap/genparam.h"
#include "snap/rftrans.h"
#include "snap/snapglob.h"
#include "snap/stnadj.h"
#include "snap/survfile.h"
#include "snapdata/datatype.h"
#include "snapdata/gpscvr.h"
#include "util/bltmatrx.h"
#include "util/leastsqu.h"
#include "util/chkalloc.h"
#include "util/classify.h"
#include "util/datafile.h"
#include "util/dateutil.h"
#include "util/dstring.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "util/filelist.h"
#include "util/pi.h"
#include "util/readcfg.h"
#include "autofix.h"
#include "coefs.h"
#include "grddeform.h"
#include "lnzdeform.h"
#include "loadsnap.h"
#include "output.h"
#include "reorder.h"
#include "ressumry.h"
#include "rftrnadj.h"
#include "sortobs.h"
#include "stnobseq.h"
#include "testspec.h"
#include "vecdata.h"

#define CONFIG_CMD CFG_USERFLAG1
#define CONSTRAINT_CMD CFG_USERFLAG2

#define COMMENT_CHAR '!'

#define DTP_VELOCITY 1
#define DTP_LINZDEF  2

static int read_program_mode( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_geoid_option( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int process_station_list( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_ignore_missing_stations( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_use_zero_inverse( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_coef( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_rftrans( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_rfscale( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_pb_use_datum_trans( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_set_obs_option( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_use_distance_ratios( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_flag_levels( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_error_type( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_error_summary( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_topocentre( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_gps_vertical( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int load_plot_data( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_station_ordering( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_sort_option( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int set_magic_number( CFG_FILE *cfg, std::string_view string ,void *value, int len, int code );
static int read_configuration_command( CFG_FILE *cfg, std::string_view string ,void *value, int len, int code );
static int read_output_precision( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_residual_format( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_deformation_model(CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_specification_command(CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_spec_test_options(CFG_FILE *cfg, std::string_view string, void *value, int len, int code );

static int config_initialised = 0;
static double dflt_herr, dflt_verr;
static double dflt_rc;

#define FIX_STATIONS    1
#define FREE_STATIONS  2
#define FLOAT_STATIONS  3
#define IGNORE_STATIONS 4
#define REJECT_STATIONS 5
#define ACCEPT_STATIONS 6
#define NOREORDER_STATIONS 7
#define SPEC_TEST 8

#define MODE_HOR 1
#define MODE_VRT 2
#define MODE_AUTO 4

struct station_process_mode
{
    int mode;
    int option;
};

#define INC_COMMAND 0
#define CFG_COMMAND 1
#define CON_COMMAND 2

#define DEFINE_RESIDUAL_COLUMNS 0
#define ADD_RESIDUAL_COLUMNS    1

static config_item snap_commands[] =
{
    {"title",nullptr,CFG_ABSOLUTE,0,read_job_title_command,CFG_REQUIRED+CFG_ONEONLY,0},
    {"mode",&program_mode,CFG_ABSOLUTE,0,read_program_mode,CONFIG_CMD,0},
    {"coordinate_file",NULL,CFG_ABSOLUTE,0,load_coordinate_file,CFG_REQUIRED, 0},
    {"add_coordinate_file",NULL,CFG_ABSOLUTE,0,add_coordinate_file, 0, 0 },
    {"output_coordinate_file",NULL,CFG_ABSOLUTE,0,set_output_coordinate_file,0 , 0},
    {"station_offset_file",NULL,CFG_ABSOLUTE,0,load_offset_file,0, 0},
    {"topocentre",NULL,CFG_ABSOLUTE,0,read_topocentre,CFG_ONEONLY,0},
    {"data_file",NULL,CFG_ABSOLUTE,0,load_data_file,CFG_REQUIRED,0},
    {"geoid",NULL,CFG_ABSOLUTE,0,read_geoid_option,CONFIG_CMD,0},
    {"fix",NULL,CFG_ABSOLUTE,0,process_station_list,CONSTRAINT_CMD,FIX_STATIONS},
    {"free",NULL,CFG_ABSOLUTE,0,process_station_list,CONSTRAINT_CMD,FREE_STATIONS},
    {"float",NULL,CFG_ABSOLUTE,0,process_station_list,CONSTRAINT_CMD,FLOAT_STATIONS},
    {"ignore",NULL,CFG_ABSOLUTE,0,process_station_list,0,IGNORE_STATIONS},
    {"reject",NULL,CFG_ABSOLUTE,0,process_station_list,0,REJECT_STATIONS},
    {"accept",NULL,CFG_ABSOLUTE,0,process_station_list,0,ACCEPT_STATIONS},
    {"horizontal_float_error",&dflt_herr,CFG_ABSOLUTE,0,readcfg_double,CONFIG_CMD | CONSTRAINT_CMD,0},
    {"vertical_float_error",&dflt_verr,CFG_ABSOLUTE,0,readcfg_double,CONFIG_CMD | CONSTRAINT_CMD,0},
    {"ignore_missing_stations",NULL,CFG_ABSOLUTE,0,read_ignore_missing_stations,CONFIG_CMD,0},
    {"recode",NULL,CFG_ABSOLUTE,0,read_recode_command,0,0},
    {"max_iterations",&max_iterations,CFG_ABSOLUTE,0,readcfg_int,CONFIG_CMD,0},
    {"min_iterations",&min_iterations,CFG_ABSOLUTE,0,readcfg_int,CONFIG_CMD,0},
    {"max_adjustment",&max_adjustment,CFG_ABSOLUTE,0,readcfg_double,CONFIG_CMD,0},
    {"convergence_tolerance",&convergence_tol,CFG_ABSOLUTE,0,readcfg_double,CONFIG_CMD,0},
    {"coordinate_precision",&coord_precision,CFG_ABSOLUTE,0,readcfg_int,CONFIG_CMD,0},
    {"reorder_stations",NULL,CFG_ABSOLUTE,0,read_station_ordering,CONFIG_CMD,0},
    {"use_zero_inverse",NULL,CFG_ABSOLUTE,0,read_use_zero_inverse,CONFIG_CMD,0},
    {"refraction_coefficient",NULL,CFG_ABSOLUTE,0,read_coef,CONFIG_CMD,PRM_REFCOEF},
    {"distance_scale_error",NULL,CFG_ABSOLUTE,0,read_coef,CONFIG_CMD,PRM_DISTSF},
    {"bearing_orientation_error",NULL,CFG_ABSOLUTE,0,read_coef,CONFIG_CMD,PRM_BRNGREF},
    {"systematic_error",NULL,CFG_ABSOLUTE,0,read_coef,0,PRM_SYSERR},
    {"default_refraction_coefficient",&dflt_rc,CFG_ABSOLUTE,0,readcfg_double,CONFIG_CMD,0},
    {"reference_frame",NULL,CFG_ABSOLUTE,0,read_rftrans,CONFIG_CMD,0},
    {"reference_frame_scale_error",NULL,CFG_ABSOLUTE,0,read_rfscale,CFG_ONEONLY,0},
    {"proj_bearing_use_datum",NULL,CFG_ABSOLUTE,0,read_pb_use_datum_trans,CONFIG_CMD,0},
    {"set_observation_option",NULL,CFG_ABSOLUTE,0,read_set_obs_option,0,0},
    {"output",NULL,CFG_ABSOLUTE,0,read_output_options,CONFIG_CMD,LIST_OPTIONS},
    {"print",NULL,CFG_ABSOLUTE,0,read_output_options,CONFIG_CMD,LIST_OPTIONS},
    {"list",NULL,CFG_ABSOLUTE,0,read_output_options,CONFIG_CMD,LIST_OPTIONS},
    {"output_csv",NULL,CFG_ABSOLUTE,0,read_output_options,CONFIG_CMD,CSV_OPTIONS},
    {"define_residual_format",NULL,CFG_ABSOLUTE,0,read_residual_format,CONFIG_CMD,DEFINE_RESIDUAL_COLUMNS},
    {"define_residual_columns",NULL,CFG_ABSOLUTE,0,read_residual_format,CONFIG_CMD,DEFINE_RESIDUAL_COLUMNS},
    {"add_residual_column",NULL,CFG_ABSOLUTE,0,read_residual_format,CONFIG_CMD,ADD_RESIDUAL_COLUMNS},
    {"output_precision",NULL,CFG_ABSOLUTE,0,read_output_precision,CONFIG_CMD,0},
    {"classification",NULL,CFG_ABSOLUTE,0,read_classification_command,0,0},
    {"reweight_observations",NULL,CFG_ABSOLUTE,0,read_obs_modification_command,0,OBS_MOD_REWEIGHT},
    {"reject_observations",NULL,CFG_ABSOLUTE,0,read_obs_modification_command,0,OBS_MOD_REJECT},
    {"ignore_observations",NULL,CFG_ABSOLUTE,0,read_obs_modification_command,0,OBS_MOD_IGNORE},
    {"gps_antenna_height",NULL,CFG_ABSOLUTE,0,read_obs_modification_command,0,OBS_MOD_ANTENNA_OFFSET},
    {"sort_observations",NULL,CFG_ABSOLUTE,0,read_sort_option,CONFIG_CMD,0},
    {"summarize_errors_by",NULL,CFG_ABSOLUTE,0,read_error_summary,CONFIG_CMD,0},
    {"summarise_errors_by",NULL,CFG_ABSOLUTE,0,read_error_summary,CONFIG_CMD,0},
    {"summarize_residuals_by",NULL,CFG_ABSOLUTE,0,read_error_summary,CONFIG_CMD,0},
    {"summarise_residuals_by",NULL,CFG_ABSOLUTE,0,read_error_summary,CONFIG_CMD,0},
    {"number_of_worst_residuals",&maxworst,CFG_ABSOLUTE,0,readcfg_int,CONFIG_CMD,0},
    {"station_code_width",&stn_name_width,CFG_ABSOLUTE,0,readcfg_int,CONFIG_CMD,0},
    {"file_location_frequency",&file_location_frequency,CFG_ABSOLUTE,0,readcfg_int,CONFIG_CMD,0},
    {"flag_significance",NULL,CFG_ABSOLUTE,0,read_flag_levels,CONFIG_CMD,0},
    {"mde_power",&mde_power,CFG_ABSOLUTE,0,readcfg_double,CONFIG_CMD,0},
    {"redundancy_flag_level",&redundancy_flag_level,CFG_ABSOLUTE,0,readcfg_double,CONFIG_CMD,0},
    {"error_type",NULL,CFG_ABSOLUTE,0,read_error_type,CONFIG_CMD,0},
    {"gps_vertical",NULL,CFG_ABSOLUTE,0,read_gps_vertical,CONFIG_CMD,0},
    {"use_distance_ratios",NULL,CFG_ABSOLUTE,0,read_use_distance_ratios,CONFIG_CMD,0},
    {"deformation",NULL,CFG_ABSOLUTE,0,read_deformation_model,0,CFG_ONEONLY},
    {"plot",NULL,CFG_ABSOLUTE,0,load_plot_data,0,0},
    {"magic_number",NULL,CFG_ABSOLUTE,0,set_magic_number,CONFIG_CMD,0},
    {"configuration",NULL,CFG_ABSOLUTE,0,read_configuration_command,0,CFG_COMMAND},
    {"include",NULL,CFG_ABSOLUTE,0,read_configuration_command,0,INC_COMMAND},
    {"include",NULL,CFG_ABSOLUTE,0,read_configuration_command,CONSTRAINT_CMD,CON_COMMAND},
    {"test_specification",NULL,CFG_ABSOLUTE,0,process_station_list,0,SPEC_TEST},
    {"specification",NULL,CFG_ABSOLUTE,0,read_specification_command,CONFIG_CMD,0},
    {"spec_test_options",NULL,CFG_ABSOLUTE,0,read_spec_test_options,CONFIG_CMD,0},
    {NULL}
};

static void initialise_config( void )
{
    if( config_initialised ) return;
    stations_read = 0;
    dflt_herr = dflt_verr = 1.0;
    dflt_rc = DEFAULT_REFCOEF;
    initialise_config_items( snap_commands );
    config_initialised = 1;
}


int read_command_file( const std::string &fname )
{
    CFG_FILE *cfg;

    int sts;

    initialise_config();

    cfg = open_config_file( fname, COMMENT_CHAR );

    if(cfg)
    {
        record_filename(get_config_filename(cfg),"command");
        set_config_read_options( cfg, CFG_CHECK_MISSING | CFG_SET_PATH );
        set_config_ignore_flag( cfg, CONSTRAINT_CMD );
        sts = read_config_file( cfg, snap_commands );
        close_config_file( cfg );
        sts = sts ? INVALID_DATA : OK;
    }
    else
    {
        sts = FILE_OPEN_ERROR;
    }

    set_default_refcoef( dflt_rc );

    return sts;
}

int read_command_file_constraints( const std::string &fname )
{
    CFG_FILE *cfg;
    int sts;

    cfg = open_config_file( fname, COMMENT_CHAR );
    if(cfg)
    {
        set_config_read_options( cfg, CFG_IGNORE_BAD | CFG_SET_PATH );
        set_config_command_flag( cfg, CONSTRAINT_CMD );
        sts = read_config_file( cfg, snap_commands );
        close_config_file( cfg );
        sts = sts ? INVALID_DATA : OK;
    }
    else
    {
        sts = FILE_OPEN_ERROR;
    }

    return sts;
}


static int process_configuration_file( const std::string &file_name, const char cfg_only )
{
    CFG_FILE *cfg;
    int sts;

    if( ! path_exists( file_name ) ) return FILE_OPEN_ERROR;

    initialise_config();
    cfg = open_config_file( file_name, COMMENT_CHAR );
    if( cfg )
    {
        record_filename(get_config_filename(cfg),"configuration");
        set_config_read_options( cfg,  CFG_SET_PATH );
        if( cfg_only ) set_config_command_flag( cfg, CONFIG_CMD );
        else set_config_ignore_flag( cfg, CONSTRAINT_CMD );
        sts = read_config_file( cfg, snap_commands );
        close_config_file( cfg );
        sts = sts ? INVALID_DATA : OK;
    }
    else
    {
        sts = FILE_OPEN_ERROR;
    }
    return sts;
}

int read_configuration_file( const std::string &file_name )
{
    return process_configuration_file( file_name, 1 );
}


std::optional<std::string> find_configuration_file( const std::string &name )
{
    return find_file( name, DFLTCONFIG_EXT, std::nullopt, FF_TRYPROJECT, SNAP_CONFIG_SECTION );
}

int process_default_configuration( void )
{
    int sts, sts1;
    std::string spec;
    sts = OK;
    spec=build_config_filespec(system_config_dir(),false,SNAP_CONFIG_SECTION, SNAP_CONFIG_FILE, "" );
    if( path_exists( spec ))
    {
        sts = read_configuration_file( spec );
    }

    if( auto userdir = user_config_dir() )
    {
        spec=build_config_filespec(*userdir,false,SNAP_CONFIG_SECTION, SNAP_CONFIG_FILE, "" );
        if( path_exists( spec ))
        {
            sts1 = read_configuration_file( spec );
            if( sts == OK ) sts = sts1;
        }
    }

    spec=build_config_filespec( command_file->path, true, "", SNAP_CONFIG_FILE, "" );
    if( path_exists( spec ))
    {
        sts1 = read_configuration_file( spec );
        if( sts == OK ) sts = sts1;
    }
    return sts;
}


// #pragma warning( disable : 4100 )

static int read_program_mode( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    bool dimset = false;
    bool modeset = false;
    int sts = OK;

    FieldScanner scanner(string);
    for( auto opt = scanner.next(); sts == OK && opt; opt = scanner.next() )
    {
        if( boost::algorithm::iequals(*opt,"horizontal") || boost::algorithm::iequals(*opt,"2d") )
        {
            dimension = 2;
            if( dimset ) sts = INVALID_DATA;
            dimset = true;
        }
        else if( boost::algorithm::iequals(*opt,"vertical") || boost::algorithm::iequals(*opt,"1d") )
        {
            dimension = 1;
            if( dimset ) sts = INVALID_DATA;
            dimset = true;
        }
        else if( boost::algorithm::iequals(*opt,"3d") )
        {
            dimension = 3;
            if( dimset ) sts = INVALID_DATA;
            dimset = true;
        }
        else if( boost::algorithm::iequals(*opt,"preanalysis") ||
                 boost::algorithm::iequals(*opt,"network_analysis") )
        {
            program_mode = PREANALYSIS;
            if( modeset ) sts = INVALID_DATA;
            modeset = true;
        }
        else if( boost::algorithm::iequals(*opt,"adjustment") )
        {
            program_mode = ADJUST;
            if( modeset ) sts = INVALID_DATA;
            modeset = true;
        }
        else if( boost::algorithm::iequals(*opt,"data_check") )
        {
            program_mode = DATA_CHECK;
            if( modeset ) sts = INVALID_DATA;
            modeset = true;
        }
        else if( boost::algorithm::iequals(*opt,"data_consistency") ||
                 boost::algorithm::iequals(*opt,"free_net_adjustment") )
        {
            program_mode = DATA_CONSISTENCY;
            if( modeset ) sts = INVALID_DATA;
            modeset = true;
        }
        else
        {
            sts = INVALID_DATA;
        }
    }
    if( sts == INVALID_DATA )
    {
        send_config_error( cfg, INVALID_DATA, "Adjustment mode is not correctly defined");
    }
    return OK;
}

static int read_geoid_option( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    FieldScanner scanner(string);
    auto opt = scanner.next();
    if( ! opt )
    {
        send_config_error( cfg, INVALID_DATA, "Geoid command requires a filename or option");
        return OK;
    }

    geoid_file = std::nullopt;
    overwrite_geoid = 0;
    geoid_error_level = WARNING_ERROR;

    if( ! boost::algorithm::iequals(*opt,"none") )
    {
        geoid_file = std::string(*opt);
        opt = scanner.next();
        while( opt )
        {
            if( boost::algorithm::iequals(*opt,"overwrite") )
            {
                overwrite_geoid = 1;
            }
            else if( boost::algorithm::iequals(*opt,"warn_errors") )
            {
                geoid_error_level = INFO_ERROR;
            }
            else if( boost::algorithm::iequals(*opt,"ignore_errors") )
            {
                geoid_error_level = OK;
            }
            else
            {
                break;
            }
            opt = scanner.next();
        }
    }
    else
    {
        opt = scanner.next();
    }
    if( opt )
    {
        send_config_error(cfg, INVALID_DATA, "Geoid command is not correct");
    }
    return OK;
}

static void set_station_mode( station *st, void *modep )
{
    stn_adjustment *sa;
    station_process_mode *spm = (station_process_mode *)modep;

    int mode = spm->mode;
    int fixhor = spm->option & MODE_HOR;
    int fixver = spm->option & MODE_VRT;
    int fixauto = spm->option & MODE_AUTO ? 1 : 0;

    if( mode == SPEC_TEST )
    {
        int istn = find_station(net,st->Code);
        set_station_spec_testid( istn, spm->option, 1 );
        return;
    }

    if( fixauto )
    {
        int istn = find_station(net,st->Code);
        int fixflags=station_autofix_constraints( istn );
        if( ! (fixflags & AUTOFIX_HOR ) ) fixhor=0;
        if( ! (fixflags & AUTOFIX_VRT ) ) fixver=0;
        if( ! (fixhor | fixver ) ) return;
    }

    sa = stnadj(st);

    switch( mode )
    {

    case REJECT_STATIONS: sa->flag.rejected = 1; sa->flag.ignored = 0; break;

    case ACCEPT_STATIONS: sa->flag.rejected = 0; sa->flag.ignored = 0; break;

    case IGNORE_STATIONS: sa->flag.rejected = 1; sa->flag.ignored = 1; break;

    case NOREORDER_STATIONS:  sa->flag.noreorder = 1; break;

    case FIX_STATIONS:    
        if(fixhor) 
        { 
            if( sa->flag.adj_h ) sa->flag.auto_h=fixauto; 
            sa->flag.adj_h = 0; 
            sa->flag.float_h=0; 
        }
        if(fixver) 
        { 
            if( sa->flag.adj_v ) sa->flag.auto_v=fixauto; 
            sa->flag.adj_v = 0; 
            sa->flag.float_v=0; 
        }
        break;

    case FREE_STATIONS:  
        if(fixhor) { sa->flag.adj_h = 1; sa->flag.float_h = 0; sa->flag.auto_h=0; }
        if(fixver) { sa->flag.adj_v = 1; sa->flag.float_v = 0; sa->flag.auto_v=0; }
        break;

    case FLOAT_STATIONS:  
        if( sa->idcol )
        {
            char errmsg[80+STNCODELEN*2];
            sprintf(errmsg,"Cannot float station %s - already co-located with %s",
                    st->Code,stnptr(sa->idcol)->Code);
            handle_error(INFO_ERROR,errmsg,NO_MESSAGE);
        }
        else
        {
            if(fixhor) 
            { 
                if( (fixauto && ! sa->flag.float_h) || ! fixauto )
                {
                    sa->flag.auto_h=fixauto;
                    sa->flag.adj_h=1;
                    sa->flag.float_h=1;
                    sa->herror=(float) dflt_herr;
                }
            }
            if(fixver) 
            { 
                if( (fixauto && ! sa->flag.float_v) || ! fixauto )
                {
                    sa->flag.auto_v=fixauto;
                    sa->flag.adj_v=1;
                    sa->flag.float_v=1;
                    sa->verror=(float) dflt_verr;
                }
            }
        }
        break;
    }
}


static int process_station_list( CFG_FILE *cfg, std::string_view string, void *, int, int mode )
{
    station_process_mode spm;

    if( ! stations_read )
    {
        send_config_error(cfg,INVALID_DATA,
                          "Stations cannot be fixed, floated, or rejected before the station file is loaded");
        return OK;
    }

    spm.mode = mode;
    spm.option = MODE_HOR | MODE_VRT;

    FieldScanner scanner(string);
    // listStart always holds the remainder as it stood right before the
    // most recent scanner.next() call, so at any point it equals
    // "field's own text, plus everything after it, to the end of input" -
    // reassigned in lockstep with field throughout this function.
    std::string_view listStart = scanner.remainder();
    auto field = scanner.next();

    if( mode == SPEC_TEST )
    {

        if( ! field )
        {
            send_config_error(cfg,INVALID_DATA,"Test class name missing");
            return OK;
        }
        if( get_spec_testid( *field, &(spm.option) ) != OK )
        {
            send_config_error(cfg,INVALID_DATA,"Invalid  test class name");
            return OK;
        }
        listStart = scanner.remainder();
        field = scanner.next();
    }

    // allStart is checkpointed right before testing for "all", so the
    // eventual station-list span can be re-anchored to include "all"
    // itself when it matches - matching the original's allptr, which
    // pointed at "all"'s own start (and consequently at everything from
    // "all" onward, including any further keywords consumed below, ends up
    // handed to process_selected_stations verbatim - a real, if unusual,
    // quirk of the original preserved exactly, not second-guessed here).
    const std::string_view allStart = listStart;
    bool haveAll = false;
    if( field && boost::algorithm::iequals( *field, "all" ) )
    {
        haveAll = true;
        listStart = scanner.remainder();
        field = scanner.next();
    }

    if( field && (
                mode == FLOAT_STATIONS ||
                mode == FIX_STATIONS   ||
                mode == FREE_STATIONS    ) )
    {
        spm.option = 0;
        if( mode != FREE_STATIONS && boost::algorithm::iequals( *field, "automatically") )
        {
            spm.option |= MODE_AUTO;
            listStart = scanner.remainder();
            field = scanner.next();
        }
        // The original dereferenced field here unguarded, even though it
        // could be null after consuming "automatically" with nothing
        // following - a latent null-deref on master. Guarded here instead,
        // falling through to the same default (MODE_HOR|MODE_VRT) a
        // genuinely-absent trailing keyword would produce anyway.
        if( field && boost::algorithm::iequals( *field, "horizontal") )
        {
            spm.option |= MODE_HOR;
            listStart = scanner.remainder();
            field = scanner.next();
        }
        else if( field && boost::algorithm::iequals( *field, "vertical") )
        {
            spm.option |= MODE_VRT;
            listStart = scanner.remainder();
            field = scanner.next();
        }
        else if( field && boost::algorithm::iequals( *field, "3d") )
        {
            spm.option |= (MODE_HOR | MODE_VRT);
            listStart = scanner.remainder();
            field = scanner.next();
        }
        else
        {
            spm.option |= (MODE_HOR | MODE_VRT);
        }
    }

    if( haveAll && ! field )
    {
        int istn;
        for( istn = number_of_stations(net); istn; istn-- )
        {
            set_station_mode( stnptr(istn),&spm );
        }
    }

    else if( field )
    {
        int nerr;
        const std::string stationList( haveAll ? allStart : listStart );

        /* Set up error handler so that errors can be attributed to configuration file */
        set_error_location( get_config_location(cfg) );
        nerr = get_error_count();

        process_selected_stations( net,stationList,cfg->name,&spm,set_station_mode);

        set_error_location(NO_MESSAGE);
        cfg->errcount += (get_error_count()-nerr);
    }

    return OK;
}


static int read_ignore_missing_stations( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    int first=1;
    FieldScanner scanner(string);
    for( auto opt = scanner.next(); opt; opt = scanner.next() )
    {
        if( first )
        {
            unsigned char ignoremissing=0;
            first=0;
            int sts=readcfg_boolean(cfg,*opt,&ignoremissing,1,0);
            if( sts == OK )
            {
                set_ignore_missing_stations( ignoremissing );
                continue;
            }
        }
        if( boost::algorithm::iequals(*opt,"report_all") )
        {
            set_report_missing_stations(REPORT_MISSING_ALL);
            continue;
        }
        if( boost::algorithm::iequals(*opt,"report_none") )
        {
            set_report_missing_stations(REPORT_MISSING_NONE);
            continue;
        }
        if( boost::algorithm::iequals(*opt,"report_unlisted") )
        {
            set_report_missing_stations(REPORT_MISSING_UNLISTED);
            continue;
        }
        set_accept_missing_station( *opt );
    }
    if( first )
    {
        set_ignore_missing_stations( 1 );
    }

    return OK;
}

static int read_use_zero_inverse( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    unsigned char option;
    int sts;
    option = 0;
    if( string.empty() )
    {
        option = 1;
        sts = OK;
    }
    else
    {
        sts = readcfg_boolean( cfg, string, &option, 1, 0 );
    }
    if( sts == OK ) lsq_set_use_zero_inverse( option );
    return sts;
}

static int read_station_ordering( CFG_FILE *cfg, std::string_view string, void *value, int len, int )
{
    FieldScanner scanner(string);
    for( auto opt = scanner.next(); opt; opt = scanner.next() )
    {
        if( boost::algorithm::iequals(*opt,"on") )
        {
            reorder_stations = FORCE_REORDERING;
        }
        else if ( boost::algorithm::iequals(*opt,"off") )
        {
            reorder_stations = SKIP_REORDERING;
        }
        else if( boost::algorithm::iequals(*opt,"except") && ! scanner.remainder().empty() )
        {
            if( !stations_read )
            {
                send_config_error( cfg, INVALID_DATA,
                                   "Cannot specify which stations not to reorder before station_file command");
                break;
            }
            else
            {
                process_station_list( cfg, scanner.remainder(), value, len, NOREORDER_STATIONS );
                reorder_stations = FORCE_REORDERING;
                break;
            }
        }
        else
        {
            send_config_error(cfg,INVALID_DATA,"Invalid option in reorder_stations command");
            break;
        }
    }
    return OK;
}


static int read_coef( CFG_FILE *, std::string_view string, void *, int, int code )
{
    int sts, use, calculate;
    double rc;

    sts = OK;

    rc = code == PRM_REFCOEF ? dflt_rc : 0.0;
    calculate = 0;
    use = 0;

    FieldScanner scanner(string);
    auto rcname = scanner.next();

    if( rcname )
    {
        if( code != PRM_SYSERR && boost::algorithm::iequals(*rcname, "use" ) )
        {
            use = 1;
            rcname = scanner.next();
        }
        else if( boost::algorithm::iequals( *rcname, "calculate" ) )
        {
            calculate = 1;
            rcname = scanner.next();
        }
    }

    if( !rcname ) sts = MISSING_DATA;

    auto st = scanner.next();

    if( use )
    {
        if( sts == OK )
        {
            switch( code )
            {
            case PRM_BRNGREF: set_coef_class( COEF_CLASS_BRNGREF, *rcname ); break;
            case PRM_DISTSF:  set_coef_class( COEF_CLASS_DISTSF, *rcname ); break;
            case PRM_REFCOEF: set_coef_class( COEF_CLASS_REFCOEF, *rcname ); break;
            }
        }
        if( st ) sts = INVALID_DATA;
        return sts;
    }

    if( !calculate && st && *st == "=" )
    {
        st = scanner.next();
        if( st )
        {
            configure_param_match( code, *rcname, *st );
        }
        else
        {
            sts = MISSING_DATA;
        }
    }
    else if ( calculate || st )
    {
        if( st )
        {
            auto value = parse_leading<double>(*st);
            if( !value ) sts = INVALID_DATA; else rc = *value;
            st = scanner.next();
            if( st && *st == "?" ) calculate = 1;
        }
        if( sts == OK ) configure_param( code, *rcname, rc, calculate );
    }
    else
    {
        sts = MISSING_DATA;
    }
    return sts;
}


static int read_rfscale( CFG_FILE *, std::string_view string, void *, int, int )
{
    double scale = 0.0;
    int calculate = 0;

    FieldScanner scanner(string);
    for( auto st = scanner.next(); st; st = scanner.next() )
    {
        if( boost::algorithm::iequals(*st,"calculate") || *st == "?" )
        {
            calculate = 1;
        }
        else
        {
            auto value = parse_leading<double>(*st);
            if( !value ) return INVALID_DATA;
            scale = *value;
        }
    }

    init_rf_scale_error( scale, calculate );
    return OK;
}


static int read_rftrans( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    std::optional<std::string_view> prmname, valuetype;
    int rfid;
    rfTransformation *rf;
    double val[14];
    int calcval[14];
    int defined[14];
    double date;
    int i, nval, ival;
    int sts;
    int calculate;
    int topocentric;
    int origintype=REFFRM_ORIGIN_DEFAULT;
    int iers;
    char errmess[256];
    int first;

    calculate = 0;
    topocentric = 0;
    for( i = 0; i < 14; i++ )
    {
        val[i]=0.0;
        defined[i]=0;
        calcval[i]=0;
    }
    date=UNDEFINED_DATE;
    sts=OK;
    iers=0;

    /* Process to handle the calculate, geocentric/topocentric, and name
       fields */

    sts = OK;
    first = 1;

    FieldScanner scanner(string);
    // pending is a one-slot hold-and-check cache shared by both the outer
    // keyword loop and the inner value loop below, matching the original's
    // shared prmread flag: fetchPrmname() returns whatever's cached there
    // and clears it if something's waiting, otherwise it pulls a fresh
    // token from the scanner. Which of the two loops last populated it
    // doesn't matter - whichever calls fetchPrmname() next just gets it.
    std::optional<std::string_view> pending;
    auto fetchPrmname = [&]() -> std::optional<std::string_view>
    {
        if( pending ) { auto p = pending; pending = std::nullopt; return p; }
        return scanner.next();
    };

    auto rfnameField = scanner.next();
    if( ! rfnameField )
    {
        send_config_error( cfg, MISSING_DATA, "Reference frame command requires a name" );
        return OK;
    }
    const std::string rfnameStr(*rfnameField);

    if( boost::algorithm::iequals(*rfnameField,"use") )
    {
        prmname = scanner.next();
        if( prmname )
        {
            set_coef_class( COEF_CLASS_REFFRM, *prmname );
            if( scanner.next() ) sts = INVALID_DATA;
        }
        else
        {
            sts = MISSING_DATA;
        }
        return sts;
    }

    for( prmname = fetchPrmname(); prmname; prmname = fetchPrmname() )
    {
        nval=0;
        ival=0;
        valuetype=prmname;
        if( boost::algorithm::iequals( *prmname, "calculate" ) )
        {
            calculate=1;
        }
        else if( boost::algorithm::iequals( *prmname, "topocentric" ) )
        {
            topocentric=1;
        }
        else if( boost::algorithm::iequals( *prmname, "geocentric" ) )
        {
            topocentric=0;
        }
        else if( boost::algorithm::iequals( *prmname, "epoch" ) )
        {
            prmname=scanner.next();
            if( ! prmname )
            {
                sprintf(errmess,"Missing epoch date for reference frame %.20s",rfnameStr.c_str());
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }
            date=snap_datetime_parse( *prmname );
            if( date == UNDEFINED_DATE )
            {
                sprintf(errmess,"Invalid epoch date %.20s for reference frame %.20s",
                        std::string(*prmname).c_str(),rfnameStr.c_str());
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }
        }
        else if( boost::algorithm::iequals( *prmname, "origin" ) )
        {
            prmname=scanner.next();
            if( prmname && boost::algorithm::iequals(*prmname,"zero") ) origintype=REFFRM_ORIGIN_ZERO;
            else if( prmname && *prmname=="0" ) origintype=REFFRM_ORIGIN_ZERO;
            else if( prmname && boost::algorithm::iequals(*prmname,"topocentre") ) origintype=REFFRM_ORIGIN_TOPOCENTRE;
            else if( prmname && boost::algorithm::iequals(*prmname,"default") ) origintype=REFFRM_ORIGIN_DEFAULT;
            else
            {
                sprintf(errmess,"Invalid origin type %.20s for reference frame %.20s",
                        prmname?std::string(*prmname).c_str():"",rfnameStr.c_str());
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }
        }
        else if( boost::algorithm::iequals( *prmname, "translation" ) )
        {
            ival=rfTx;
            nval=3;
        }
        else if( boost::algorithm::iequals( *prmname, "translation_rate" ) )
        {
            ival=rfTxRate;
            nval=3;
        }
        else if( boost::algorithm::iequals( *prmname, "scale" ) )
        {
            ival=rfScale;
            nval=1;
        }
        else if( boost::algorithm::iequals( *prmname, "scale_rate" ) )
        {
            ival=rfScaleRate;
            nval=1;
        }
        else if( boost::algorithm::iequals( *prmname, "rotation" ) )
        {
            ival=rfRotx;
            nval=3;
        }
        else if( boost::algorithm::iequals( *prmname, "rotation_rate" ) )
        {
            ival=rfRotxRate;
            nval=3;
        }
        else if( boost::algorithm::iequals( *prmname, "iers_tsr" ) )
        {
            ival=rfTx;
            nval=7;
            iers=1;
        }
        else if( boost::algorithm::iequals( *prmname, "iers_etsr" ) )
        {
            prmname=scanner.next();
            if( ! prmname )
            {
                sprintf(errmess,"Missing IERS_ETSR epoch date for reference frame %.20s",rfnameStr.c_str());
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }
            date=snap_datetime_parse( *prmname );
            if( date == UNDEFINED_DATE )
            {
                sprintf(errmess,"Invalid IERS_ETSR epoch date %.20s for reference frame %.20s",
                        std::string(*prmname).c_str(),rfnameStr.c_str());
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }
            ival=rfTx;
            nval=14;
            iers=1;
        }
        else
        {
            sprintf(errmess,"Invalid parameter %.20s for reference frame %.20s command",
                    std::string(*prmname).c_str(), rfnameStr.c_str());
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }

        first=1;
        for( ; nval; nval--, ival++, first=0 )
        {
            prmname=fetchPrmname();
            if( ! prmname )
            {
                if( first )
                {
                    while( nval--) calcval[ival++]=calculate;
                    break;
                }
                sprintf(errmess,"Missing %.20s value for reference frame %.20s command",
                        std::string(*valuetype).c_str(),rfnameStr.c_str() );
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }

            calcval[ival]=calculate;
            auto parsed = parse_leading_field<double>(*prmname);
            if( ! parsed )
            {
                if( first )
                {
                    while( nval--) calcval[ival++]=calculate;
                    pending=prmname;
                    break;
                }
                sprintf(errmess,"Invalid %.20s value %.20s for reference frame %.20s command",
                        std::string(*valuetype).c_str(), std::string(*prmname).c_str(), rfnameStr.c_str());
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }
            const std::string_view suffix = prmname->substr( parsed->result.ptr - prmname->data() );
            if( ! suffix.empty() )
            {
                if( suffix == "?" )
                {
                    calcval[ival]=1;
                }
                else
                {
                    sprintf(errmess,"Invalid %.20s value %.20s for reference frame %.20s command",
                            std::string(*valuetype).c_str(), std::string(*prmname).c_str(), rfnameStr.c_str());
                    send_config_error( cfg, INVALID_DATA, errmess );
                    return OK;
                }
            }
            else
            {
                prmname=scanner.next();
                if( prmname && *prmname == "?" )
                {
                    calcval[ival]=1;
                }
                else
                {
                    pending=prmname;
                }
            }
            if( defined[ival] )
            {
                sprintf(errmess,"Duplicated %.20s value definition for reference frame %.20s command",
                        std::string(*valuetype).c_str(), rfnameStr.c_str());
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }
            defined[ival]=1;
            val[ival]=parsed->value;
        }
    }

    if( iers )
    {
        if( topocentric )
        {
            sprintf(errmess,"Ref frame %s cannot be defined with IERS parameters and topocentric",rfnameStr.c_str());
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }
        for( i=0; i<14; i++) val[i]=0.001*val[i];
        for( i=0; i<3; i++ ) { val[i+rfRotx]*=-1.0; val[i+rfRotxRate]*=-1.0; }
    }

    if( topocentric )
    {
        rfid = get_rftrans_id( rfnameStr, REFFRM_TOPOCENTRIC );
        rf=rftrans_from_id( rfid );
        if( ! rf->istopo )
        {
            sprintf(errmess,"Ref frame %s defined as both topocentric and geocentric",rfnameStr.c_str());
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }
    }
    else
    {
        rfid = get_rftrans_id( rfnameStr, iers ? REFFRM_IERS : REFFRM_GEOCENTRIC );
        rf=rftrans_from_id( rfid );
        if( rf->istopo && iers )
        {
            sprintf(errmess,"Topocentric ref frame %s cannot be defined with IERS parameters",rfnameStr.c_str());
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }
    }

    if( origintype != REFFRM_ORIGIN_DEFAULT ) rf->setOriginType( origintype );
    if( date != UNDEFINED_DATE ) rf->setRefDate( date );

    rf->setParameters( val, calcval, defined );
    return OK;
}

namespace {
/// Decodes one residual-format title field from remaining, up to the next
/// un-escaped ':' or end of text (or after 80 decoded characters, matching
/// the original's fixed 80-char title buffer limit) - '_' becomes a space,
/// '\' takes the next character literally, including a colon (so "\:"
/// inserts a literal ':' into the title rather than ending the field - the
/// terminator check only ever looks at the *current*, not-yet-consumed
/// character, and the backslash branch unconditionally consumes the next
/// character as content without re-checking it). Leaves remaining
/// positioned at the ':' that ended the field, or empty if none was found.
/// \return the decoded title, or nullopt if the field was empty.
std::optional<std::string> decodeResidualTitle( std::string_view &remaining )
{
    std::string result;
    std::size_t i = 0;
    for( ; result.size() < 80; i++ )
    {
        if( i >= remaining.size() || remaining[i] == ':' ) break;
        if( remaining[i] == '_' )
        {
            result += ' ';
        }
        else
        {
            if( remaining[i] == '\\' )
            {
                i++;
                if( i >= remaining.size() ) break;
            }
            result += remaining[i];
        }
    }
    remaining.remove_prefix( std::min(i,remaining.size()) );
    if( result.empty() ) return std::nullopt;
    return result;
}
}

static int read_residual_format( CFG_FILE *cfg, std::string_view string, void *, int, int code )
{
    FieldScanner scanner(string);
    auto typesField = scanner.next();
    if( !typesField ) return MISSING_DATA;

    /* If types doesn't define valid types, then assume it is all and
     * use as column definition */

    std::optional<std::string_view> nextcol = typesField;
    if( define_residual_formats( *typesField, code ) == OK )
    {
        nextcol = std::nullopt;
    }

    while( true )
    {
        std::string_view column;
        if( nextcol )
        {
            column = *nextcol;
            nextcol = std::nullopt;
        }
        else
        {
            auto colField = scanner.next();
            if( ! colField ) break;
            column = *colField;
        }

        bool valid = true;
        int width = 0;
        std::optional<std::string> ttl1, ttl2;

        const auto colonPos = column.find(':');
        const std::string_view columnName = colonPos==std::string_view::npos ? column : column.substr(0,colonPos);
        std::string_view remaining = colonPos==std::string_view::npos ? std::string_view() : column.substr(colonPos);

        if( ! remaining.empty() )
        {
            std::string_view afterFirstColon = remaining.substr(1);
            const auto nextColon = afterFirstColon.find(':');
            const std::string_view widthField = nextColon==std::string_view::npos ? afterFirstColon : afterFirstColon.substr(0,nextColon);
            remaining = nextColon==std::string_view::npos ? std::string_view() : afterFirstColon.substr(nextColon);

            if( ! widthField.empty() )
            {
                auto parsedWidth = parse_leading_field<int>(widthField);
                if( ! parsedWidth || parsedWidth->result.ptr != widthField.data()+widthField.size() || parsedWidth->value < 0 )
                {
                    valid = false;
                }
                else
                {
                    width = parsedWidth->value;
                }
            }
        }

        for( int i = 1; i <= 2; i++ )
        {
            /* Reading the titles, only if we have a : separator, then
               a character after it. */
            if( remaining.empty() ) continue;
            remaining.remove_prefix(1);
            if( remaining.empty() ) continue;
            if( i == 1 ) ttl1 = decodeResidualTitle( remaining );
            else ttl2 = decodeResidualTitle( remaining );
        }
        if( valid )
        {
            if( add_residual_field( columnName, width, ttl1, ttl2 ) != OK ) valid = false;
        }
        if( !valid )
        {
            char errmess[100];
            sprintf( errmess, "Invalid column definition %.40s",std::string(column).c_str());
            send_config_error( cfg, INVALID_DATA, errmess );
        }
    }
    return OK;
}


static int read_output_precision( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    FieldScanner scanner(string);
    for( auto st = scanner.next(); st; st = scanner.next() )
    {
        int type;
        auto ndp_str = scanner.next();
        if( !ndp_str )
        {
            send_config_error(cfg,MISSING_DATA,"Missing precision in output_precision command");
            break;
        }
        for( type = 0; type < NOBSTYPE; type++ )
        {
            if( boost::algorithm::iequals(*st,datatype[type].code) ) break;
        }
        if( type == NOBSTYPE )
        {
            char errmess[80];
            sprintf(errmess,"Invalid type code %.20s in output_precision command",std::string(*st).c_str());
            send_config_error( cfg, INVALID_DATA, errmess );
        }
        else if( ndp_str->size() != 1 || !ISDIGIT((*ndp_str)[0]) )
        {
            char errmess[80];
            sprintf(errmess,"Invalid precision %.20s in output_precision command",std::string(*ndp_str).c_str());
            send_config_error( cfg, INVALID_DATA, errmess );
        }
        else
        {
            obs_precision[type] = (*ndp_str)[0] - '0';
        }
    }
    return OK;
}

static int read_sort_option( CFG_FILE *, std::string_view string, void *, int, int )
{
    sort_obs = SORTED_OBS + SORT_BY_LINE;
    FieldScanner scanner(string);

    for( auto s = scanner.next(); s; s = scanner.next() )
    {
        if( boost::algorithm::iequals(*s,"by_line") )
        {
        }
        else if( boost::algorithm::iequals(*s,"by_type") )
        {
            sort_obs |= SORT_BY_TYPE;
        }
        else if( boost::algorithm::iequals(*s,"by_instrument_station") )
        {
            sort_obs &= ~SORT_BY_LINE;
        }
        else
        {
            return INVALID_DATA;
        }
    }

    return OK;
}


static int read_pb_use_datum_trans( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    unsigned char option;
    int sts;
    option = 0;
    if( string.empty() )
    {
        option = 1;
        sts = OK;
    }
    else
    {
        sts = readcfg_boolean( cfg, string, &option, 1, 0 );
    }
    if( sts == OK ) set_bproj_use_datum_transformation( option );
    return sts;
}

static int read_set_obs_option( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    int option;
    std::string selection_prefix;
    int result;
    int set;
    void *obsmod=0;

    FieldScanner scanner(string);
    auto optionField = scanner.next();

    if( ! optionField )
    {
        send_config_error( cfg, INVALID_DATA, "Observation option not specified");
        return OK;
    }
    std::string_view optionstr = *optionField;
    const std::string_view criteria = scanner.remainder();

    set=1;
    if( boost::algorithm::istarts_with( optionstr, "no_" ) )
    {
        set=0;
        optionstr = optionstr.substr(3);
    }

    if( boost::algorithm::iequals(optionstr,"calculate_gx_translation") )
    {
        option=OBS_OPT_CALC_GX_TRANSLATION;
        selection_prefix="data_type=GX";
    }
    else if( boost::algorithm::iequals(optionstr,"use_distance_ratios_as_distances") )
    {
        option=OBS_OPT_CALC_DISTRATIO_AS_DIST;
        selection_prefix="data_type=DR";
    }
    else
    {
        // The original built a real "Invalid observation option ..." message
        // into errmess here but never actually used it below, sending the
        // generic message instead - preserved as-is, not "fixed", since
        // changing which message is sent would be an observable behavior
        // change.
        send_config_error( cfg, INVALID_DATA, "Observation option not specified");
        return OK;
    }

    if( selection_prefix.empty() && criteria.empty() ) selection_prefix="all_observations";
    std::string selection = selection_prefix;
    if( ! selection_prefix.empty() && ! criteria.empty() ) selection += " ";
    if( ! criteria.empty() ) selection += criteria;
    obsmod=snap_obs_modifications(true);
    result=add_obs_option_modification(cfg,obsmod,selection,set,option);
    return result;
}

static int read_use_distance_ratios( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    unsigned char set=0;
    int sts=readcfg_boolean(cfg,string,&set,1,0);
    if( sts == OK )
    {
        void *obsmod=snap_obs_modifications(true);
        add_obs_option_modification(cfg,obsmod,"data_type=DR",set,OBS_OPT_CALC_DISTRATIO_AS_DIST);
    }
    return sts;
}

static int read_flag_levels( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    int nf;

    nf = 0;

    FieldScanner scanner(string);
    for( auto s = scanner.next(); s; s = scanner.next() )
    {
        if( nf > 2 ) { nf = 0; break; }
        if( boost::algorithm::iequals(*s,"maximum") ) { taumax[nf] = 1; continue; }
        auto fl = parse_leading<double>(*s);
        if( !fl || *fl <= 0.0 || *fl >= 100.0 )
        {
            nf = 0;
            break;
        }
        else
        {
            flag_level[nf] = *fl;
            nf++;
        }
    }

    if( !nf )
    {
        send_config_error( cfg, INVALID_DATA, "Invalid or missing data in flag_significance command");
    }
    return OK;
}

static int read_error_type( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    double conf=1.0;
    int useconf=0;

    FieldScanner scanner(string);
    auto fld = scanner.next();
    if( !fld ) return MISSING_DATA;
    if( boost::algorithm::iequals( *fld, "aposteriori" ) )
    {
        apriori = 0;
        fld = scanner.next();
    }
    else if( boost::algorithm::iequals( *fld, "apriori" ) )
    {
        apriori = 1;
        fld = scanner.next();
    }
    else
    {
         send_config_error( cfg, INVALID_DATA, "Expected \"apriori\" or \"aposteriori\"");
         return OK;
    }

    if( fld )
    {
        if( boost::algorithm::iequals(*fld,"standard_error") )
        {
            useconf = 0;
        }
        else
        {
            auto parsedConf = parse_leading<double>(*fld);
            if( ! parsedConf )
            {
                send_config_error( cfg, INVALID_DATA, "Expected \"standard_error\" or \"##.#% confidence_limit\"");
                return OK;
            }
            conf = *parsedConf;
            if( conf <= 0.0 || conf >= 100.0 )
            {
                send_config_error( cfg, INVALID_DATA, "Confidence limit not between 0 and 100");
                return OK;
            }
            fld = scanner.next();
            if( ! fld ) return MISSING_DATA;
            if( boost::algorithm::iequals(*fld,"standard_error") )
            {
                useconf = 0;
            }
            else if( boost::algorithm::iequals(*fld,"confidence_limit") )
            {
                useconf = 1;
            }
            else
            {
                send_config_error( cfg, INVALID_DATA, "Expected \"standard_error\" or \"confidence_limit\"");
            }
        }
    }
    errconflim = useconf;
    errconfval = conf;
    return OK;
}

static int read_error_summary( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    int sts;
    FieldScanner scanner(string);
    for( auto str = scanner.next(); str; str = scanner.next() )
    {
        sts = define_error_summary( std::string(*str) );
        if( sts != OK )
        {
            char errmsg[100];
            sprintf(errmsg,"Invalid error summary definition %.50s",std::string(*str).c_str());
            send_config_error( cfg, INVALID_DATA, errmsg );
        }
    }
    return OK;
}

static int read_topocentre( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    if( ! stations_read )
    {
        send_config_error(cfg,INVALID_DATA,
                          "The topocentre cannot be defined before the station file is loaded");
        return OK;
    }

    FieldScanner scanner(string);
    auto ltField = scanner.next();
    auto lnField = ltField ? scanner.next() : std::nullopt;
    auto lt = ltField ? parse_leading<double>(*ltField) : std::nullopt;
    auto ln = lnField ? parse_leading<double>(*lnField) : std::nullopt;

    if( lt && ln &&
            *lt > -90.0 && *lt < 90.0 && *ln >= -180.0 && *ln <= 180.0 )
    {
        set_network_topocentre( net, *lt*DTOR, *ln*DTOR );
        return OK;
    }

    return INVALID_DATA;
}

static int read_gps_vertical( CFG_FILE *, std::string_view string, void *, int, int )
{

    if( boost::algorithm::iequals(string,"individual") || boost::algorithm::iequals(string,"midpoint") )
    {
        set_gps_vertical_fixed( 0 );
        return OK;
    }

    if( boost::algorithm::iequals(string,"topocentre") )
    {
        set_gps_vertical_fixed( 1 );
        return OK;
    }

    return INVALID_DATA;
}

static int load_plot_data( CFG_FILE *, std::string_view, void *, int, int )
{
    return OK;
}

static int read_deformation_model(CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    double epoch;
    std::optional<std::string> model;
    int type;
    bool first;
    first = true;
    epoch = -1;
    ignore_deformation = 0;
    type = 0;

    FieldScanner scanner(string);
    for( auto itemField = scanner.next(); itemField; itemField = scanner.next() )
    {
        const std::string_view item = *itemField;
        if( first && boost::algorithm::iequals(item,"none") )
        {
            ignore_deformation = 1;
            return OK;
        }
        else if( first && boost::algorithm::iequals(item,"datum") )
        {
            return OK;
        }
        first = false;
        const auto eqPos = item.find('=');
        const std::string_view key = eqPos==std::string_view::npos ? item : item.substr(0,eqPos);
        const std::string_view value = eqPos==std::string_view::npos ? std::string_view() : item.substr(eqPos+1);
        if( boost::algorithm::iequals(key,"type") )
        {
            ignore_deformation = 1;
            if( boost::algorithm::iequals(value,"velocity") || boost::algorithm::iequals(value,"velgrid") )
            {
                type = DTP_VELOCITY;
            }
            else if( boost::algorithm::iequals(value,"linz") || boost::algorithm::iequals(value,"linzdef") )
            {
                type = DTP_LINZDEF;
                if( epoch < 0 ) epoch = 0;
            }
            else if( boost::algorithm::iequals(value,"none") )
            {
                ignore_deformation = 1;
            }
            else
            {
                send_config_error( cfg, INVALID_DATA, "Invalid deformation model type" );
            }
        }
        else if (boost::algorithm::iequals(key,"model") )
        {
            if( ! model )
            {
                model = std::string(value);
            }
            else
            {
                send_config_error( cfg, INVALID_DATA, "Duplicated model in deformation definition" );
            }

        }
        else if (boost::algorithm::iequals(key,"epoch") )
        {
            auto parsedEpoch = parse_leading<double>(value);
            if( ! parsedEpoch )
            {
                send_config_error( cfg, INVALID_DATA, "Invalid epoch in deformation definition" );
            }
            else
            {
                epoch = *parsedEpoch;
            }
        }
    }
    if( epoch < 0 )
    {
        send_config_error( cfg, MISSING_DATA, "Epoch missing in deformation definition");
    }
    else if ( type == 0 )
    {
        send_config_error( cfg, MISSING_DATA, "Type missing in deformation definition");
    }
    else if( ! model )
    {
        send_config_error( cfg, MISSING_DATA, "Model missing in deformation definition");
    }
    else if( type==DTP_VELOCITY && create_grid_deformation( &deformation, *model, epoch ) != OK )
    {
        send_config_error( cfg, INVALID_DATA, "Invalid parameters in grid deformation definition");
    }
    else if( type==DTP_LINZDEF && create_linzdef_deformation( &deformation, *model, epoch ) != OK )
    {
        send_config_error( cfg, INVALID_DATA, "Invalid parameters in LINZ deformation definition");
    }
    return OK;
}


namespace {
/// Matches sscanf's %Ns behavior for this comparison: only the first cap
/// characters of field participate (as a fixed %Ns-sized buffer would only
/// have captured that many) - a no-op unless field is actually longer than
/// cap, which it practically never is for a real keyword here.
bool truncatedIEquals( const std::string_view field, const std::size_t cap, const std::string_view literal )
{
    return boost::algorithm::iequals( field.substr(0,cap), literal );
}
}

static int read_specification_command(CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    int gothortol;
    int gotvertol;
    double conf = 0.0;
    double herrabs, herrppm, herrmax;
    double verrabs, verrppm, verrmax;
    int errdirflg;

    herrabs = herrppm = herrmax = 0.0;
    verrabs = verrppm = verrmax = 0.0;
    gothortol = gotvertol = 0;

    FieldScanner scanner(string);
    auto nameField = scanner.next();
    auto typeField = scanner.next();
    if( ! nameField || ! typeField || ! truncatedIEquals(*typeField,20,"confidence") )
    {
        send_config_error( cfg, INVALID_DATA, "Missing confidence in specification command" );
        return OK;
    }
    const std::string name(*nameField);

    auto confField = scanner.next();
    auto parsedConf = confField ? parse_leading_field<double>(*confField) : std::nullopt;
    std::string_view confSuffix;
    if( parsedConf ) confSuffix = confField->substr( parsedConf->result.ptr - confField->data() );
    if( ! parsedConf || confSuffix.empty() || confSuffix[0] != '%' )
    {
        send_config_error( cfg, INVALID_DATA, "Missing confidence in specification command" );
        return OK;
    }
    conf = parsedConf->value;

    errdirflg = 0;
    while( true )
    {
        int errtypflg = 0;
        int errdir = 0;

        auto dirField = scanner.next();
        if( ! dirField ) break;

        if( truncatedIEquals(*dirField,20,"horizontal") )
        {
            errdir = 1;
            gothortol = 1;
        }
        else if( truncatedIEquals(*dirField,20,"vertical") )
        {
            errdir = 2;
            gotvertol = 1;
        }
        if( ! errdir || (errdir & errdirflg) )
        {
            send_config_error( cfg, INVALID_DATA, "Invalid hor/ver in accuracy specification");
            return OK;
        }
        errdirflg |= errdir;

        while( true )
        {
            // A non-numeric token here isn't an error - it's the next
            // direction keyword (e.g. "vertical") ending this direction's
            // accuracy list, which is the normal one-value-per-direction
            // case. Rewind to the checkpoint so the outer loop re-reads it.
            const std::string_view checkpoint = scanner.remainder();
            auto valField = scanner.next();
            if( ! valField ) break;
            auto parsedErr = parse_leading_field<double>(*valField);
            if( ! parsedErr )
            {
                scanner = FieldScanner(checkpoint);
                break;
            }
            const std::string_view errSuffix = valField->substr( parsedErr->result.ptr - valField->data() );
            const double err = parsedErr->value;
            if( errSuffix.empty() )
            {
                send_config_error(cfg,INVALID_DATA, "Invalid accuracy in specification");
                return OK;
            }

            int errtyp = 0;
            if( truncatedIEquals(errSuffix,7,"MM") )
            {
                errtyp = 4;
            }
            else if( truncatedIEquals(errSuffix,7,"PPM") )
            {
                errtyp = 8;
            }
            else if( truncatedIEquals(errSuffix,7,"MM_ABS") )
            {
                errtyp = 16;
            }
            if( ! errtyp || (errtyp & errtypflg) )
            {
                send_config_error(cfg,INVALID_DATA, "Invalid error type in specification -  must be mm, ppm, or mm_abs");
                return OK;
            }
            switch( errtyp + errdir )
            {
            case 5: herrabs = err; break;
            case 9: herrppm = err; break;
            case 17: herrmax = err; break;
            case 6: verrabs = err; break;
            case 10: verrppm = err; break;
            case 18: verrmax = err; break;
            }

            errtypflg |= errtyp;
            if( errtypflg == 28 ) break;
        }
        if( errtypflg == 0 )
        {
            send_config_error( cfg, INVALID_DATA, "Missing error values in specification");
            return OK;
        }

        if( errdirflg == 3 ) break;
    }



    if( ! errdirflg )
    {
        send_config_error( cfg, INVALID_DATA, "Specification lists neither horizontal nor vertical tolerance");
        return OK;
    }

    if( conf <= 0.0 || conf >= 100.0 ||
            herrabs < 0.0 || herrppm < 0.0 ||
            verrabs < 0.0 || verrppm < 0.0 ||
            herrmax < 0.0 || verrmax < 0.0 )
    {
        send_config_error( cfg, INVALID_DATA, "Invalid confidence or errors in specification");
        return OK;
    }

    if( define_spec(name, conf, gothortol, herrabs/1000, herrppm, herrmax/1000,
                    gotvertol, verrabs/1000, verrppm, verrmax/1000)
            != OK )
    {
        send_config_error( cfg, INVALID_DATA, "Unable to define specification");
    }

    return OK;
}


static int read_spec_test_options(CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    FieldScanner scanner(string);
    for( auto option = scanner.next(); option; option = scanner.next() )
    {
        if( boost::algorithm::iequals(*option,"APRIORI") )
        {
            set_spec_apriori( 1 );
        }
        else if ( boost::algorithm::iequals(*option,"APOSTERIORI") )
        {
            set_spec_apriori( 0 );
        }
        else if ( boost::algorithm::iequals(*option,"LIST_ALL") )
        {
            set_spec_listoption( SPEC_LIST_ALL);
        }
        else if ( boost::algorithm::iequals(*option,"LIST_FAIL") )
        {
            set_spec_listoption( SPEC_LIST_FAIL);
        }
        else if ( boost::algorithm::iequals(*option,"LIST_NONE") )
        {
            set_spec_listoption( SPEC_LIST_NONE);
        }
        else
        {
            send_config_error( cfg, INVALID_DATA, "Invalid option in spec_test_options command");
        }
    }
    return OK;
}


static int set_magic_number( CFG_FILE *, std::string_view string ,void *, int, int )
{
    FieldScanner scanner(string);
    auto idField = scanner.next();
    auto valField = idField ? scanner.next() : std::nullopt;
    auto idOpt = idField ? parse_leading<int>(*idField) : std::nullopt;
    auto valOpt = valField ? parse_leading<double>(*valField) : std::nullopt;
    if( !idOpt || !valOpt ) return INVALID_DATA;
    int id = *idOpt;
    double val = *valOpt;

    switch( id )
    {
    case 1:
    case 2:  id--;
        val *= blt_get_small(id);
        blt_set_small( id, val );
        break;

    default: return INVALID_DATA;
    }

    return OK;
}

static int read_configuration_command( CFG_FILE *cfg, std::string_view string ,void *, int, int code )
{
    char errmsg[60+MAX_FILENAME_LEN];
    char cfg_only;
    char constraint;

    cfg_only = code == CFG_COMMAND;
    constraint = code == CON_COMMAND;

    FieldScanner scanner(string);
    // The original's error messages read the whole `string` pointer, not
    // the current cfgfile - but by the time any error is reported, the
    // first strtokq call has already null-terminated that buffer right
    // after the first filename, so on a multi-file include line every
    // error message actually shows only the first filename, never
    // whichever one failed. Preserved exactly via firstFile below.
    std::optional<std::string> firstFile;
    while( auto cfgfileField = scanner.checkAndRecoverQuotedValue( true,
                std::vector<QuoteFollowOption>{QuoteFollowOption::Whitespace,QuoteFollowOption::End} ) )
    {
        const std::string cfgfile(*cfgfileField);
        if( ! firstFile ) firstFile = cfgfile;
        auto resolved = find_file( cfgfile, cfg_only ? DFLTCONFIG_EXT : DFLTCOMMAND_EXT,
                             std::nullopt, FF_TRYPROJECT,
                             cfg_only ? SNAP_CONFIG_SECTION : "" );
        if( resolved )
        {
            int status = constraint ? read_command_file_constraints( *resolved ) : process_configuration_file( *resolved, cfg_only );
            if( status != OK )
            {
                sprintf(errmsg,"Invalid data in configuration file %.*s",MAX_FILENAME_LEN,firstFile->c_str());
                send_config_error(cfg,INVALID_DATA,errmsg);
            }
        }
        else
        {
            sprintf(errmsg,"Cannot find configuration file %.*s",MAX_FILENAME_LEN,firstFile->c_str());
            send_config_error( cfg, INVALID_DATA, errmsg );
        }
    }
    return OK;
}

