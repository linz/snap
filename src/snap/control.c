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
#include <ctype.h>

#include "control.h"
#include "snap/cfgprocs.h"
#include "snap/deform.h"
#include "snap/rftrans.h"
#include "snap/stnadj.h"
#include "snap/survfile.h"
#include "snapdata/datatype.h"
#include "snapdata/gpscvr.h"
#include "util/bltmatrx.h"
#include "util/chkalloc.h"
#include "util/classify.h"
#include "util/datafile.h"
#include "util/dateutil.h"
#include "util/dstring.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "util/pi.h"
#include "util/readcfg.h"
#include "coefs.h"
#include "grddeform.h"
#include "lnzdeform.h"
#include "loadsnap.h"
#include "output.h"
#include "reorder.h"
#include "ressumry.h"
#include "rftrnadj.h"
#include "snapmain.h"
#include "sortobs.h"
#include "stnobseq.h"
#include "testspec.h"
#include "vecdata.h"

#define CONFIG_CMD CFG_USERFLAG1
#define CONSTRAINT_CMD CFG_USERFLAG2

#define COMMENT_CHAR '!'

#define DTP_VELOCITY 1
#define DTP_LINZDEF  2

static int read_program_mode( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_geoid_option( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int process_station_list( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_recode( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_ignore_missing_stations( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_coef( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_classification( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_rftrans( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_rfscale( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_output_options( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_flag_levels( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_error_type( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_error_summary( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_topocentre( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_gps_vertical( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int load_plot_data( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_station_ordering( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_sort_option( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int set_magic_number( CFG_FILE *cfg, char *string ,void *value, int len, int code );
static int read_configuration_command( CFG_FILE *cfg, char *string ,void *value, int len, int code );
static int read_output_precision( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_residual_format( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_deformation_model(CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_specification_command(CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_spec_test_options(CFG_FILE *cfg, char *string, void *value, int len, int code );

static int config_initiallized = 0;
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

typedef struct
{
    int mode;
    int option;
} station_process_mode;

#define INC_COMMAND 0
#define CFG_COMMAND 1
#define CON_COMMAND 2

#define DEFINE_RESIDUAL_COLUMNS 0
#define ADD_RESIDUAL_COLUMNS    1

static config_item snap_commands[] =
{
    {"title",job_title,ABSOLUTE,JOBTITLELEN,STORE_AS_STRING,CFG_REQUIRED+CFG_ONEONLY,0},
    {"mode",&program_mode,ABSOLUTE,0,read_program_mode,CONFIG_CMD,0},
    {"coordinate_file",NULL,ABSOLUTE,0,load_coordinate_file,CFG_REQUIRED+CFG_ONEONLY, 0},
    {"station_offset_file",NULL,ABSOLUTE,0,load_offset_file,0, 0},
    {"topocentre",NULL,ABSOLUTE,0,read_topocentre,CFG_ONEONLY,0},
    {"data_file",NULL,ABSOLUTE,0,load_data_file,CFG_REQUIRED,0},
    {"geoid",NULL,ABSOLUTE,0,read_geoid_option,CONFIG_CMD,0},
    {"fix",NULL,ABSOLUTE,0,process_station_list,CONSTRAINT_CMD,FIX_STATIONS},
    {"free",NULL,ABSOLUTE,0,process_station_list,CONSTRAINT_CMD,FREE_STATIONS},
    {"float",NULL,ABSOLUTE,0,process_station_list,CONSTRAINT_CMD,FLOAT_STATIONS},
    {"ignore",NULL,ABSOLUTE,0,process_station_list,0,IGNORE_STATIONS},
    {"reject",NULL,ABSOLUTE,0,process_station_list,0,REJECT_STATIONS},
    {"accept",NULL,ABSOLUTE,0,process_station_list,0,ACCEPT_STATIONS},
    {"horizontal_float_error",&dflt_herr,ABSOLUTE,0,readcfg_double,CONFIG_CMD | CONSTRAINT_CMD,0},
    {"vertical_float_error",&dflt_verr,ABSOLUTE,0,readcfg_double,CONFIG_CMD | CONSTRAINT_CMD,0},
    {"ignore_missing_stations",NULL,ABSOLUTE,0,read_ignore_missing_stations,CONFIG_CMD,0},
    {"recode",NULL,ABSOLUTE,0,read_recode,0,0},
    {"max_iterations",&max_iterations,ABSOLUTE,0,readcfg_int,CONFIG_CMD,0},
    {"max_adjustment",&max_adjustment,ABSOLUTE,0,readcfg_double,CONFIG_CMD,0},
    {"convergence_tolerance",&convergence_tol,ABSOLUTE,0,readcfg_double,CONFIG_CMD,0},
    {"coordinate_precision",&coord_precision,ABSOLUTE,0,readcfg_int,CONFIG_CMD,0},
    {"reorder_stations",NULL,ABSOLUTE,0,read_station_ordering,CONFIG_CMD,0},
    {"refraction_coefficient",NULL,ABSOLUTE,0,read_coef,CONFIG_CMD,PRM_REFCOEF},
    {"distance_scale_error",NULL,ABSOLUTE,0,read_coef,CONFIG_CMD,PRM_DISTSF},
    {"bearing_orientation_error",NULL,ABSOLUTE,0,read_coef,CONFIG_CMD,PRM_BRNGREF},
    {"systematic_error",NULL,ABSOLUTE,0,read_coef,0,PRM_SYSERR},
    {"default_refraction_coefficient",&dflt_rc,ABSOLUTE,0,readcfg_double,CONFIG_CMD,0},
    {"reference_frame",NULL,ABSOLUTE,0,read_rftrans,CONFIG_CMD,0},
    {"reference_frame_scale_error",NULL,ABSOLUTE,0,read_rfscale,CFG_ONEONLY,0},
    {"output",NULL,ABSOLUTE,0,read_output_options,CONFIG_CMD,LIST_OPTIONS},
    {"print",NULL,ABSOLUTE,0,read_output_options,CONFIG_CMD,LIST_OPTIONS},
    {"list",NULL,ABSOLUTE,0,read_output_options,CONFIG_CMD,LIST_OPTIONS},
    {"output_csv",NULL,ABSOLUTE,0,read_output_options,CONFIG_CMD,CSV_OPTIONS},
    {"define_residual_format",NULL,ABSOLUTE,0,read_residual_format,CONFIG_CMD,DEFINE_RESIDUAL_COLUMNS},
    {"add_residual_column",NULL,ABSOLUTE,0,read_residual_format,CONFIG_CMD,ADD_RESIDUAL_COLUMNS},
    {"output_precision",NULL,ABSOLUTE,0,read_output_precision,CONFIG_CMD,0},
    {"classification",NULL,ABSOLUTE,0,read_classification,0,0},
    {"sort_observations",NULL,ABSOLUTE,0,read_sort_option,CONFIG_CMD,0},
    {"summarize_errors_by",NULL,ABSOLUTE,0,read_error_summary,CONFIG_CMD,0},
    {"summarise_errors_by",NULL,ABSOLUTE,0,read_error_summary,CONFIG_CMD,0},
    {"number_of_worst_residuals",&maxworst,ABSOLUTE,0,readcfg_int,CONFIG_CMD,0},
    {"station_code_width",&stn_name_width,ABSOLUTE,0,readcfg_int,CONFIG_CMD,0},
    {"file_location_frequency",&file_location_frequency,ABSOLUTE,0,readcfg_int,CONFIG_CMD,0},
    {"flag_significance",NULL,ABSOLUTE,0,read_flag_levels,CONFIG_CMD,0},
    {"mde_power",&mde_power,ABSOLUTE,0,readcfg_double,CONFIG_CMD,0},
    {"redundancy_flag_level",&redundancy_flag_level,ABSOLUTE,0,readcfg_double,CONFIG_CMD,0},
    {"error_type",NULL,ABSOLUTE,0,read_error_type,CONFIG_CMD,0},
    {"gps_vertical",NULL,ABSOLUTE,0,read_gps_vertical,CONFIG_CMD,0},
    {"use_distance_ratios",&use_distance_ratios,ABSOLUTE,0,readcfg_boolean,CONFIG_CMD,0},
    {"deformation",NULL,ABSOLUTE,0,read_deformation_model,0,CFG_ONEONLY},
    {"plot",NULL,ABSOLUTE,0,load_plot_data,0,0},
    {"magic_number",NULL,ABSOLUTE,0,set_magic_number,CONFIG_CMD,0},
    {"configuration",NULL,ABSOLUTE,0,read_configuration_command,0,CFG_COMMAND},
    {"include",NULL,ABSOLUTE,0,read_configuration_command,0,INC_COMMAND},
    {"include",NULL,ABSOLUTE,0,read_configuration_command,CONSTRAINT_CMD,CON_COMMAND},
    {"test_specification",NULL,ABSOLUTE,0,process_station_list,0,SPEC_TEST},
    {"specification",NULL,ABSOLUTE,0,read_specification_command,CONFIG_CMD,0},
    {"spec_test_options",NULL,ABSOLUTE,0,read_spec_test_options,CONFIG_CMD,0},
    {NULL}
};

static void initiallize_config( void )
{
    if( config_initiallized ) return;
    stations_read = 0;
    dflt_herr = dflt_verr = 1.0;
    dflt_rc = DEFAULT_REFCOEF;
    initiallize_config_items( snap_commands );
    config_initiallized = 1;
}


int read_command_file( const char *command_file )
{
    CFG_FILE *cfg;

    int sts;

    initiallize_config();

    cfg = open_config_file( command_file, COMMENT_CHAR );

    if(cfg)
    {
        set_config_read_options( cfg, CFG_CHECK_MISSING );
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

int read_command_file_constraints( const char *command_file )
{
    CFG_FILE *cfg;

    int sts;

    cfg = open_config_file( command_file, COMMENT_CHAR );
    if(cfg)
    {
        set_config_read_options( cfg, CFG_IGNORE_BAD );
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


static int process_configuration_file( const char *file_name, char cfg_only )
{
    CFG_FILE *cfg;
    int sts;

    if( ! file_exists( file_name ) ) return FILE_OPEN_ERROR;

    initiallize_config();
    cfg = open_config_file( file_name, COMMENT_CHAR );
    if( cfg )
    {
        set_config_read_options( cfg, 0 );
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

int read_configuration_file( const char *file_name )
{
    return process_configuration_file( file_name, 1 );
}


const char *find_configuration_file( const char *name )
{
    return find_file( name, DFLTCONFIG_EXT, 0, FF_TRYPROJECT, SNAP_CONFIG_SECTION );
}

int process_default_configuration( void )
{
    int sts, sts1;
    const char *spec;
    sts = OK;
    spec=build_config_filespec(0, 0, system_config_dir(),0,SNAP_CONFIG_SECTION, SNAP_CONFIG_FILE, NULL );
    if( file_exists( spec ))
    {
        sts = read_configuration_file( spec );
    }

    spec=build_config_filespec(0, 0, user_config_dir(),0,SNAP_CONFIG_SECTION, SNAP_CONFIG_FILE, NULL );
    if( file_exists( spec ))
    {
        sts1 = read_configuration_file( spec );
        if( sts == OK ) sts = sts1;
    }

    spec=build_config_filespec( 0, 0, command_file, 1, 0, SNAP_CONFIG_FILE, NULL );
    if( file_exists( spec ))
    {
        sts1 = read_configuration_file( spec );
        if( sts == OK ) sts = sts1;
    }
    return sts;
}


// #pragma warning( disable : 4100 )

static int read_program_mode( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char modeset, dimset, sts, *opt;
    dimset = 0;
    modeset = 0;
    sts = OK;

    for( opt = strtok(string," "); sts == OK && opt; opt = strtok(NULL," ") )
    {
        if( _stricmp(opt,"horizontal") == 0 || _stricmp(opt,"2d") == 0 )
        {
            dimension = 2;
            if( dimset ) sts = INVALID_DATA;
            dimset = 1;
        }
        else if( _stricmp(opt,"vertical") == 0 || _stricmp(opt,"1d") == 0 )
        {
            dimension = 1;
            if( dimset ) sts = INVALID_DATA;
            dimset = 1;
        }
        else if( _stricmp(opt,"3d") == 0 )
        {
            dimension = 3;
            if( dimset ) sts = INVALID_DATA;
            dimset = 1;
        }
        else if( _stricmp(opt,"preanalysis") == 0 ||
                 _stricmp(opt,"network_analysis") == 0 )
        {
            program_mode = PREANALYSIS;
            if( modeset ) sts = INVALID_DATA;
            modeset = 1;
        }
        else if( _stricmp(opt,"adjustment") == 0 )
        {
            program_mode = ADJUST;
            if( modeset ) sts = INVALID_DATA;
            modeset = 1;
        }
        else if( _stricmp(opt,"data_check") == 0 )
        {
            program_mode = DATA_CHECK;
            if( modeset ) sts = INVALID_DATA;
            modeset = 1;
        }
        else if( _stricmp(opt,"data_consistency") == 0 ||
                 _stricmp(opt,"free_net_adjustment") == 0 )
        {
            program_mode = DATA_CONSISTENCY;
            if( modeset ) sts = INVALID_DATA;
            modeset = 1;
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

static int read_geoid_option( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *opt;

    opt = strtok( string, " " );
    if( ! opt )
    {
        send_config_error( cfg, INVALID_DATA, "Geoid command requires a filename or option");
        return OK;
    }

    if( geoid_file )
    {
        check_free( geoid_file );
        geoid_file = 0;
    }
    overwrite_geoid = 0;

    if( _stricmp(opt,"none") != 0 )
    {
        geoid_file = copy_string( opt );
        opt = strtok( NULL, " " );
        if( opt && _stricmp(opt,"overwrite") == 0 )
        {
            overwrite_geoid = 1;
            opt = strtok( NULL, " " );
        }
    }
    else
    {
        opt = strtok( NULL, " " );
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

    if( mode == SPEC_TEST )
    {
        int istn = find_station(net,st->Code);
        set_station_spec_testid( istn, spm->option, 1 );
        return;
    }

    sa = stnadj(st);

    switch( mode )
    {

    case REJECT_STATIONS: sa->flag.rejected = 1; sa->flag.ignored = 0; break;

    case ACCEPT_STATIONS: sa->flag.rejected = 0; sa->flag.ignored = 0; break;

    case IGNORE_STATIONS: sa->flag.rejected = 1; sa->flag.ignored = 1; break;

    case NOREORDER_STATIONS:  sa->flag.noreorder = 1; break;

    case FIX_STATIONS:    if(fixhor) sa->flag.adj_h = 0;
        if(fixver) sa->flag.adj_v = 0;
        break;

    case FREE_STATIONS:  if(fixhor) sa->flag.adj_h = 1;
        if(fixver) sa->flag.adj_v = 1;
        break;
    case FLOAT_STATIONS:  set_station_float( st, fixhor, dflt_herr,
                fixver, dflt_verr );
        break;
    }
}


static int process_station_list( CFG_FILE *cfg, char *string, void *value, int len, int mode )
{
    char *field;
    char *strend;
    station_process_mode spm;
    int setall;

    if( ! stations_read )
    {
        send_config_error(cfg,INVALID_DATA,
                          "Stations cannot be fixed, floated, or rejected before the station file is loaded");
        return OK;
    }

    strend = string + strlen(string);

    spm.mode = mode;
    spm.option = MODE_HOR | MODE_VRT;

    setall = 0;

    field = strtok( string, " " );

    if( mode == SPEC_TEST )
    {

        if( ! field )
        {
            send_config_error(cfg,INVALID_DATA,"Test class name missing");
            return OK;
        }
        if( get_spec_testid( field, &(spm.option) ) != OK )
        {
            send_config_error(cfg,INVALID_DATA,"Invalid  test class name");
            return OK;
        }
        field = strtok(NULL, " ");
    }


    if( field && _stricmp( field, "all" ) == 0 )
    {
        setall = 1;
        field = strtok(NULL, " ");
    }

    if( field && (
                mode == FLOAT_STATIONS ||
                mode == FIX_STATIONS   ||
                mode == FREE_STATIONS    ) )
    {
        spm.option = 0;
        if( _stricmp( field, "horizontal") == 0 )
        {
            spm.option |= MODE_HOR;
            field = strtok( NULL, " ");
        }
        else if( _stricmp( field, "vertical") == 0 )
        {
            spm.option |= MODE_VRT;
            field = strtok( NULL, " ");
        }
        else if( _stricmp( field, "3d") == 0 )
        {
            spm.option |= (MODE_HOR | MODE_VRT);
            field = strtok( NULL, " ");
        }
        else
        {
            spm.option |= (MODE_HOR | MODE_VRT);
        }
    }

    if( setall )
    {
        int istn;
        for( istn = number_of_stations(net); istn; istn-- )
        {
            set_station_mode( stnptr(istn),&spm );
        }
    }

    else if( field )
    {
        /* Remove nulls introduced by strtok from rest of string */
        char *s;
        int nerr;

        for( s = field; s < strend; s++ )
        {
            if( ! *s ) *s = ' ';
        }
        /* Set up error handler so that errors can be attributed to configuration file */
        set_error_location( get_config_location(cfg));
        nerr = get_error_count();

        process_selected_stations( net,field,cfg->name,&spm,set_station_mode);

        set_error_location(NULL);
        cfg->errcount += (get_error_count()-nerr);
    }

    return OK;
}

static int read_recode( CFG_FILE *cfg, char *string, void *value, int len, int code )
{

    if( ! stations_read )
    {
        send_config_error(cfg,INVALID_DATA,
                          "Stations cannot be recoded before the station file is loaded");
        return OK;
    }
    if( ! stnrecode ) stnrecode=create_stn_recode_map( net );
    if( read_station_recode_definition( stnrecode, string, cfg->name ) != OK )
    {
        send_config_error(cfg,INVALID_DATA,"Errors encountered in recode command" );
    }
    return OK;
}


static int read_ignore_missing_stations( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    unsigned char option;
    int sts;
    option = 0;
    if( !string[0] )
    {
        option = 1;
        sts = OK;
    }
    else
    {
        sts = readcfg_boolean( cfg, string, &option, 1, 0 );
    }
    if( sts == OK ) set_ignore_missing_stations( option );
    return sts;
}

static int read_station_ordering( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *opt;
    char *st;

    for( opt = strtok( string, " " ); opt; opt = strtok( NULL, " " ) )
    {

        if( !opt || _stricmp(opt,"on") == 0  )
        {
            reorder_stations = FORCE_REORDERING;
        }
        else if ( _stricmp(opt,"off") == 0 )
        {
            reorder_stations = SKIP_REORDERING;
        }
        else if( _stricmp(opt,"except") == 0 && NULL != (st = strtok(NULL,"")) )
        {
            if( !stations_read )
            {
                send_config_error( cfg, INVALID_DATA,
                                   "Cannot specify which stations not to reorder before station_file command");
                break;
            }
            else
            {
                process_station_list( cfg, st, value, len, NOREORDER_STATIONS );
                reorder_stations = FORCE_REORDERING;
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


static int read_coef( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *st, *rcname;
    int sts, use, calculate;
    double rc;

    sts = OK;

    rc = code == PRM_REFCOEF ? dflt_rc : 0.0;
    calculate = 0;
    use = 0;

    rcname = strtok( string, " " );

    if( rcname )
    {
        if( code != PRM_SYSERR && _stricmp(rcname, "use" ) == 0 )
        {
            use = 1;
            rcname = strtok( NULL, " " );
        }
        else if( _stricmp( rcname, "calculate" ) == 0 )
        {
            calculate = 1;
            rcname = strtok( NULL, " " );
        }
    }

    if( !rcname ) sts = MISSING_DATA;

    st = strtok( NULL, " ");

    if( use )
    {
        if( sts == OK )
        {
            switch( code )
            {
            case PRM_BRNGREF: set_coef_class( COEF_CLASS_BRNGREF, rcname ); break;
            case PRM_DISTSF:  set_coef_class( COEF_CLASS_DISTSF, rcname ); break;
            case PRM_REFCOEF: set_coef_class( COEF_CLASS_REFCOEF, rcname ); break;
            }
        }
        if( st ) sts = INVALID_DATA;
        return sts;
    }

    if( !calculate && st && strcmp( st, "=" ) == 0 )
    {
        st = strtok( NULL, " " );
        if( st )
        {
            define_coef_match( code, rcname, st );
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
            if( sscanf( st, "%lf", &rc ) != 1 ) sts = INVALID_DATA;
            st = strtok( NULL, " ");
            if( st && strcmp(st,"?") == 0 ) calculate = 1;
        }
        if( sts == OK ) define_coef( code, rcname, rc, calculate );
    }
    else
    {
        sts = MISSING_DATA;
    }
    return sts;
}


static int obstype_from_code( char *code )
{
    datatypedef *dt = datatypedef_from_code( code );
    return dt ? dt->id : -1;
}


static int read_classification( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    double errfct;
    int class_id;
    int name_id;
    int reject;
    int ignore;
    char *st;

    st = strtok( string, " " );
    if( !st )
    {
        send_config_error( cfg, MISSING_DATA, "Name of the classification is missing");
        return OK;
    }

    if( _stricmp( st, "data_type" ) == 0 )
    {
        class_id = -1;
    }
    else if( _stricmp( st, "data_file" ) == 0 )
    {
        class_id = -2;
    }
    else
    {
        class_id = classification_id( &obs_classes, st, 1 );
    }

    errfct = -1.0;
    reject = ignore = 0;

    st = strtok( NULL, " " );
    if( st )
    {
        if( _stricmp( st, "reject" ) == 0 )
        {
            reject = 1;
        }
        else if( _stricmp( st, "ignore" ) == 0 )
        {
            ignore = 1;
        }
        if( reject || ignore ) st = strtok( NULL, " " );
    }

    if( !st )
    {
        send_config_error( cfg, MISSING_DATA, "Value of classification is missing");
        return OK;
    }

    if( class_id == -1 )
    {
        name_id = obstype_from_code( st );
        if( name_id < 0 )
        {
            send_config_error( cfg, INVALID_DATA, "Invalid observation data_type in classification command");
            return OK;
        }
    }
    else if( class_id == -2 )
    {
        name_id = survey_data_file_id( st );
        if( name_id < 0 )
        {
            send_config_error( cfg, INVALID_DATA, "Invalid data_file name in classification command");
            return OK;
        }
    }
    else
    {
        name_id = class_value_id( &obs_classes, class_id, st, 1 );
    }

    st = strtok( NULL, " " );

    if( !reject && !ignore )
    {
        if( !st )
        {
            send_config_error( cfg, MISSING_DATA,"Classification commands needs ignore, reject, or error_factor specified");
            return OK;
        }

        if( _stricmp( st, "reject" ) == 0 )
        {
            reject = 1;
        }
        else if( _stricmp( st, "ignore" ) == 0 )
        {
            ignore = 1;
        }
        if( reject || ignore ) st = strtok( NULL, " " );
    }

    if( st )
    {
        if( _stricmp(st,"error_factor") != 0 || (st=strtok(NULL," ")) == NULL ||
                sscanf(st, "%lf", &errfct ) != 1 || errfct <= 0.0 ||
                strtok(NULL," ") != NULL )
        {

            send_config_error(cfg, INVALID_DATA, "Invalid or missing data in classification command");
            return OK;
        }
    }

    if( class_id == -1 )
    {
        if( ignore ) obs_usage[name_id] |= IGNORE_OBS_BIT;
        if( reject ) obs_usage[name_id] |= REJECT_OBS_BIT;
        if( errfct > 0.0 ) obs_errfct[name_id] = errfct;
    }
    else if( class_id == -2 )
    {
        survey_data_file *s;
        s = survey_data_file_ptr( name_id );
        if( ignore ) s->usage |= IGNORE_OBS_BIT;
        if( reject ) s->usage |= REJECT_OBS_BIT;
        if( errfct > 0.0 ) s->errfct = errfct;
    }
    else
    {
        if( ignore ) set_class_flag( &obs_classes, class_id, name_id, IGNORE_OBS_BIT );
        if( reject ) set_class_flag( &obs_classes, class_id, name_id, REJECT_OBS_BIT );
        if( errfct > 0.0 ) set_class_error_factor( &obs_classes, class_id, name_id, errfct );
    }

    return OK;
}


static int read_rfscale( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    double scale = 0.0;
    int calculate = 0;
    char *st;

    for( st = strtok(string," "); st; st=strtok(NULL," "))
    {
        if( _stricmp(st,"calculate") == 0 || strcmp(st,"?") == 0 )
        {
            calculate = 1;
        }
        else if( sscanf( st, "%lf", &scale ) != 1 )
        {
            return INVALID_DATA;
        }
    }

    init_rf_scale_error( scale, calculate );
    return OK;
}


static int read_rftrans( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *rfname, *prmname;
    int rfid;
    rfTransformation *rf;
    double val[14];
    double date;
    int i, nval, nprm;
    int sts;
    int calculate;
    int first;
    int calcval[14];
    int got_type;
    int iers;
    int topocentric;
    int origintype=REFFRM_ORIGIN_DEFAULT;
    char errmess[256];

    calculate = 0;
    topocentric = 0;
    got_type = 0;
    iers=0;
    rfname = NULL;

    /* Process to handle the calculate, geocentric/topocentric, and name
       fields */

    sts = OK;
    first = 1;

    for( prmname = strtok( string, " " ); prmname; prmname = strtok(NULL, " ") )
    {
        if( first )
        {
            char global_command=0;
            if( _stricmp( prmname, "use") == 0 )
            {
                rfname = strtok(NULL," ");
                if( rfname )
                {
                    set_coef_class( COEF_CLASS_REFFRM, rfname );
                    if( strtok(NULL," ")) sts = INVALID_DATA;
                }
                else
                {
                    sts = MISSING_DATA;
                }
                global_command=1;
            }
            if( _stricmp( prmname, "use_topocentre_origin") == 0 )
            {
                set_use_refframe_topocentre( 1 );
                global_command=1;
            }
            else if( _stricmp( prmname, "use_zero_origin") == 0 )
            {
                set_use_refframe_topocentre( 0 );
                global_command=1;
            }
            if( global_command )
            {
                prmname=strtok(NULL," ");
                if( prmname )
                {
                    send_config_error(cfg,INVALID_DATA,"Extraneous data in reference_frame command");
                }
                return OK;
            }
        }

        first = 0;
        if( !calculate && _stricmp( prmname, "calculate" ) == 0 )
        {
            if( !calculate )
            {
                calculate = 1;
            }
            else
            {
                sts = INVALID_DATA;
            }
        }
        else if( _stricmp( prmname, "geocentric" ) == 0 )
        {
            if( !got_type )
            {
                topocentric = 0;
                iers=0;
                got_type = 1;
            }
            else
            {
                sts = INVALID_DATA;
            }
        }
        else if( _stricmp( prmname, "topocentric" ) == 0 )
        {
            if( !got_type )
            {
                topocentric = 1;
                iers=0;
                got_type = 1;
            }
            else
            {
                sts = INVALID_DATA;
            }
        }
        else if( _stricmp( prmname, "iers_etsr" ) == 0 )
        {
            if( !got_type )
            {
                topocentric = 0;
                iers=1;
                got_type = 1;
                origintype=REFFRM_ORIGIN_ZERO;
            }
            else
            {
                sts = INVALID_DATA;
            }
        }
        else if( _stricmp( prmname, "origin" ) == 0 )
        {
            prmname=strtok(NULL," ");
            if( ! prmname )
            {
                send_config_error(cfg,INVALID_DATA,
                        "Origin type missing in reference_frame command");
                return OK;
            }
            else if( _stricmp(prmname,"topocentre") == 0 )
            {
                origintype=REFFRM_ORIGIN_TOPOCENTRE;
            }
            else if( _stricmp(prmname,"zero") == 0 )
            {
                origintype=REFFRM_ORIGIN_ZERO;
            }
            else
            {
                sprintf(errmess,"Origin %.*s invalid in reference frame command. "
                        "Must be either topocentre or zero.",20,prmname);
                send_config_error(cfg,INVALID_DATA,errmess);
                return OK;
            }
        }
        else if( !rfname )
        {
            rfname = prmname;
        }
        else
        {
            break;
        }
    }

    if( !rfname ) sts = MISSING_DATA;
    if( sts != OK ) return sts;

    if( topocentric )
    {
        rfid = get_topocentric_rftrans_id( rfname );
        rf=rftrans_from_id( rfid );
        if( ! rftrans_topocentric( rf ) ) sts = INVALID_DATA;
    }
    else
    {
        rfid = get_rftrans_id( rfname );
        rf=rftrans_from_id( rfid );
        if( rftrans_topocentric( rf ) ) sts = INVALID_DATA;
    }
    set_rftrans_origintype( rf, origintype);

    if( sts == INVALID_DATA )
    {
        sprintf(errmess,"Ref frame %s defined as both topocentric and geocentric",rfname);
        send_config_error( cfg, INVALID_DATA, errmess );
        return OK;
    }

    sts = MISSING_DATA;

    if( iers )
    {
        if( ! prmname )
        {
            sprintf(errmess,"Missing epoch date for IERS reference frame transformation");
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }
        date=snap_datetime_parse( prmname, 0 );
        if( date == UNDEFINED_DATE )
        {
            sprintf(errmess,"Invalid epoch date %s for IERS reference frame transformation",prmname);
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }

        sts=OK;
        nval=14;
        for( i = 0; i < 14; i++ )
        {
            prmname = strtok( NULL, " ");
            if( i && prmname && strcmp(prmname,"?") == 0 )
            {
                i--;
                calcval[i] = 1;
            }
            if( ! prmname )
            {
                if( i == 7 ) 
                {
                    nval=7;
                    break;
                }
                sts = MISSING_DATA;
                break;
            }
            if( prmname[strlen(prmname)-1] == '?' )
            {
                prmname[strlen(prmname)-1] = 0;
                calcval[i] = 1;
            }
            if( sscanf(prmname,"%lf",val+i) != 1 )
            {
                sts = MISSING_DATA;
                break;
            }
        }
        if( prmname ) prmname = strtok(NULL," ");
        if( prmname && strcmp(prmname,"?") == 0 )
        {
            calcval[13] = 1;
            prmname = strtok(NULL, " ");
        }
        if( sts != OK )
        {
            sprintf(errmess,"Invalid or missing IERS reference frame parameters");
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }
        if( prmname )
        {
            sprintf(errmess,"Extraneous data in IERS reference frame parameters");
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }
        /* Convert to legacy conventions */
        for( int i = 0; i < nval; i++ ) { val[i] *= 0.001; calcval[i] |= calculate; }
        for( int i = 0; i < 3; i++ ){ val[i+4] *= -1; val[i+11] *= -1; } 
            
        set_rftrans_ref_date( rf, date );
        set_rftrans_translation( rf, val, calcval ); 
        set_rftrans_scale( rf, val[3], calcval[3] );
        set_rftrans_rotation( rf, val+4, calcval+4 );
        if( nval > 7 )
        {
            set_rftrans_translation_rate( rf+7, val+7, calcval+7 );
            set_rftrans_scale_rate( rf, val[10], calcval[10] );
            set_rftrans_rotation_rate( rf, val+11, calcval+11 );
        }
    }
    else if( (! prmname) && calculate )
    {
        val[0]=val[1]=val[2]=0.0;
        calcval[0]=calcval[1]=calcval[2]=1;
        set_rftrans_scale( rf, 0.0, 1 ); 
        set_rftrans_rotation( rf, val, calcval ); 
        set_rftrans_translation( rf, val, calcval ); 
        sts=OK;
    }
    else
    {
        while( prmname )
        {
            sts = OK;
            if( _stricmp(prmname,"epoch") == 0  ) { nval = 1; nprm = 0; }
            else if( _stricmp(prmname,"scale") == 0  ) { nval = 1; nprm = 1; }
            else if (_stricmp(prmname,"rotation") == 0 ) { nval = 3; nprm = 2; }
            else if (_stricmp(prmname,"translation") == 0 ) { nval = 3; nprm = 3; }
            else if( _stricmp(prmname,"scale_rate") == 0  ) { nval = 1; nprm = 4; }
            else if (_stricmp(prmname,"rotation_rate") == 0 ) { nval = 3; nprm = 5; }
            else if (_stricmp(prmname,"translation_rate") == 0 ) { nval = 3; nprm = 6; }
            else
            {
                sprintf(errmess,"Invalid parameter %s in reference_frame command",prmname);
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }

            if( nprm == 0 )
            {
                double date = UNDEFINED_DATE;
                prmname = strtok(NULL, " ");
                if( prmname ) date = snap_datetime_parse( prmname, 0 );
                if( date == UNDEFINED_DATE )
                {
                    sprintf(errmess,"Invalid or missing epoch date %s for reference frame transformation",prmname);
                    send_config_error( cfg, INVALID_DATA, errmess );
                    return OK;
                }
                set_rftrans_ref_date( rf, date );
                prmname = strtok(NULL," ");
                continue;
            }

            /* Try to read values for the parameter */

            val[0] = val[1] = val[2] = 0;
            calcval[0] = calcval[1] = calcval[2] = calculate;

            prmname = strtok(NULL, " ");

            if( prmname && sscanf(prmname,"%lf",val) == 1 )
            {
                for( i = 1; i < nval; i++ )
                {
                    prmname = strtok( NULL, " ");
                    if( prmname && strcmp(prmname,"?") == 0 )
                    {
                        i--;
                        calcval[i] = 1;
                    }
                    else if( !prmname || sscanf(prmname,"%lf",val+i) != 1 )
                    {
                        sts = MISSING_DATA;
                        break;
                    }
                }
                prmname = strtok(NULL," ");
                if( prmname && strcmp(prmname,"?") == 0 )
                {
                    calcval[nval-1] = 1;
                    prmname = strtok(NULL, " ");
                }
            }

            if( sts != OK ) break;

            /* Set the values */

            switch( nprm )
            {
            case 1: set_rftrans_scale( rf, val[0], calcval[0] ); break;
            case 2: set_rftrans_rotation( rf, val, calcval ); break;
            case 3: set_rftrans_translation( rf, val, calcval ); break;
            case 4: set_rftrans_scale_rate( rf, val[0], calcval[0] ); break;
            case 5: set_rftrans_rotation_rate( rf, val, calcval ); break;
            case 6: set_rftrans_translation_rate( rf, val, calcval ); break;
            }

        }
    }

    if( sts != OK )
    {
        send_config_error( cfg, MISSING_DATA, "Invalid or missing data in reference_frame command");
    }

    return OK;
}

static int read_output_options( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    output_option *output_set;
    output_option *o;
    char *st;
    char set;
    char errmess[80];

    output_set = code == CSV_OPTIONS ? csvopt : output;

    for( st = strtok(string," "); st; st=strtok(NULL," "))
    {
        if( _stricmp( st, "everything") == 0 )
        {
            for( o = output_set; o->name; o++ ) *(o->status) = 1;
            continue;
        }
        if( _strnicmp(st,"no_",3) == 0 )
        {
            set = 0;
            st += 3;
        }
        else
        {
            set = 1;
        }

        for( o = output_set; o->name; o++ )
        {
            if( _stricmp( st, o->name ) == 0 )
            {
                *(o->status) = set;
                break;
            }
        }
        if( !o->name )
        {
            sprintf(errmess,"Invalid output option %.30s", st );
            send_config_error( cfg, INVALID_DATA, errmess );
        }
    }
    return OK;
}

static int read_residual_format( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *types;
    char *column;
    char *saveptr, save;
    char title1[81];
    char title2[81];
    char *ttl1, *ttl2;
    int width;

    types = strtok( string, " " );
    if( !types ) return MISSING_DATA;

    if( define_residual_formats( types, code ) != OK )
    {
        char errmess[80];
        sprintf( errmess, "Invalid data types %.30s",types);
        send_config_error( cfg, MISSING_DATA, errmess );
        return OK;
    }

    string = strtok( NULL, "\n");
    while( NULL != (column = strtok( string, " " )) )
    {
        char *endcol;
        char *number;
        int valid = 1;
        int i;
        width = 0;
        ttl1 = NULL;
        ttl2 = NULL;
        string = strtok( NULL, "\n" );  /* Save a pointer to the rest */
        for( endcol = column; *endcol; endcol++ )
        {
            if( *endcol == ':' ) break;
        }
        saveptr = number = endcol;
        if( *number )
        {
            number++;
            saveptr = number;
            while( *saveptr && *saveptr != ':' ) saveptr++;
            save = *saveptr;
            *saveptr = 0;
            if( *number )
            {
                char garbage;
                int iwid;
                if( sscanf(number,"%d%c",&iwid,&garbage) != 1 || iwid < 0 )
                {
                    valid = 0;
                }
                width = iwid;
            }
            *saveptr = save;
        }
        for( i = 0; i++ < 2; )
        {
            int j;
            char *dest;

            /* Reading the titles, only if we have a : separator, then
               a character after it.  Copy characters to title1 or title2.
               _ converted to blanks, : terminates, \ initiates literal
               character */
            if( !*saveptr ) continue;
            saveptr++;
            if( !*saveptr ) continue;
            dest = (i == 1) ? title1 : title2;
            for( j = 0; j < 80; j++, saveptr++)
            {
                if( *saveptr == ':' || !*saveptr ) break;
                if( *saveptr == '_' )
                {
                    *dest++ = ' ';
                }
                else
                {
                    if( *saveptr == '\\' )
                    {
                        saveptr++;
                        if( !*saveptr ) break;
                    }
                    *dest++ = *saveptr;
                }
            }
            *dest = 0;
            if( j )
            {
                if( i == 1 ) ttl1 = title1; else ttl2 = title2;
            }
        }
        if( valid )
        {
            save = *endcol;
            *endcol = 0;
            if( add_residual_field( column, width, ttl1, ttl2 ) != OK ) valid = 0;
            *endcol = save;
        }
        if( !valid )
        {
            sprintf( title1, "Invalid column definition %.40s",column);
            send_config_error( cfg, INVALID_DATA, title1 );
        }
    }
    return OK;
}


static int read_output_precision( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *st;
    for( st = strtok(string," "); st; st=strtok(NULL," "))
    {
        char *ndp_str;
        int type;
        ndp_str = strtok(NULL," ");
        if( !ndp_str )
        {
            send_config_error(cfg,MISSING_DATA,"Missing precision in output_precision command");
            break;
        }
        for( type = 0; type < NOBSTYPE; type++ )
        {
            if( _stricmp(st,datatype[type].code) == 0 ) break;
        }
        if( type == NOBSTYPE )
        {
            char errmess[80];
            sprintf(errmess,"Invalid type code %.20s in output_precision command",st);
            send_config_error( cfg, INVALID_DATA, errmess );
        }
        else if( strlen(ndp_str) != 1 || !isdigit(ndp_str[0]) )
        {
            char errmess[80];
            sprintf(errmess,"Invalid precision %.20s in output_precision command",ndp_str);
            send_config_error( cfg, INVALID_DATA, errmess );
        }
        else
        {
            obs_precision[type] = ndp_str[0] - '0';
        }
    }
    return OK;
}

static int read_sort_option( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *s;

    sort_obs = SORTED_OBS + SORT_BY_LINE;
    s = strtok( string, " " );

    while( s )
    {
        if( _stricmp(s,"by_line") == 0 )
        {
        }
        else if( _stricmp(s,"by_type") == 0 )
        {
            sort_obs |= SORT_BY_TYPE;
        }
        else if( _stricmp(s,"by_instrument_station")==0 )
        {
            sort_obs &= ~SORT_BY_LINE;
        }
        else
        {
            return INVALID_DATA;
        }
        s = strtok( NULL, " ");
    }

    return OK;
}


static int read_flag_levels( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *s;
    int nf;
    double fl;

    nf = 0;

    for( s=strtok(string," "); s; s=strtok(NULL," ") )
    {
        if( nf > 2 ) { nf = 0; break; }
        if( _stricmp(s,"maximum") == 0 ) { taumax[nf] = 1; continue; }
        if( sscanf( s, "%lf", &fl ) != 1 || fl <= 0.0 || fl >= 100.0 )
        {
            nf = 0;
            break;
        }
        else
        {
            flag_level[nf] = fl;
            nf++;
        }
    }

    if( !nf )
    {
        send_config_error( cfg, INVALID_DATA, "Invalid or missing data in flag_significance command");
    }
    return OK;
}

static int read_error_type( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *fld;
    double conf=1.0;
    int useconf=0;

    fld = strtok( string, " ");
    if( !fld ) return MISSING_DATA;
    if( _stricmp( fld, "aposteriori" ) == 0 )
    {
        apriori = 0;
        fld = strtok( NULL, " ");
    }
    else if( _stricmp( fld, "apriori" ) == 0 )
    {
        apriori = 1;
        fld = strtok( NULL, " ");
    }
    else
    {
         send_config_error( cfg, INVALID_DATA, "Expected \"apriori\" or \"aposteriori\"");
         return OK;
    }

    if( fld ) 
    {
        if( _stricmp(fld,"standard_error") == 0 )
        {
            useconf = 0;
        }
        else
        {
            if( sscanf(fld,"%lf",&conf) != 1 )
            {
                send_config_error( cfg, INVALID_DATA, "Expected \"standard_error\" or \"##.#% confidence_limit\"");
                return OK;
            }
            if( conf <= 0.0 || conf >= 100.0 )
            {
                send_config_error( cfg, INVALID_DATA, "Confidence limit not between 0 and 100");
                return OK;
            }
            fld = strtok( NULL, " " );
            if( ! fld ) return MISSING_DATA;
            if( _stricmp(fld,"standard_error") == 0 )
            {
                useconf = 0;
            }
            else if( _stricmp(fld,"confidence_limit") == 0 )
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

static int read_error_summary( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *str;
    int sts;
    for( str = strtok(string," "); str; str=strtok(NULL," "))
    {
        sts = define_error_summary( str );
        if( sts != OK )
        {
            char errmsg[100];
            sprintf(errmsg,"Invalid error summary definition %.50s",str);
            send_config_error( cfg, INVALID_DATA, errmsg );
        }
    }
    return OK;
}

static int read_topocentre( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    double lt, ln;

    if( ! stations_read )
    {
        send_config_error(cfg,INVALID_DATA,
                          "The topocentre cannot be defined before the station file is loaded");
        return OK;
    }

    if( sscanf( string,"%lf%lf", &lt, &ln ) == 2 &&
            lt > -90.0 && lt < 90.0 && ln >= -180.0 && ln <= 180.0 )
    {
        set_network_topocentre( net, lt*DTOR, ln*DTOR );
        return OK;
    }

    return INVALID_DATA;
}

static int read_gps_vertical( CFG_FILE *cfg, char *string, void *value, int len, int code )
{

    if( _stricmp(string,"individual") == 0 || _stricmp(string,"midpoint")==0 )
    {
        set_gps_vertical_fixed( 0 );
        return OK;
    }

    if( _stricmp(string,"topocentre") == 0 )
    {
        set_gps_vertical_fixed( 1 );
        return OK;
    }

    return INVALID_DATA;
}

static int load_plot_data( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    return OK;
}

static int read_deformation_model(CFG_FILE *cfg, char *string, void *pvalue, int len, int code )
{
    char *params;
    double epoch;
    char *model;
    int type;
    char *rest;
    char *item;
    const char *value;
    const char *blank="";
    char first;
    first = 1;
    epoch = -1;
    model = NULL;
    ignore_deformation = 0;
    type = 0;
    params = (char *) check_malloc( strlen(string) + 2 );
    params[0] = 0;
    rest = string;

    while( NULL != (item = strtok( rest, " " )) )
    {
        rest = strtok( NULL, "");
        if( first && _stricmp(item,"none") == 0 )
        {
            ignore_deformation = 1;
            return OK;
        }
        else if( first && _stricmp(item,"datum") == 0 )
        {
            return OK;
        }
        first = 0;
        item = strtok(item,"=");
        value = strtok( NULL,"");
        if( value == NULL ) value = blank;
        if( _stricmp(item,"type") == 0 )
        {
            ignore_deformation = 1;
            if( _stricmp(value,"velocity") == 0 || _stricmp(value,"velgrid") == 0 )
            {
                type = DTP_VELOCITY;
            }
            else if( _stricmp(value,"linz") == 0 || _stricmp(value,"linzdef") == 0 )
            {
                type = DTP_LINZDEF;
                if( epoch < 0 ) epoch = 0;
            }
            else if( _stricmp(value,"none") == 0 )
            {
                ignore_deformation = 1;
            }
            else
            {
                send_config_error( cfg, INVALID_DATA, "Invalid deformation model type" );
            }
        }
        else if (_stricmp(item,"model") == 0 )
        {
            if( ! model ) 
            {
                model = copy_string(value);
            }
            else
            {
                send_config_error( cfg, INVALID_DATA, "Duplicated model in deformation definition" );
            }

        }
        else if (_stricmp(item,"epoch") == 0 )
        {
            if( sscanf(value,"%lf",&epoch) != 1 )
            {
                send_config_error( cfg, INVALID_DATA, "Invalid epoch in deformation definition" );
            }
        }
        else
        {
            strcat( params, item );
            if( strlen(value) > 0 )
            {
                strcat( params, "=");
                strcat( params, value );
            }
            strcat( params, " ");
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
    else if( model == NULL )
    {
        send_config_error( cfg, MISSING_DATA, "Model missing in deformation definition");
    }
    else if( type==DTP_VELOCITY && create_grid_deformation( &deformation, model, epoch ) != OK )
    {
        send_config_error( cfg, INVALID_DATA, "Invalid parameters in grid deformation definition");
    }
    else if( type==DTP_LINZDEF && create_linzdef_deformation( &deformation, model, epoch ) != OK )
    {
        send_config_error( cfg, INVALID_DATA, "Invalid parameters in LINZ deformation definition");
    }
    check_free( params );
    if( model ) check_free( model );
    return OK;
}


static int read_specification_command(CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *name;
    char *data;
    char type[21];
    char errtype[8];
    int nfld;
    int nchr;
    int gothortol;
    int gotvertol;
    double conf;
    double herrabs;
    double herrppm;
    double herrmax;
    double verrabs;
    double verrppm;
    double verrmax;
    int errdirflg;

    herrabs = herrppm = herrmax = 0.0;
    verrabs = verrppm = verrmax = 0.0;
    gothortol = gotvertol = 0;
    conf = 0.0;

    name = strtok(string," ");
    data = strtok(NULL,"\n");
    if( ! name || ! data )
    {
        send_config_error( cfg, INVALID_DATA, "Missing data in specification command" );
        return OK;
    }

    nfld = sscanf(data, "%20s%lf%%%n",type,&conf,&nchr);

    if( nfld < 2 || _stricmp(type,"confidence") != 0 )
    {
        send_config_error( cfg, INVALID_DATA, "Missing confidence in specification command" );
        return OK;
    }
    data += nchr;

    errdirflg = 0;
    for(;;)
    {
        int errtypflg = 0;
        int errdir = 0;

        nfld = sscanf(data,"%20s%n",type,&nchr);
        if( nfld <= 0 ) break;
        if( nfld != 1 )
        {
            send_config_error( cfg, INVALID_DATA, "Invalid specification accuracy");
            return OK;
        }
        data += nchr;

        if( _stricmp(type, "horizontal") == 0 )
        {
            errdir = 1;
            gothortol = 1;
        }
        else if( _stricmp(type, "vertical") == 0 )
        {
            errdir = 2;
            gotvertol = 1;
        }
        if( ! errdir || errdir & errdirflg )
        {
            send_config_error( cfg, INVALID_DATA, "Invalid hor/ver in accuracy specification");
            return OK;
        }
        errdirflg |= errdir;

        for(;;)
        {
            double err;
            int errtyp = 0;

            nfld = sscanf(data,"%lf%7s%n",&err,errtype,&nchr);
            if( nfld <= 0 ) break;
            data += nchr;
            if( nfld != 2 )
            {
                send_config_error(cfg,INVALID_DATA, "Invalid accuracy in specification");
                return OK;
            }
            if( _stricmp(errtype,"MM") == 0 )
            {
                errtyp = 4;
            }
            else if( _stricmp(errtype,"PPM") == 0 )
            {
                errtyp = 8;
            }
            else if( _stricmp(errtype,"MM_ABS") == 0 )
            {
                errtyp = 16;
            }
            if( ! errtyp || errtyp & errtypflg )
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


static int read_spec_test_options(CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *option;
    for( option = strtok(string," "); option; option = strtok(NULL," "))
    {
        if( _stricmp(option,"APRIORI") == 0 )
        {
            set_spec_apriori( 1 );
        }
        else if ( _stricmp(option,"APOSTERIORI") == 0 )
        {
            set_spec_apriori( 0 );
        }
        else if ( _stricmp(option,"LIST_ALL") == 0 )
        {
            set_spec_listoption( SPEC_LIST_ALL);
        }
        else if ( _stricmp(option,"LIST_FAIL") == 0 )
        {
            set_spec_listoption( SPEC_LIST_FAIL);
        }
        else if ( _stricmp(option,"LIST_NONE") == 0 )
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


static int set_magic_number( CFG_FILE *cfg, char *string ,void *value, int len, int code )
{
    int id;
    double val;

    if( sscanf( string, "%d%lf", &id, &val ) != 2 ) return INVALID_DATA;

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

static int read_configuration_command( CFG_FILE *cfg, char *string ,void *value, int len, int code )
{
    const char *cfgfile;
    char *ptr;
    char errmsg[100];
    char cfg_only;
    char constraint;

    cfg_only = code == CFG_COMMAND;
    constraint = code == CON_COMMAND;

    ptr = string;
    while( ptr && NULL != (cfgfile=strtok(ptr," \t\n")))
    {
        ptr = strtok(NULL,"\n");
        cfgfile = find_file( cfgfile, cfg_only ? DFLTCONFIG_EXT : DFLTCOMMAND_EXT,
                             0, FF_TRYPROJECT,
                             cfg_only ? SNAP_CONFIG_SECTION : 0 );
        if( cfgfile )
        {
            if( constraint )
            {
                if( read_command_file_constraints( cfgfile ) != OK )
                {
                    sprintf(errmsg,"Invalid data in configuration file %.40s",string);
                    send_config_error(cfg,INVALID_DATA,errmsg);
                }
            }
            else 
            {
                if( process_configuration_file( cfgfile, cfg_only ) != OK )
                {
                    sprintf(errmsg,"Invalid data in configuration file %.40s",string);
                    send_config_error(cfg,INVALID_DATA,errmsg);
                }
            }
        }
        else
        {
            sprintf(errmsg,"Cannot find configuration file %.40s",string);
            send_config_error( cfg, INVALID_DATA, errmsg );
        }
    }
    return OK;
}

