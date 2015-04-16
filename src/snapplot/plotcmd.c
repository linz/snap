#include "snapconfig.h"
/*
   $Log: plotcmd.c,v $
   Revision 1.4  1998/06/15 02:17:21  ccrook
   Fixed handling of "include" command in configuration file.

   Revision 1.3  1997/04/28 11:00:13  CHRIS
   Added reading and writing of observation listing options.

   Revision 1.2  1996/07/12 20:33:00  CHRIS
   Modified to support hidden stations.

   Revision 1.1  1996/01/03 22:24:06  CHRIS
   Initial revision

*/

#include <stdio.h>

#include "util/errdef.h"

#include "snap/stnadj.h"
#include "snap/snapglob.h"
#include "util/readcfg.h"
#include "snap/cfgprocs.h"
#include "plotconn.h"
#include "loadplot.h"
#include "backgrnd.h"
#include "plotstns.h"
#include "plotscal.h"
#include "plotpens.h"
#include "plotconn.h"
#include "plotcmd.h"
#include "util/fileutil.h"
#include "util/dstring.h"
#include "util/chkalloc.h"
#include "util/linklist.h"

#include <stdio.h>
#include <string.h>

#define COMMENT_CHAR '!'

static int load_plot_data( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_include_command( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_recode( CFG_FILE *cfg, char *string, void *value, int len, int code );

static int read_station_size_command( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_error_type_command( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_error_scale_command( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_station_colour_command( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_observation_colour_command( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_observation_options( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_observation_spacing_command( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_obs_listing_fields_command( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_obs_listing_order_command( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_key_command( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_highlight_command( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_text_rows( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_station_offset( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_station_font( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int process_station_list( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_ignore_offsets( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_config_menu_command( CFG_FILE *cfg, char *string, void *value, int len, int code );

static config_item snapplot_general_commands[] =
{
    {"title",job_title,ABSOLUTE,JOBTITLELEN,STORE_AS_STRING,CFG_ONEONLY,0},
    {"coordinate_file",NULL,ABSOLUTE,0,load_coordinate_file,CFG_REQUIRED+CFG_ONEONLY, 0},
    {"data_file",NULL,ABSOLUTE,0,load_data_file,0,0},
    {"recode",NULL,ABSOLUTE,0,read_recode,0,0},
    {"plot",NULL,ABSOLUTE,0,load_plot_data,0,0},
    {"include",NULL,ABSOLUTE,0,read_include_command,0,0},
    {NULL}
};

static config_item snapplot_binary_commands[] =
{
    {"plot",NULL,ABSOLUTE,0,load_plot_data,0,0},
    {"include",NULL,ABSOLUTE,0,read_include_command,0,0},
    {NULL}
};

/* Note: these commands must match those in write_config_file below ... */

#define HIDE_STATION 0
#define SHOW_STATION 1
#define HIGHLIGHT_STATION 2
#define UNHIGHLIGHT_STATION 3

static config_item snapplot_cfg_commands[] =
{
    {"station_size",NULL,ABSOLUTE,0,read_station_size_command,0,0},
    {"use_fixed_size_font",&use_default_font,ABSOLUTE,0,readcfg_boolean,0,0},
    {"ignore_station_offsets",NULL,ABSOLUTE,0,read_ignore_offsets,0,0},
    {"error_type",NULL,ABSOLUTE,0,read_error_type_command,0,0},
    {"error_scale",NULL,ABSOLUTE,0,read_error_scale_command,0,0},
    {"station_colours",NULL,ABSOLUTE,0,read_station_colour_command,0,0},
    {"observation_colours",NULL,ABSOLUTE,0,read_observation_colour_command,0,0},
    {"observation_options",NULL,ABSOLUTE,0,read_observation_options,0,0},
    {"observation_spacing",NULL,ABSOLUTE,0,read_observation_spacing_command,0,0},
    {"obs_listing_fields",NULL,ABSOLUTE,0,read_obs_listing_fields_command,0,0},
    {"obs_listing_order",NULL,ABSOLUTE,0,read_obs_listing_order_command,0,0},
    {"highlight_observations",NULL,ABSOLUTE,0,read_highlight_command,0,0},
    {"key",NULL,ABSOLUTE,0,read_key_command,0,0},
    {"text_rows",NULL,ABSOLUTE,0,read_text_rows,0,0},
    {"station_font",NULL,ABSOLUTE,0,read_station_font,CFG_ONEONLY,0},
    {"offset_station",NULL,ABSOLUTE,0,read_station_offset,0,0},
    {"hide",NULL,ABSOLUTE,0,process_station_list,0,HIDE_STATION},
    {"show",NULL,ABSOLUTE,0,process_station_list,0,SHOW_STATION},
    {"highlight",NULL,ABSOLUTE,0,process_station_list,0,HIGHLIGHT_STATION},
    {"unhighlight",NULL,ABSOLUTE,0,process_station_list,0,UNHIGHLIGHT_STATION},
    {"config_menu",NULL,ABSOLUTE,0,read_config_menu_command,0,0},

    {NULL}
};

static config_item *snapplot_commands = NULL;
static void *cfg_list = NULL;
static char *whitespace = " \t\r\n";

static void add_config_menu_item( const char *filename, char *text );

typedef struct config_menu_item_s
{
    char *menu_text;
    char *file_name;
    struct config_menu_item_s *next;
} config_menu_item;

config_menu_item *config_menu = NULL;
int config_menu_size = 0;

static int read_command_file( const char *file_name, int main_file  )
{
    CFG_FILE *cfg;
    int sts;

    if( ! file_exists( file_name ) ) return FILE_OPEN_ERROR;

    cfg = open_config_file( file_name, COMMENT_CHAR );
    if( cfg && snapplot_commands )
    {
        int options = CFG_IGNORE_BAD;
        if( main_file ) options |= CFG_CHECK_MISSING;
        set_config_read_options( cfg, options );
        sts = read_config_file( cfg, snapplot_commands );
        close_config_file( cfg );
        sts = sts ? INVALID_DATA : OK;
    }
    else
    {
        sts = FILE_OPEN_ERROR;
    }
    return sts;
}


int read_plot_command_file( char *command_file, int got_data )
{
    int sts;

    if( !got_data ) job_title[0] = 0;

    snapplot_commands = got_data ? snapplot_binary_commands :
                        snapplot_general_commands;

    sts = read_command_file( command_file, 1 );

    if( sts == OK && !job_title[0] )
    {
        strncpy( job_title, net->name, JOBTITLELEN );
        job_title[JOBTITLELEN] = 0;
    }

    return sts;
}



CFG_FILE *current_cfg = NULL;

int read_plot_configuration_file( const char *cfg_file )
{
    int sts;
    CFG_FILE *old_cfg;
    CFG_FILE *cfg;

    cfg = open_config_file( cfg_file, COMMENT_CHAR );
    if( cfg )
    {
        old_cfg = current_cfg;
        current_cfg = cfg;
        sts = read_config_file( cfg, snapplot_cfg_commands );
        close_config_file( cfg );
        current_cfg = old_cfg;
        sts = sts ? INVALID_DATA : OK;
    }
    else
    {
        sts = FILE_OPEN_ERROR;
    }
    return sts;
}

void abort_snapplot_config_file( void )
{
    if( current_cfg ) abort_config_file( current_cfg );
}

static char spec[MAX_FILENAME_LEN];

/* Add a configuration file to a list of files to process */

static void store_configuration_file( const char *fname )
{
    if( !cfg_list )
    {
        cfg_list = create_list( 0 );
    }
    add_to_list( cfg_list, copy_string(fname) );
}

int add_configuration_file( const char *fname )
{
    store_configuration_file( fname );
    return OK;
}

void add_default_configuration_files( void )
{
    int nch;
    char *spec;
    const char *cfgdir;

    cfgdir = system_config_dir();
    if( cfgdir )
    {
        spec=build_config_filespec( 0, 0, cfgdir,0,SNAPPLOT_CONFIG_SECTION, SNAPPLOT_CONFIG_FILE, 0 );
        if( file_exists( spec )) store_configuration_file( spec );
    }

    cfgdir = user_config_dir();
    if( cfgdir )
    {
        spec=build_config_filespec( 0, 0, cfgdir,0,SNAPPLOT_CONFIG_SECTION, SNAPPLOT_CONFIG_FILE, 0 );
        if( file_exists( spec )) store_configuration_file( spec );
    }

    spec=build_config_filespec( 0, 0, command_file, 1, 0, SNAPPLOT_CONFIG_FILE, 0 );
    if( file_exists( spec )) store_configuration_file( spec );


    nch = path_len( command_file, 1 );
    if( nch + strlen(SNAPPLOT_CONFIG_EXT) + 1 < MAX_FILENAME_LEN )
    {
        strncpy( spec, command_file, nch );
        strcpy( spec+nch, SNAPPLOT_CONFIG_EXT );
        if( file_exists( spec )) store_configuration_file( spec );
    }
}

int process_configuration_file_list( void )
{
    int sts = OK;
    char *fname;

    if( !cfg_list ) return OK;
    reset_list_pointer( cfg_list );
    while( NULL != (fname = (char *) next_list_item( cfg_list )) )
    {
        int fsts;
        fsts = read_plot_configuration_file( fname );
        if( fsts != OK ) sts = fsts;
        check_free( fname );
    }
    clear_list( cfg_list, NO_ACTION );
    return sts;
}


int process_configuration_file( char *fname )
{
    const char *fspec;
    int sts;
    fspec = find_file( fname, SNAPPLOT_CONFIG_EXT, 0, FF_TRYLOCAL, SNAPPLOT_CONFIG_SECTION );
    if( fspec )
    {
        sts = read_plot_configuration_file( fspec );
    }
    else
    {
        sts = FILE_OPEN_ERROR;
        handle_error(sts,"Cannot open plot configuration file",fname);
    }
    return sts;
}

// #pragma warning ( disable : 4100 )

static int read_include_command( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    const char *cmdfile;
    char *ptr;
    char errmsg[100];

    ptr = string;
    while( ptr && NULL != (cmdfile=strtok(ptr,whitespace)))
    {
        ptr = strtok(NULL,"\n");
        cmdfile = find_file( cmdfile, SNAPPLOT_CONFIG_EXT, cfg->name, 0, SNAPPLOT_CONFIG_SECTION );
        if( cmdfile )
        {

            if( read_command_file( cmdfile, 0 ) != OK )
            {
                sprintf(errmsg,"Invalid data in command file %.40s",string);
                send_config_error(cfg,INVALID_DATA,errmsg);
            }
        }
        else
        {
            sprintf(errmsg,"Cannot find command file %.40s",string);
            send_config_error( cfg, INVALID_DATA, errmsg );
        }
    }
    return OK;
}

static int read_recode( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    if( ! stnrecode ) stnrecode=create_stn_recode_map( net );
    if( read_station_recode_definition( stnrecode, string, cfg->name ) != OK )
    {
        send_config_error(cfg,INVALID_DATA,"Errors encountered in recode command" );
    }
    return OK;
}

static int load_plot_data( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *plot_command;
    char *plot_data;

    plot_command = strtok( string, " ");
    plot_data = strtok( NULL, "\n");

    if( !plot_command ) return MISSING_DATA;

    if( _stricmp( plot_command, "configuration" ) == 0 )
    {
        const char *cfgfile;
        if( ! plot_data ) return MISSING_DATA;
        cfgfile=find_file(plot_data,SNAPPLOT_CONFIG_EXT,cfg->name,0,SNAPPLOT_CONFIG_SECTION);
        if( !cfg ) return MISSING_DATA;
        if( ! cfgfile || add_configuration_file( cfgfile ) != OK )
        {
            char errmess[80];
            sprintf(errmess, "Cannot find configuration file %.40s",plot_data);
            send_config_error( cfg, INVALID_DATA, errmess );
        }
        return OK;
    }

    if( _stricmp( plot_command, "offset_station" ) == 0 )
    {
        return read_station_offset( cfg, plot_data, value, len, code );
    }

    if( _stricmp( plot_command, "background" ) == 0 )
    {
        char *fname;
        const char *fspec;
        char *crdsys;
        char *layer;
        coordsys *cs;
        fname = strtok( plot_data, whitespace);
        crdsys = strtok( NULL, whitespace );
        layer = strtok( NULL, whitespace );
        if( !fname )
        {
            send_config_error( cfg, MISSING_DATA, "Background file name missing" );
            return OK;
        }
        fspec = find_file( fname, ".dat", cfg->name, FF_TRYALL, SNAPPLOT_CONFIG_SECTION );
        if( !fspec )
        {
            char errmess[80];
            sprintf(errmess,"Cannot open background file %.40s",fname);
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }
        if( crdsys )
        {
            cs = load_coordsys( crdsys );
            if( !cs )
            {
                char errmess[80];
                sprintf( errmess,"Invalid coordinate system %.20s for background file",
                         crdsys );
                send_config_error( cfg, INVALID_DATA, errmess );
                return OK;
            }
            delete_coordsys( cs );
        }
        add_background_file( fspec, crdsys, layer );
        return OK;
    }

    if( _stricmp( plot_command, "projection" ) == 0 )
    {
        coordsys *cs;
        if( !plot_data ) return MISSING_DATA;

        cs = load_coordsys( plot_data );
        if( !cs )
        {
            send_config_error( cfg, INVALID_DATA,
                               "The plot projection specified is not a valid coordinate system code");
            return OK;
        }
        if( !is_projection(cs) )
        {
            send_config_error( cfg, INVALID_DATA,
                               "The coordinate system specified is not a projection");
            return OK;
        }

        set_plot_projection( cs );
        return OK;
    }

    /* The command cannot be processed here */

    return INVALID_DATA;
}


static int read_station_size_command( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *fld;
    char text = 1;
    char symbol = 1;
    int autoscl = 0;
    double size;
    fld = strtok( string, whitespace);
    if( fld )
    {
        if( _stricmp(fld,"text") == 0 ) symbol = 0;
        else if( _stricmp(fld,"symbol") == 0 ) text = 0;
        if( !text || !symbol ) fld = strtok( NULL, whitespace);
    }
    if( !fld ) return MISSING_DATA;
    if( sscanf(fld,"%lf",&size) != 1 ) return INVALID_DATA;
    fld = strtok( NULL, whitespace);
    if( fld )
    {
        if( _stricmp(fld,"times_default") != 0 ) return INVALID_DATA;
        autoscl = 1;
    }
    if( text ) set_stn_name_size( size, autoscl );
    if( symbol ) set_stn_symbol_size( size, autoscl );
    return OK;
}

static int read_error_type_command( CFG_FILE *cfg, char *string, void *value, int len, int code )
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
    set_confidence_limit();
    return OK;
}

static int read_error_scale_command( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *fld;
    char horizontal = 1;
    char vertical = 1;
    int autoscl = 0;
    double size;
    fld = strtok( string, whitespace);
    if( fld )
    {
        if( _stricmp(fld,"horizontal") == 0 ) vertical = 0;
        else if( _stricmp(fld,"vertical") == 0 ) horizontal = 0;
        if( !horizontal || !vertical) fld = strtok( NULL, whitespace);
    }
    if( !fld ) return MISSING_DATA;
    if( sscanf(fld,"%lf",&size) != 1 ) return INVALID_DATA;
    fld = strtok( NULL, whitespace);
    if( fld )
    {
        if( _stricmp(fld,"times_default") != 0 ) return INVALID_DATA;
        autoscl = 1;
    }
    if( horizontal ) set_errell_exaggeration( size, autoscl );
    if( vertical )   set_hgterr_exaggeration( size, autoscl );
    return OK;
}

static int read_observation_colour_command( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    return set_datapen_definition( string );
}

static int read_station_colour_command( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    int class_id = 0;
    if( _stricmp(string,"usage") != 0 ) class_id = network_class_id( net, string, 0 );
    setup_station_pens(class_id);
    return OK;
}

static int read_observation_options( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *s;
    for( s = strtok( string, whitespace ); s; s = strtok(NULL,whitespace))
    {
        if( _stricmp(s,"show_obs_directions") == 0 )
        {
            show_oneway_obs = 1;
        }
        else if( _stricmp(s,"no_show_obs_directions") == 0 )
        {
            show_oneway_obs = 0;
        }
        else if( _stricmp(s,"merge_all_obs") == 0 )
        {
            merge_common_obs = PCONN_ONE_CONNECTION;
        }
        else if( _stricmp(s,"merge_similar_obs") == 0 )
        {
            merge_common_obs = PCONN_DIFFERENT_TYPES;
        }
        else if( _stricmp(s,"no_merge_obs") == 0 )
        {
            merge_common_obs = PCONN_ALL_CONNECTIONS;
        }
        else if( _stricmp(s,"show_hidden_station_obs") == 0 )
        {
            show_hidden_stn_obs = 1;
        }
        else if( _stricmp(s,"no_show_hidden_station_obs") == 0 )
        {
            show_hidden_stn_obs = 0;
        }
        else
        {
            char errmess[80];
            sprintf(errmess,"Invalid option %.20s in observation_options command");
            send_config_error( cfg, INVALID_DATA, errmess );
        }
    }
    return OK;
}


static int read_observation_spacing_command( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *fld;
    int autoscl = 0;
    double size;
    fld = strtok( string, whitespace);
    if( !fld ) return MISSING_DATA;
    if( sscanf(fld,"%lf",&size) != 1 ) return INVALID_DATA;
    fld = strtok( NULL, whitespace);
    if( fld )
    {
        if( _stricmp(fld,"times_default") != 0 ) return INVALID_DATA;
        autoscl = 1;
    }
    offset_spacing = size;
    autospacing = autoscl;
    return OK;
}




static int read_obs_listing_fields_command( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    read_display_fields_definition( string );
    return OK;
}


static int read_obs_listing_order_command( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *fld;
    int order;
    fld = strtok( string, whitespace );
    order = get_display_field_code( fld );
    set_sres_sort_option( order );
    return OK;
}


static int read_key_command( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    int sts;
    sts = read_key_definition( string );
    if( sts == INCONSISTENT_DATA ) sts = OK;  /* Ignore non-existent pen codes */
    return sts;
}



static int read_highlight_command( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *s1, *s2;
    char need_value = 0;
    int option;
    double threshold = 0.0;
    char garbage[2];
    s1 = strtok( string, whitespace );
    s2 = strtok( NULL, whitespace );
    if( !s1 ) return MISSING_DATA;
    if( _stricmp( s1, "none" ) == 0 )
    {
        option = PCONN_HIGHLIGHT_NONE;
    }
    else if( _stricmp( s1, "to_stations" ) == 0 )
    {
        option = PCONN_HIGHLIGHT_IF_EITHER;
    }
    else if( _stricmp( s1, "between_stations" ) == 0 )
    {
        option = PCONN_HIGHLIGHT_IF_BOTH;
    }
    else if( _stricmp( s1, "std_residual" ) == 0 )
    {
        option = PCONN_HIGHLIGHT_SRES;
        need_value = 1;
    }
    else if( _stricmp( s1, "apost_std_residual" ) == 0 )
    {
        option = PCONN_HIGHLIGHT_APOST_SRES;
        need_value = 1;
    }
    else if( _stricmp( s1, "redundancy" ) == 0 )
    {
        option = PCONN_HIGHLIGHT_RFAC;
        need_value = 1;
    }
    else if( _stricmp( s1, "rejected" ) == 0 )
    {
        option = PCONN_HIGHLIGHT_REJECTED;
    }
    else if( _stricmp( s1, "unused" ) == 0 )
    {
        option = PCONN_HIGHLIGHT_UNUSED;
    }
    else
    {
        return INVALID_DATA;
    }
    if( need_value )
    {
        if( !s2 ) return MISSING_DATA;
        if( sscanf( s2, "%lf%1s", &threshold, garbage) != 1 ) return INVALID_DATA;
    }
    set_obs_highlight_option( option, threshold );
    return OK;
}

static int read_text_rows( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    int nlines;
    int sts;

    sts = readcfg_short( cfg, string, &nlines, len, code );
    // TODO: fix this: if( sts == OK ) set_text_rows( nlines );
    return sts;
}

/*================================================================*/



static void set_station_mode( int istn, int mode )
{

    switch( mode )
    {

    case HIDE_STATION: hide_station( istn ); break;
    case SHOW_STATION: unhide_station( istn ); break;
    case HIGHLIGHT_STATION: highlight_station( istn ); break;
    case UNHIGHLIGHT_STATION: unhighlight_station( istn ); break;
    }
}


static void process_station_list_file( CFG_FILE *cfg, char *name,
                                       int mode)
{
    char list_spec[MAX_FILENAME_LEN];
    FILE *list_file;
    char stn_code[21];

    build_filespec( list_spec, MAX_FILENAME_LEN, cmd_dir, name, DFLTSTLIST_EXT );
    list_file = fopen( list_spec, "r" );
    if( !list_file )
    {
        build_filespec( list_spec, MAX_FILENAME_LEN, NULL, name, DFLTSTLIST_EXT );
        list_file = fopen( list_spec, "r" );
    }

    if( !list_file )
    {
        char errmess[80];
        sprintf(errmess,"Cannot open station list file %.40s\n",name);
        send_config_error( cfg, INVALID_DATA, errmess );
        return;
    }

    while( fscanf(list_file,"%20s",stn_code) == 1 )
    {

        if( stn_code[0] == COMMENT_CHAR )
        {
            int c;
            do { c = fgetc(list_file); }
            while (c != '\n' && c != EOF);
        }
        else
        {
            int istn;

            _strupr(stn_code);
            istn = find_station( net, stn_code );

            /* Is the string matched as a station */

            if( istn )
            {
                set_station_mode( istn, mode );
                continue;
            }
            else
            {
                char errmess[100];
                sprintf( errmess,"Invalid station %.10s in list %.40s \n",stn_code,name);
                send_config_error( cfg, INVALID_DATA, errmess );
            }
        }
    }
    fclose( list_file );
}


static int process_station_list( CFG_FILE *cfg, char *string, void *value, int len, int mode )
{
    char *field;
    int setall, istn, ist1, ist2;
    char errmess[80], *delim;

    setall = 0;

    field = strtok( string, " " );

    if( field && _stricmp( field, "all" ) == 0 )
    {
        setall = 1;
        field = strtok(NULL, " ");
    }


    if( !setall && field && _stricmp( field, "all" ) == 0 )
    {
        setall = 1;
        field = strtok(NULL, " ");
    }

    if( setall )
    {
        for( istn = number_of_stations(net); istn; istn-- )
        {
            set_station_mode( istn, mode );
        }
    }

    else
    {
        for( ; field; field = strtok( NULL, " ") )
        {

            /* Included list of station names */

            if( field[0] == '@' && field[1] )
            {
                process_station_list_file( cfg, field+1, mode );
                continue;
            }


            if( _strnicmp(field,"order=",6) == 0 )
            {
                int orderId = network_order_id( net, field+6, 0 );
                for( istn = number_of_stations(net); istn; istn-- )
                {
                    station *st = stnptr(istn);
                    int iorder = get_station_class( st, net->orderclsid );
                    if( iorder == orderId )
                    {
                        set_station_mode( istn, mode );
                    }
                }
                continue;
            }

            _strupr(field);
            istn = find_station( net, field );

            /* Is the string matched as a station */

            if( istn )
            {
                set_station_mode( istn, mode );
                continue;
            }

            /* Is it matched as a range? */

            for( delim = field+1; *delim && *delim != '-'; delim++);
            if( *delim )
            {
                *delim = 0;
                if( 0 != (ist1=find_station( net,field)) &&
                        0 != (ist2=find_station( net,delim+1)) &&
                        ist2 >= ist1 )
                {


                    while( ist1 <= ist2 )
                    {
                        set_station_mode( ist1, mode );
                        ist1++;
                    }
                    continue;
                }
                *delim = '-';
            }

            /* Bother - it must be a mistake */

            sprintf(errmess,"Invalid station %s in list of stations",field);
            send_config_error(cfg,INVALID_DATA,errmess);
        }
    }

    return OK;
}

static int read_station_font( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    set_station_font( string );
    return OK;
}

static int read_station_offset( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    int istn;
    char *s1, *s2, *s3;
    double oe = 0.0, on = 0.0;
    int sts;
    s1 = strtok(string, whitespace );
    s2 = strtok( NULL, whitespace );
    s3 = strtok( NULL, whitespace );
    istn = find_station( net, s1 );
    if( !istn )
    {
        char buf[80];
        sprintf(buf,"Offset station %.20s does not exist",s1);
        send_config_error( cfg, INVALID_DATA, buf );
        return OK;
    }
    sts = readcfg_double( cfg, s2, &oe, len, code );
    if( sts == OK ) sts = readcfg_double( cfg, s3, &on, len, code );
    if( sts != OK )
    {
        send_config_error( cfg, INVALID_DATA, "Invalid coordinates in station offset");
        return OK;
    }
    set_station_offset( istn, oe, on );
    return OK;
}

static int read_ignore_offsets( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char ignore;
    int sts;
    sts = readcfg_boolean( cfg, string, &ignore, 0, 0 );
    if( sts == OK ) use_station_offsets( ignore ? 0 : 1 );
    return sts;
}


static int read_config_menu_command( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *s1, *s2;
    const char *fspec;
    s1 = strtok(string,whitespace);
    s2 = strtok(NULL,"\n");
    if( !s2 ) return MISSING_DATA;
    fspec = find_file( s1, SNAPPLOT_CONFIG_EXT, cfg->name, FF_TRYALL, SNAPPLOT_CONFIG_SECTION  );
    if( !fspec )
    {
        char buf[256];
        sprintf(buf,"Cannot find configuration file %.128s in config_menu command",s1);
        send_config_error( cfg, INVALID_DATA, buf);
        return OK;
    }
    add_config_menu_item( fspec, s2 );
    return OK;
}


int write_config_file( FILE *out, int key_only )
{
    char def[256];
    fprintf( out, "! SNAPPLOT configuration file\n\n");
    if( !key_only )
    {
        double val;
        int autoscl;
        int opt;
        double threshold;
        /*
        get_stn_name_size( &val, &autoscl );
        fprintf( out, "station_size text %.2lf%s\n",
           val, autoscl ? " times_default" : "" );
        fprintf( out, "use_fixed_size_font %s\n", use_default_font ? "yes" : "no");
        get_stn_symbol_size( &val, &autoscl );
        fprintf( out, "station_size symbol %.2lf%s\n",
           val, autoscl ? " times_default" : "" );
        */
        fprintf( out, "error_type %s", aposteriori_errors ? "aposteriori" : "apriori");
        if( use_confidence_limit )
        {
            fprintf(out," %.2lf confidence_limit\n",confidence_limit);
        }
        else
        {
            fprintf(out," %.1lf standard_error\n",confidence_limit);
        }
        get_errell_exaggeration( &val, &autoscl );
        fprintf( out, "error_scale horizontal %.2lf%s\n",
                 val, autoscl ? " times_default" : "" );;
        get_hgterr_exaggeration( &val, &autoscl );
        fprintf( out, "error_scale vertical %.2lf%s\n",
                 val, autoscl ? " times_default" : "" );
        fputs( "obs_listing_fields ",out);
        write_display_fields_definition( def, 256 );
        fputs( def, out );
        fputs( "\n", out );
        fprintf( out, "obs_listing_order %s\n",get_display_field_name(get_sres_sort_option()));
        fputs( "observation_options ", out );

        if( !show_oneway_obs ) fputs("no_", out);
        fputs( "show_obs_directions ", out);
        switch( merge_common_obs )
        {
        case PCONN_ONE_CONNECTION:
            fputs("merge_all_obs ",out); break;

        case PCONN_DIFFERENT_TYPES:
            fputs("merge_similar_obs ",out); break;

        default:
            fputs("no_merge_obs ",out); break;
        }
        if( !show_hidden_stn_obs ) fputs("no_", out);
        fputs( "show_hidden_station_obs", out);
        fputs("\n",out);
        fprintf(out,"observation_spacing %.2lf%s\n",offset_spacing,
                autospacing ? " times_default" : "" );
        fprintf( out,"%s ","highlight_observations");
        get_obs_highlight_option( &opt, &threshold );
        switch( opt )
        {
        case PCONN_HIGHLIGHT_IF_EITHER:
            fprintf( out, "%s\n", "to_stations" );
            break;
        case PCONN_HIGHLIGHT_IF_BOTH:
            fprintf( out, "%s\n", "between_stations" );
            break;
        case PCONN_HIGHLIGHT_REJECTED:
            fprintf( out, "%s\n", "rejected" );
            break;
        case PCONN_HIGHLIGHT_UNUSED:
            fprintf( out, "%s\n", "unused" );
            break;
        case PCONN_HIGHLIGHT_SRES:
            fprintf( out, "%s %.3lf\n", "std_residual", threshold );
            break;
        case PCONN_HIGHLIGHT_APOST_SRES:
            fprintf( out, "%s %.3lf\n", "apost_std_residual", threshold );
            break;
        case PCONN_HIGHLIGHT_RFAC:
            fprintf( out, "%s %.3lf\n", "redundancy", threshold );
            break;
        default:
            fprintf( out, "%s\n", "none" );
            break;
        }

        fprintf(out,"\n! Station code and name font\n");
        fprintf(out,"station_font %s\n",get_station_font());
        fprintf(out,"\n! Station offsets\n");
        fprintf( out, "ignore_station_offsets %s\n",using_station_offsets() ?
                 "no" : "yes" );
        if( offset_station_count() )
        {
            int istn;
            for( istn = 0; istn++ < station_count(); )
            {
                double oe, on;
                if( get_station_offset( istn, &oe, &on ) )
                {
                    fprintf( out, "offset_station %s  %.2lf %.2lf\n",
                             stnptr(istn)->Code, oe, on );
                }
            }
        }

        {
            int istn;
            int first;
            int nline;
            first = 1;
            nline = 0;
            for( istn = 0; istn++ < station_count(); )
            {
                if( station_highlighted( istn ) )
                {
                    if( first )
                    {
                        fprintf( out, "\n! Highlighted stations\nhighlight");
                        first = 0;
                    }
                    if( nline > 8 )
                    {
                        fprintf( out, "\nhighlight" );
                        nline = 0;
                    }
                    fprintf(out," %s",stnptr(istn)->Code );
                    nline++;
                }
            }
            if( first ) fprintf( out,"\n" );

        }

        {
            int istn;
            int first;
            int nline;
            first = 1;
            nline = 0;
            for( istn = 0; istn++ < station_count(); )
            {
                if( station_hidden( istn ) )
                {
                    if( first )
                    {
                        fprintf( out, "\n! Hidden stations\nhide");
                        first = 0;
                    }
                    if( nline > 8 )
                    {
                        fprintf( out, "\nhide" );
                        nline = 0;
                    }
                    fprintf(out," %s",stnptr(istn)->Code );
                    nline++;
                }
            }
            if( first ) fprintf( out,"\n" );

        }
        fprintf( out, "\n! Key defined as follows:\n\n");
    }
    get_stationpen_definition( def );
    fprintf(out,"station_colours %s\n",def );
    get_datapen_definition( def );
    fprintf(out,"observation_colours %s\n",def );
    print_key( out, "key" );
    return OK;
}

int save_configuration( char *cfgname )
{
    FILE *cfg =  fopen(cfgname,"w");
    if( !cfg )
    {
        return 0;
    }

    write_config_file( cfg, 0 );
    fclose( cfg );
    return 1;
}

void add_config_menu_item( const char *filename, char *text )
{
    config_menu_item *item;
    config_menu_item **itemptr;

    item = (config_menu_item *) check_malloc( sizeof(config_menu_item) );
    item->menu_text = copy_string( text );
    item->file_name = copy_string( filename );
    item->next = NULL;

    /* Append the item to the end of the list */
    itemptr = &config_menu;
    while( *itemptr ) { itemptr = &((*itemptr)->next); }
    *itemptr = item;

    config_menu_size++;
}

int config_menu_item_count()
{
    return config_menu_size;
}

static config_menu_item *get_config_menu( int i )
{
    config_menu_item *menu = config_menu;
    while( menu && i-- ) { menu = menu->next; }
    return menu;
}
char *config_menu_text( int i )
{
    config_menu_item *menu = get_config_menu(i);
    return menu ? menu->menu_text : NULL;
}

char *config_menu_filename( int i )
{
    config_menu_item *menu = get_config_menu(i);
    return menu ? menu->file_name : NULL;
}
