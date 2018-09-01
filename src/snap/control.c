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

static int read_program_mode( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_geoid_option( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int process_station_list( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_ignore_missing_stations( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_use_zero_inverse( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_coef( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_rftrans( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_rfscale( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_pb_use_datum_trans( CFG_FILE *cfg, char *string, void *value, int len, int code );
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
#define MODE_AUTO 4

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
    {"title",job_title,CFG_ABSOLUTE,JOBTITLELEN,STORE_AS_STRING,CFG_REQUIRED+CFG_ONEONLY,0},
    {"mode",&program_mode,CFG_ABSOLUTE,0,read_program_mode,CONFIG_CMD,0},
    {"coordinate_file",NULL,CFG_ABSOLUTE,0,load_coordinate_file,CFG_REQUIRED, 0},
    {"add_coordinate_file",NULL,CFG_ABSOLUTE,0,add_coordinate_file, 0, 0 },
    {"output_coordinate_file",NULL,CFG_ABSOLUTE,0,set_output_coordinate_file,CFG_ONEONLY, 0},
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
    {"use_distance_ratios",&use_distance_ratios,CFG_ABSOLUTE,0,readcfg_boolean,CONFIG_CMD,0},
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

int read_command_file_constraints( const char *command_file )
{
    CFG_FILE *cfg;
    int sts;

    cfg = open_config_file( command_file, COMMENT_CHAR );
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


static int process_configuration_file( const char *file_name, char cfg_only )
{
    CFG_FILE *cfg;
    int sts;

    if( ! file_exists( file_name ) ) return FILE_OPEN_ERROR;

    initiallize_config();
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

static int read_program_mode( CFG_FILE *cfg, char *string, void *, int, int )
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

static int read_geoid_option( CFG_FILE *cfg, char *string, void *, int, int )
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
    geoid_error_level = WARNING_ERROR;

    if( _stricmp(opt,"none") != 0 )
    {
        geoid_file = copy_string( opt );
        opt = strtok( NULL, " " );
        while( opt )
        {
            if( _stricmp(opt,"overwrite") == 0 )
            {
                overwrite_geoid = 1;
            }
            else if( _stricmp(opt,"warn_errors") == 0 )
            {
                geoid_error_level = INFO_ERROR;
            }
            else if( _stricmp(opt,"ignore_errors") == 0 )
            {
                geoid_error_level = OK;
            }
            else
            {
                break;
            }
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
        if(fixver) { sa->flag.adj_v = 1; sa->flag.float_h = 0; sa->flag.auto_v=0; }
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
                    sa->flag.float_h=1;
                    sa->herror=(float) dflt_herr;
                }
            }
            if(fixver) 
            { 
                if( (fixauto && ! sa->flag.float_v) || ! fixauto )
                {
                    sa->flag.auto_v=fixauto;
                    sa->flag.float_v=1;
                    sa->verror=(float) dflt_verr;
                }
            }
        }
        break;
    }
}


static int process_station_list( CFG_FILE *cfg, char *string, void *, int, int mode )
{
    char *field;
    char *strend;
    station_process_mode spm;
    char *allptr=0;

    if( ! stations_read )
    {
        send_config_error(cfg,INVALID_DATA,
                          "Stations cannot be fixed, floated, or rejected before the station file is loaded");
        return OK;
    }

    strend = string + strlen(string);

    spm.mode = mode;
    spm.option = MODE_HOR | MODE_VRT;

    allptr = 0;

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
        allptr = field;
        field = strtok(NULL, " ");
    }

    if( field && (
                mode == FLOAT_STATIONS ||
                mode == FIX_STATIONS   ||
                mode == FREE_STATIONS    ) )
    {
        spm.option = 0;
        if( mode != FREE_STATIONS && _stricmp( field, "automatically") == 0 )
        {
            spm.option |= MODE_AUTO;
            field = strtok( NULL, " ");
        }
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

    if( allptr && ! field )
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

        if( allptr )
        {
            for( s=allptr+3; s<field; s++ )
            {
                *s = ' ';
            }
            field=allptr;
        }

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


static int read_ignore_missing_stations( CFG_FILE *cfg, char *string, void *, int, int )
{
    int first=1;
    for( char *opt = strtok( string, " " ); opt; opt = strtok( NULL, " " ) )
    {
        if( first )
        {
            unsigned char ignoremissing=0;
            first=0;
            int sts=readcfg_boolean(cfg,opt,&ignoremissing,1,0);
            if( sts == OK ) 
            {
                set_ignore_missing_stations( ignoremissing );
                continue;
            }
        } 
        if( _stricmp(opt,"report_all") == 0 )
        {
            set_report_missing_stations(REPORT_MISSING_ALL);
            continue;
        }
        if( _stricmp(opt,"report_none") == 0 )
        {
            set_report_missing_stations(REPORT_MISSING_NONE);
            continue;
        }
        if( _stricmp(opt,"report_unlisted") == 0 )
        {
            set_report_missing_stations(REPORT_MISSING_UNLISTED);
            continue;
        }
        set_accept_missing_station( opt );
    }
    if( first )
    {
        set_ignore_missing_stations( 1 );
    }

    return OK;
}

static int read_use_zero_inverse( CFG_FILE *cfg, char *string, void *, int, int )
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
    if( sts == OK ) lsq_set_use_zero_inverse( option );
    return sts;
}

static int read_station_ordering( CFG_FILE *cfg, char *string, void *value, int len, int )
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


static int read_coef( CFG_FILE *, char *string, void *, int, int code )
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
            configure_param_match( code, rcname, st );
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
        if( sts == OK ) configure_param( code, rcname, rc, calculate );
    }
    else
    {
        sts = MISSING_DATA;
    }
    return sts;
}


static int read_rfscale( CFG_FILE *, char *string, void *, int, int )
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


static int read_rftrans( CFG_FILE *cfg, char *string, void *, int, int )
{
    char *rfname, *prmname, *valuetype;
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
    int prmread;
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

    rfname = NULL;

    /* Process to handle the calculate, geocentric/topocentric, and name
       fields */

    sts = OK;
    first = 1;

    rfname=strtok( string, " " );
    if( _stricmp(rfname,"use") == 0 )
    {
        prmname = strtok(NULL," ");
        if( prmname )
        {
            set_coef_class( COEF_CLASS_REFFRM, prmname );
            if( strtok(NULL," ")) sts = INVALID_DATA;
        }
        else
        {
            sts = MISSING_DATA;
        }
        return sts;
    }

    for( prmread=0, prmname = strtok(NULL, " ");
            prmname;
            prmname = prmread ? prmname : strtok(NULL, " "), prmread=0)
    {
        nval=0;
        ival=0;
        valuetype=prmname;
        if( _stricmp( prmname, "calculate" ) == 0 )
        {
            calculate=1;
        }
        else if( _stricmp( prmname, "topocentric" ) == 0 )
        {
            topocentric=1;
        }
        else if( _stricmp( prmname, "geocentric" ) == 0 )
        {
            topocentric=0;
        }
        else if( _stricmp( prmname, "epoch" ) == 0 )
        {
            prmname=strtok( NULL, " ");
            if( ! prmname )
            {
                sprintf(errmess,"Missing epoch date for reference frame %.20s",rfname);
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }
            date=snap_datetime_parse( prmname, 0 );
            if( date == UNDEFINED_DATE )
            {
                sprintf(errmess,"Invalid epoch date %.20s for reference frame %.20s",
                        prmname,rfname);
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }
        }
        else if( _stricmp( prmname, "origin" ) == 0 )
        {
            prmname=strtok( NULL, " ");
            if( _stricmp(prmname,"zero") == 0 ) origintype=REFFRM_ORIGIN_ZERO;
            else if( _stricmp(prmname,"0") == 0 ) origintype=REFFRM_ORIGIN_ZERO;
            else if( _stricmp(prmname,"topocentre") == 0 ) origintype=REFFRM_ORIGIN_TOPOCENTRE;
            else if( _stricmp(prmname,"default") == 0 ) origintype=REFFRM_ORIGIN_DEFAULT;
            else 
            {
                sprintf(errmess,"Invalid origin type %.20s for reference frame %.20s",
                        prmname,rfname);
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }
        }
        else if( _stricmp( prmname, "translation" ) == 0 )
        {
            ival=rfTx;
            nval=3;
        }
        else if( _stricmp( prmname, "translation_rate" ) == 0 )
        {
            ival=rfTxRate;
            nval=3;
        }
        else if( _stricmp( prmname, "scale" ) == 0 )
        {
            ival=rfScale;
            nval=1;
        }
        else if( _stricmp( prmname, "scale_rate" ) == 0 )
        {
            ival=rfScaleRate;
            nval=1;
        }
        else if( _stricmp( prmname, "rotation" ) == 0 )
        {
            ival=rfRotx;
            nval=3;
        }
        else if( _stricmp( prmname, "rotation_rate" ) == 0 )
        {
            ival=rfRotxRate;
            nval=3;
        }
        else if( _stricmp( prmname, "iers_tsr" ) == 0 )
        {
            ival=rfTx;
            nval=7;
            iers=1;
        }
        else if( _stricmp( prmname, "iers_etsr" ) == 0 )
        {
            prmname=strtok( NULL, " ");
            if( ! prmname )
            {
                sprintf(errmess,"Missing IERS_ETSR epoch date for reference frame %.20s",rfname);
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }
            date=snap_datetime_parse( prmname, 0 );
            if( date == UNDEFINED_DATE )
            {
                sprintf(errmess,"Invalid IERS_ETSR epoch date %.20s for reference frame %.20s",
                        prmname,rfname);
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
                    prmname, rfname);
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }

        first=1;
        for( ; nval; nval--, ival++, first=0 )
        {
            char *endptr;
            double value;
            prmname=prmread ? prmname : strtok(NULL, " ");
            if( ! prmname )
            {
                if( first )
                {
                    while( nval--) calcval[ival++]=calculate;
                    break;
                }
                sprintf(errmess,"Missing %.20s value for reference frame %.20s command",
                        valuetype,rfname );
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }
                
            calcval[ival]=calculate;
            value=strtod(prmname,&endptr);
            if( endptr == prmname )
            {
                if( first )
                {
                    while( nval--) calcval[ival++]=calculate;
                    prmread=1;
                    break;
                }
                sprintf(errmess,"Invalid %.20s value %.20s for reference frame %.20s command",
                        valuetype, prmname, rfname);
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }
            if( *endptr )
            {
                if( strcmp(endptr,"?" )==0 ) 
                {
                    calcval[ival]=1;
                }
                else
                {
                    sprintf(errmess,"Invalid %.20s value %.20s for reference frame %.20s command",
                            valuetype, prmname, rfname);
                    send_config_error( cfg, INVALID_DATA, errmess );
                    return OK;
                }
            }
            else
            {
                prmname=strtok(NULL," ");
                if( prmname && strcmp(prmname,"?") == 0 )
                {
                    calcval[ival]=1;
                }
                else
                {
                    prmread=1;
                }
            }
            if( defined[ival] )
            {
                sprintf(errmess,"Duplicated %.20s value definition for reference frame %.20s command",
                        valuetype, rfname);
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }
            defined[ival]=1;
            val[ival]=value;
        }
    }
    
    if( iers )
    {
        if( topocentric )
        {
            sprintf(errmess,"Ref frame %s cannot be defined with IERS parameters and topocentric",rfname);
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }
        for( i=0; i<14; i++) val[i]=0.001*val[i];
        for( i=0; i<3; i++ ) { val[i+rfRotx]*=-1.0; val[i+rfRotxRate]*=-1.0; }
    }

    if( topocentric )
    {
        rfid = get_rftrans_id( rfname, REFFRM_TOPOCENTRIC );
        rf=rftrans_from_id( rfid );
        if( ! rftrans_topocentric( rf ) ) 
        {
            sprintf(errmess,"Ref frame %s defined as both topocentric and geocentric",rfname);
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }
    }
    else
    {
        rfid = get_rftrans_id( rfname, iers ? REFFRM_IERS : REFFRM_GEOCENTRIC );
        rf=rftrans_from_id( rfid );
        if( rftrans_topocentric( rf ) && iers ) 
        {
            sprintf(errmess,"Topocentric ref frame %s cannot be defined with IERS parameters",rfname);
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }
    }

    if( origintype != REFFRM_ORIGIN_DEFAULT ) set_rftrans_origintype( rf, origintype );
    if( date != UNDEFINED_DATE ) set_rftrans_ref_date( rf, date );

    set_rftrans_parameters( rf, val, calcval, defined );
    return OK;
}

static int read_residual_format( CFG_FILE *cfg, char *string, void *, int, int code )
{
    char *types;
    char *column;
    char *nextcol;
    char *saveptr, save;
    char title1[81];
    char title2[81];
    char *ttl1, *ttl2;
    int width;

    types = strtok( string, " " );
    if( !types ) return MISSING_DATA;

    /* If types doesn't define valid types, then assume it is all and
     * use as column definition */

    nextcol=types;
    if( define_residual_formats( types, code ) == OK )
    {
        nextcol=0;
    }
    string = strtok( NULL, "\n");

    while( 1 )
    {
        char *endcol;
        char *number;
        int valid = 1;
        int i;

        if( nextcol )
        {
            column=nextcol;
            nextcol=0;
        }
        else
        {
            column = strtok( string, " " );
            string = strtok( NULL, "\n" );  /* Save a pointer to the rest */
        }
        if( ! column ) break;

        width = 0;
        ttl1 = NULL;
        ttl2 = NULL;
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


static int read_output_precision( CFG_FILE *cfg, char *string, void *, int, int )
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
        else if( strlen(ndp_str) != 1 || !ISDIGIT(ndp_str[0]) )
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

static int read_sort_option( CFG_FILE *, char *string, void *, int, int )
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


static int read_pb_use_datum_trans( CFG_FILE *cfg, char *string, void *, int, int )
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
    if( sts == OK ) set_bproj_use_datum_transformation( option );
    return sts;
}

static int read_flag_levels( CFG_FILE *cfg, char *string, void *, int, int )
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

static int read_error_type( CFG_FILE *cfg, char *string, void *, int, int )
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

static int read_error_summary( CFG_FILE *cfg, char *string, void *, int, int )
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

static int read_topocentre( CFG_FILE *cfg, char *string, void *, int, int )
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

static int read_gps_vertical( CFG_FILE *, char *string, void *, int, int )
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

static int load_plot_data( CFG_FILE *, char *, void *, int, int )
{
    return OK;
}

static int read_deformation_model(CFG_FILE *cfg, char *string, void *, int, int )
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


static int read_specification_command(CFG_FILE *cfg, char *string, void *, int, int )
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


static int read_spec_test_options(CFG_FILE *cfg, char *string, void *, int, int )
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


static int set_magic_number( CFG_FILE *, char *string ,void *, int, int )
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

static int read_configuration_command( CFG_FILE *cfg, char *string ,void *, int, int code )
{
    const char *cfgfile;
    char *ptr;
    char errmsg[60+MAX_FILENAME_LEN];
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
                    sprintf(errmsg,"Invalid data in configuration file %.*s",MAX_FILENAME_LEN,string);
                    send_config_error(cfg,INVALID_DATA,errmsg);
                }
            }
            else 
            {
                if( process_configuration_file( cfgfile, cfg_only ) != OK )
                {
                    sprintf(errmsg,"Invalid data in configuration file %.*s",MAX_FILENAME_LEN,string);
                    send_config_error(cfg,INVALID_DATA,errmsg);
                }
            }
        }
        else
        {
            sprintf(errmsg,"Cannot find configuration file %.*s",MAX_FILENAME_LEN,string);
            send_config_error( cfg, INVALID_DATA, errmsg );
        }
    }
    return OK;
}

