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

#include <boost/algorithm/string/predicate.hpp>

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
#include "util/fieldscanner.hpp"

#include <cctype>
#include <optional>
#include <stdio.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string.h>
#include <string_view>
#include <vector>

#define COMMENT_CHAR '!'

static int load_plot_data( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_include_command( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );

static int read_station_size_command( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_error_type_command( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_error_scale_command( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_station_colour_command( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_observation_colour_command( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_observation_options( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_observation_spacing_command( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_obs_listing_fields_command( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_obs_listing_order_command( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_key_command( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_highlight_command( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_text_rows( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_station_offset( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_station_font( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int process_station_list( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_ignore_offsets( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_config_menu_command( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );

static config_item snapplot_general_commands[] =
{
    {"title",nullptr,CFG_ABSOLUTE,0,read_job_title_command,CFG_ONEONLY,0},
    {"coordinate_file",NULL,CFG_ABSOLUTE,0,load_coordinate_file,CFG_REQUIRED, 0},
    {"add_coordinate_file",NULL,CFG_ABSOLUTE,0,add_coordinate_file, 0, 0 },
    {"data_file",NULL,CFG_ABSOLUTE,0,load_data_file,0,0},
    {"classification",NULL,CFG_ABSOLUTE,0,read_classification_command,0,0},
    {"reweight_observations",NULL,CFG_ABSOLUTE,0,read_obs_modification_command,0,OBS_MOD_REWEIGHT},
    {"reject_observations",NULL,CFG_ABSOLUTE,0,read_obs_modification_command,0,OBS_MOD_REJECT},
    {"ignore_observations",NULL,CFG_ABSOLUTE,0,read_obs_modification_command,0,OBS_MOD_IGNORE},
    {"gps_antenna_height",NULL,CFG_ABSOLUTE,0,read_obs_modification_command,0,OBS_MOD_ANTENNA_OFFSET},    
    {"recode",NULL,CFG_ABSOLUTE,0,read_recode_command,0,0},
    {"plot",NULL,CFG_ABSOLUTE,0,load_plot_data,0,0},
    {"include",NULL,CFG_ABSOLUTE,0,read_include_command,0,0},
    {NULL}
};

static config_item snapplot_binary_commands[] =
{
    {"plot",NULL,CFG_ABSOLUTE,0,load_plot_data,0,0},
    {"include",NULL,CFG_ABSOLUTE,0,read_include_command,0,0},
    {NULL}
};

/* Note: these commands must match those in write_config_file below ... */

#define HIDE_STATION 0
#define SHOW_STATION 1
#define HIGHLIGHT_STATION 2
#define UNHIGHLIGHT_STATION 3

static config_item snapplot_cfg_commands[] =
{
    {"station_size",NULL,CFG_ABSOLUTE,0,read_station_size_command,0,0},
    {"use_fixed_size_font",&use_default_font,CFG_ABSOLUTE,0,readcfg_boolean,0,0},
    {"ignore_station_offsets",NULL,CFG_ABSOLUTE,0,read_ignore_offsets,0,0},
    {"error_type",NULL,CFG_ABSOLUTE,0,read_error_type_command,0,0},
    {"error_scale",NULL,CFG_ABSOLUTE,0,read_error_scale_command,0,0},
    {"station_colours",NULL,CFG_ABSOLUTE,0,read_station_colour_command,0,0},
    {"observation_colours",NULL,CFG_ABSOLUTE,0,read_observation_colour_command,0,0},
    {"observation_options",NULL,CFG_ABSOLUTE,0,read_observation_options,0,0},
    {"observation_spacing",NULL,CFG_ABSOLUTE,0,read_observation_spacing_command,0,0},
    {"obs_listing_fields",NULL,CFG_ABSOLUTE,0,read_obs_listing_fields_command,0,0},
    {"obs_listing_order",NULL,CFG_ABSOLUTE,0,read_obs_listing_order_command,0,0},
    {"highlight_observations",NULL,CFG_ABSOLUTE,0,read_highlight_command,0,0},
    {"key",NULL,CFG_ABSOLUTE,0,read_key_command,0,0},
    {"text_rows",NULL,CFG_ABSOLUTE,0,read_text_rows,0,0},
    {"station_font",NULL,CFG_ABSOLUTE,0,read_station_font,CFG_ONEONLY,0},
    {"offset_station",NULL,CFG_ABSOLUTE,0,read_station_offset,0,0},
    {"hide",NULL,CFG_ABSOLUTE,0,process_station_list,0,HIDE_STATION},
    {"show",NULL,CFG_ABSOLUTE,0,process_station_list,0,SHOW_STATION},
    {"highlight",NULL,CFG_ABSOLUTE,0,process_station_list,0,HIGHLIGHT_STATION},
    {"unhighlight",NULL,CFG_ABSOLUTE,0,process_station_list,0,UNHIGHLIGHT_STATION},
    {"config_menu",NULL,CFG_ABSOLUTE,0,read_config_menu_command,0,0},

    {NULL}
};

static config_item *snapplot_commands = NULL;
static std::vector<std::string> cfg_list;

static void add_config_menu_item( std::string_view filename, std::string_view text );

struct config_menu_item
{
    std::string menu_text;
    std::string file_name;
};

static std::vector<config_menu_item> config_menu;

static int read_command_file( const std::string &file_name, const int main_file )
{
    CFG_FILE *cfg;
    int sts;

    if( ! path_exists( file_name ) ) return FILE_OPEN_ERROR;

    cfg = open_config_file( file_name, COMMENT_CHAR );
    if( cfg && snapplot_commands )
    {
        int options = CFG_IGNORE_BAD | CFG_SET_PATH;
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


int read_plot_command_file( const std::string &fname, const int got_data )
{
    int sts;

    if( !got_data ) job_title.clear();

    snapplot_commands = got_data ? snapplot_binary_commands :
                        snapplot_general_commands;

    sts = read_command_file( fname, 1 );

    if( sts == OK && job_title.empty() && net->name )
    {
        job_title = net->name->substr( 0, JOBTITLELEN );
    }

    return sts;
}



CFG_FILE *current_cfg = NULL;

int read_plot_configuration_file( const std::string &cfg_file )
{
    int sts;
    CFG_FILE *old_cfg;
    CFG_FILE *cfg;

    cfg = open_config_file( cfg_file, COMMENT_CHAR );
    if( cfg )
    {
        old_cfg = current_cfg;
        current_cfg = cfg;
        set_config_read_options( cfg, CFG_SET_PATH );
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

/* Add a configuration file to a list of files to process */

void add_configuration_file( const std::string &fname )
{
    cfg_list.push_back( fname );
}

void add_default_configuration_files( void )
{
    std::string spec = build_config_filespec( system_config_dir(),false,SNAPPLOT_CONFIG_SECTION, SNAPPLOT_CONFIG_FILE, "" );
    if( path_exists( spec )) add_configuration_file( spec );

    if( auto userdir = user_config_dir() )
    {
        spec=build_config_filespec( *userdir,false,SNAPPLOT_CONFIG_SECTION, SNAPPLOT_CONFIG_FILE, "" );
        if( path_exists( spec )) add_configuration_file( spec );
    }

    spec=build_config_filespec( command_file->path, true, "", SNAPPLOT_CONFIG_FILE, "" );
    if( path_exists( spec )) add_configuration_file( spec );

    spec = std::filesystem::path( native_path(command_file->path) ).replace_extension().string() + SNAPPLOT_CONFIG_EXT;
    if( path_exists( spec )) add_configuration_file( spec );
}

int process_configuration_file_list( void )
{
    int sts = OK;

    for( size_t i = 0; i < cfg_list.size(); i++ )
    {
        // A copy, as reading a file can add to cfg_list and reallocate it
        const std::string fname = cfg_list[i];
        const int fsts = read_plot_configuration_file( fname );
        if( fsts != OK ) sts = fsts;
    }
    cfg_list.clear();
    return sts;
}


int process_configuration_file( const std::string &fname )
{
    int sts;
    auto fspec = find_file( fname, SNAPPLOT_CONFIG_EXT, std::nullopt, FF_TRYLOCAL, SNAPPLOT_CONFIG_SECTION );
    if( fspec )
    {
        sts = read_plot_configuration_file( *fspec );
    }
    else
    {
        sts = FILE_OPEN_ERROR;
        handle_error(sts,"Cannot open plot configuration file",fname);
    }
    return sts;
}

// #pragma warning ( disable : 4100 )

static int read_include_command( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    FieldScanner scanner(string);
    std::optional<std::string_view> cmdfile;
    while( (cmdfile = scanner.next()) )
    {
        auto resolved = find_file( std::string(*cmdfile), SNAPPLOT_CONFIG_EXT, std::optional<std::string>(cfg->name), FF_TRYNONE, SNAPPLOT_CONFIG_SECTION );
        if( resolved )
        {
            if( read_command_file( *resolved, 0 ) != OK )
            {
                send_config_error(cfg,INVALID_DATA,"Invalid data in command file " + std::string(*cmdfile));
            }
        }
        else
        {
            send_config_error( cfg, INVALID_DATA, "Cannot find command file " + std::string(*cmdfile) );
        }
    }
    return OK;
}

static int load_plot_data( CFG_FILE *cfg, std::string_view string, void *value, int len, int code )
{
    FieldScanner scanner(string);
    auto plot_command = scanner.next();
    const std::string_view plot_data = scanner.remainder();

    if( !plot_command ) return MISSING_DATA;

    if( boost::algorithm::iequals( *plot_command, "configuration" ) )
    {
        if( plot_data.empty() ) return MISSING_DATA;
        auto cfgfile=find_file(std::string(plot_data),SNAPPLOT_CONFIG_EXT,std::optional<std::string>(cfg->name),FF_TRYNONE,SNAPPLOT_CONFIG_SECTION);
        if( cfgfile )
        {
            add_configuration_file( *cfgfile );
        }
        else
        {
            send_config_error( cfg, INVALID_DATA, "Cannot find configuration file " + std::string(plot_data) );
        }
        return OK;
    }

    if( boost::algorithm::iequals( *plot_command, "offset_station" ) )
    {
        return read_station_offset( cfg, plot_data, value, len, code );
    }

    if( boost::algorithm::iequals( *plot_command, "background" ) )
    {
        FieldScanner dataScanner(plot_data);
        auto fname = dataScanner.next();
        auto crdsys = dataScanner.next();
        auto layer = dataScanner.next();
        if( !fname )
        {
            send_config_error( cfg, MISSING_DATA, "Background file name missing" );
            return OK;
        }
        auto fspec = find_file( std::string(*fname), ".dat", std::optional<std::string>(cfg->name), FF_TRYALL, SNAPPLOT_CONFIG_SECTION );
        if( !fspec )
        {
            send_config_error( cfg, INVALID_DATA, "Cannot open background file " + std::string(*fname) );
            return OK;
        }
        if( crdsys )
        {
            coordsys *cs = load_coordsys( *crdsys );
            if( !cs )
            {
                send_config_error( cfg, INVALID_DATA,
                                    "Invalid coordinate system " + std::string(*crdsys) + " for background file" );
                return OK;
            }
            delete cs;
        }
        add_background_file( *fspec, crdsys, layer );
        return OK;
    }

    if( boost::algorithm::iequals( *plot_command, "projection" ) )
    {
        if( plot_data.empty() ) return MISSING_DATA;

        coordsys *cs = load_coordsys( plot_data );
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


static int read_station_size_command( CFG_FILE *, std::string_view string, void *, int, int )
{
    char text = 1;
    char symbol = 1;
    int autoscl = 0;

    FieldScanner scanner(string);
    auto fld = scanner.next();
    if( fld )
    {
        if( boost::algorithm::iequals(*fld,"text") ) symbol = 0;
        else if( boost::algorithm::iequals(*fld,"symbol") ) text = 0;
        if( !text || !symbol ) fld = scanner.next();
    }
    if( !fld ) return MISSING_DATA;
    auto size = parse_leading<double>(*fld);
    if( !size ) return INVALID_DATA;
    fld = scanner.next();
    if( fld )
    {
        if( ! boost::algorithm::iequals(*fld,"times_default") ) return INVALID_DATA;
        autoscl = 1;
    }
    if( text ) set_stn_name_size( *size, autoscl );
    if( symbol ) set_stn_symbol_size( *size, autoscl );
    return OK;
}

static int read_error_type_command( CFG_FILE *cfg, std::string_view string, void *, int, int )
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
            if( !parsedConf )
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
    set_confidence_limit();
    return OK;
}

static int read_error_scale_command( CFG_FILE *, std::string_view string, void *, int, int )
{
    char horizontal = 1;
    char vertical = 1;
    int autoscl = 0;

    FieldScanner scanner(string);
    auto fld = scanner.next();
    if( fld )
    {
        if( boost::algorithm::iequals(*fld,"horizontal") ) vertical = 0;
        else if( boost::algorithm::iequals(*fld,"vertical") ) horizontal = 0;
        if( !horizontal || !vertical) fld = scanner.next();
    }
    if( !fld ) return MISSING_DATA;
    auto size = parse_leading<double>(*fld);
    if( !size ) return INVALID_DATA;
    fld = scanner.next();
    if( fld )
    {
        if( ! boost::algorithm::iequals(*fld,"times_default") ) return INVALID_DATA;
        autoscl = 1;
    }
    if( horizontal ) set_errell_exaggeration( *size, autoscl );
    if( vertical )   set_hgterr_exaggeration( *size, autoscl );
    return OK;
}

static int read_observation_colour_command( CFG_FILE *, std::string_view string, void *, int, int )
{
    return set_datapen_definition( string );
}

static int read_station_colour_command( CFG_FILE *, std::string_view string, void *, int, int )
{
    int class_id = 0;
    if( ! boost::algorithm::iequals(string,"usage") ) class_id = net->class_id( std::string(string), 0 );
    setup_station_pens(class_id);
    return OK;
}

static int read_observation_options( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    FieldScanner scanner(string);
    for( auto s = scanner.next(); s; s = scanner.next() )
    {
        if( boost::algorithm::iequals(*s,"show_obs_directions") )
        {
            show_oneway_obs = 1;
        }
        else if( boost::algorithm::iequals(*s,"no_show_obs_directions") )
        {
            show_oneway_obs = 0;
        }
        else if( boost::algorithm::iequals(*s,"merge_all_obs") )
        {
            merge_common_obs = PCONN_ONE_CONNECTION;
        }
        else if( boost::algorithm::iequals(*s,"merge_similar_obs") )
        {
            merge_common_obs = PCONN_DIFFERENT_TYPES;
        }
        else if( boost::algorithm::iequals(*s,"no_merge_obs") )
        {
            merge_common_obs = PCONN_ALL_CONNECTIONS;
        }
        else if( boost::algorithm::iequals(*s,"show_hidden_station_obs") )
        {
            show_hidden_stn_obs = 1;
        }
        else if( boost::algorithm::iequals(*s,"no_show_hidden_station_obs") )
        {
            show_hidden_stn_obs = 0;
        }
        else
        {
            send_config_error( cfg, INVALID_DATA, "Invalid option " + std::string(*s) + " in observation_options command" );
        }
    }
    return OK;
}

static int read_observation_spacing_command( CFG_FILE *, std::string_view string, void *, int, int )
{
    int autoscl = 0;

    FieldScanner scanner(string);
    auto fld = scanner.next();
    if( !fld ) return MISSING_DATA;
    auto size = parse_leading<double>(*fld);
    if( !size ) return INVALID_DATA;
    fld = scanner.next();
    if( fld )
    {
        if( ! boost::algorithm::iequals(*fld,"times_default") ) return INVALID_DATA;
        autoscl = 1;
    }
    offset_spacing = *size;
    autospacing = autoscl;
    return OK;
}

static int read_obs_listing_fields_command( CFG_FILE *, std::string_view string, void *, int, int )
{
    read_display_fields_definition( string );
    return OK;
}


static int read_obs_listing_order_command( CFG_FILE *, std::string_view string, void *, int, int )
{
    FieldScanner scanner(string);
    auto fld = scanner.next();
    int order = fld ? get_display_field_code( *fld ) : 0;
    set_sres_sort_option( order );
    return OK;
}


static int read_key_command( CFG_FILE *, std::string_view string, void *, int, int )
{
    int sts = read_key_definition( string );
    if( sts == INCONSISTENT_DATA ) sts = OK;  /* Ignore non-existent pen codes */
    return sts;
}



static int read_highlight_command( CFG_FILE *, std::string_view string, void *, int, int )
{
    char need_value = 0;
    int option;
    double threshold = 0.0;

    FieldScanner scanner(string);
    auto s1 = scanner.next();
    auto s2 = scanner.next();
    if( !s1 ) return MISSING_DATA;
    if( boost::algorithm::iequals( *s1, "none" ) )
    {
        option = PCONN_HIGHLIGHT_NONE;
    }
    else if( boost::algorithm::iequals( *s1, "to_stations" ) )
    {
        option = PCONN_HIGHLIGHT_IF_EITHER;
    }
    else if( boost::algorithm::iequals( *s1, "between_stations" ) )
    {
        option = PCONN_HIGHLIGHT_IF_BOTH;
    }
    else if( boost::algorithm::iequals( *s1, "std_residual" ) )
    {
        option = PCONN_HIGHLIGHT_SRES;
        need_value = 1;
    }
    else if( boost::algorithm::iequals( *s1, "apost_std_residual" ) )
    {
        option = PCONN_HIGHLIGHT_APOST_SRES;
        need_value = 1;
    }
    else if( boost::algorithm::iequals( *s1, "redundancy" ) )
    {
        option = PCONN_HIGHLIGHT_RFAC;
        need_value = 1;
    }
    else if( boost::algorithm::iequals( *s1, "rejected" ) )
    {
        option = PCONN_HIGHLIGHT_REJECTED;
    }
    else if( boost::algorithm::iequals( *s1, "unused" ) )
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
        auto parsedThreshold = parse_double(*s2);
        if( !parsedThreshold ) return INVALID_DATA;
        threshold = *parsedThreshold;
    }
    set_obs_highlight_option( option, threshold );
    return OK;
}

static int read_text_rows( CFG_FILE *cfg, std::string_view string, void *, int len, int code )
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


static void process_station_list_file( CFG_FILE *cfg, const std::string &name,
                                       int mode)
{
    std::string list_spec = build_filespec( command_file ? command_file->dir : "", name, DFLTSTLIST_EXT );
    std::ifstream list_file( list_spec );
    if( !list_file )
    {
        list_spec = build_filespec( "", name, DFLTSTLIST_EXT );
        list_file.clear();
        list_file.open( list_spec );
    }

    if( !list_file )
    {
        send_config_error( cfg, INVALID_DATA, "Cannot open station list file " + name );
        return;
    }

    skip_utf8_bom( list_file );

    std::string stn_code;
    while( list_file >> stn_code )
    {

        if( stn_code[0] == COMMENT_CHAR )
        {
            list_file.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );
        }
        else
        {
            int istn;

            for( char &c : stn_code ) c = static_cast<char>( std::toupper( static_cast<unsigned char>(c) ) );
            istn = find_station( net, stn_code );

            /* Is the string matched as a station */

            if( istn )
            {
                set_station_mode( istn, mode );
                continue;
            }
            else
            {
                send_config_error( cfg, INVALID_DATA, "Invalid station " + stn_code + " in list " + name );
            }
        }
    }
}


static int process_station_list( CFG_FILE *cfg, std::string_view string, void *, int, int mode )
{
    int istn, ist1, ist2;
    bool setall = false;

    FieldScanner scanner(string);
    auto field = scanner.next();

    if( field && boost::algorithm::iequals( *field, "all" ) )
    {
        setall = true;
        field = scanner.next();
    }


    if( !setall && field && boost::algorithm::iequals( *field, "all" ) )
    {
        setall = true;
        field = scanner.next();
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
        for( ; field; field = scanner.next() )
        {
            const std::string_view f = *field;

            /* Included list of station names */

            if( f.size() > 1 && f[0] == '@' )
            {
                process_station_list_file( cfg, std::string(f.substr(1)), mode );
                continue;
            }


            if( boost::algorithm::istarts_with( f, "order=" ) )
            {
                int orderId = net->order_id( std::string(f.substr(6)), 0 );
                for( istn = number_of_stations(net); istn; istn-- )
                {
                    station *st = stnptr(istn);
                    int iorder = st->get_class( net->orderclsid );
                    if( iorder == orderId )
                    {
                        set_station_mode( istn, mode );
                    }
                }
                continue;
            }

            std::string code(f);
            for( char &c : code ) c = static_cast<char>( std::toupper( static_cast<unsigned char>(c) ) );

            istn = find_station( net, code );

            /* Is the string matched as a station */

            if( istn )
            {
                set_station_mode( istn, mode );
                continue;
            }

            /* Is it matched as a range? */

            auto delimPos = code.find( '-', 1 );
            if( delimPos != std::string::npos )
            {
                const std::string first = code.substr( 0, delimPos );
                const std::string second = code.substr( delimPos + 1 );
                if( 0 != (ist1=find_station( net,first)) &&
                        0 != (ist2=find_station( net,second)) &&
                        ist2 >= ist1 )
                {


                    while( ist1 <= ist2 )
                    {
                        set_station_mode( ist1, mode );
                        ist1++;
                    }
                    continue;
                }
            }

            /* Bother - it must be a mistake */

            send_config_error(cfg,INVALID_DATA,"Invalid station " + code + " in list of stations");
        }
    }

    return OK;
}

static int read_station_font( CFG_FILE *, std::string_view string, void *, int, int )
{
    set_station_font( std::string(string) );
    return OK;
}

static int read_station_offset( CFG_FILE *cfg, std::string_view string, void *, int len, int code )
{
    double oe = 0.0, on = 0.0;
    int sts;

    FieldScanner scanner(string);
    auto s1 = scanner.next();
    auto s2 = scanner.next();
    auto s3 = scanner.next();

    if( !s1 ) return MISSING_DATA;

    const int istn = find_station( net, *s1 );
    if( !istn )
    {
        send_config_error( cfg, INVALID_DATA, "Offset station " + std::string(*s1) + " does not exist" );
        return OK;
    }
    sts = readcfg_double( cfg, s2 ? *s2 : std::string_view(), &oe, len, code );
    if( sts == OK ) sts = readcfg_double( cfg, s3 ? *s3 : std::string_view(), &on, len, code );
    if( sts != OK )
    {
        send_config_error( cfg, INVALID_DATA, "Invalid coordinates in station offset");
        return OK;
    }
    set_station_offset( istn, oe, on );
    return OK;
}

static int read_ignore_offsets( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    char ignore = 0;
    int sts;
    sts = readcfg_boolean( cfg, string, &ignore, 0, 0 );
    if( sts == OK ) use_station_offsets( ignore ? 0 : 1 );
    return sts;
}


static int read_config_menu_command( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    FieldScanner scanner(string);
    auto s1 = scanner.next();
    const std::string_view s2 = scanner.remainder();
    if( s2.empty() ) return MISSING_DATA;
    // s2 non-empty implies s1 was successfully read (remainder can't be
    // non-empty if next() found nothing to consume it past).
    auto fspec = find_file( std::string(*s1), SNAPPLOT_CONFIG_EXT, std::optional<std::string>(cfg->name), FF_TRYALL, SNAPPLOT_CONFIG_SECTION  );
    if( !fspec )
    {
        send_config_error( cfg, INVALID_DATA, "Cannot find configuration file " + std::string(*s1) + " in config_menu command" );
        return OK;
    }
    add_config_menu_item( *fspec, s2 );
    return OK;
}


int write_config_file( FILE *out, int key_only )
{
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
        fputs( write_display_fields_definition().c_str(), out );
        fputs( "\n", out );
        fprintf( out, "obs_listing_order %s\n",get_display_field_name(get_sres_sort_option()).value_or("").c_str());
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
        fprintf(out,"station_font %s\n",get_station_font().c_str());
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
                             stnptr(istn)->Code.c_str(), oe, on );
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
                    fprintf(out," %s",stnptr(istn)->Code.c_str() );
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
                    fprintf(out," %s",stnptr(istn)->Code.c_str() );
                    nline++;
                }
            }
            if( first ) fprintf( out,"\n" );

        }
        fprintf( out, "\n! Key defined as follows:\n\n");
    }
    fprintf(out,"station_colours %s\n",get_stationpen_definition().c_str() );
    fprintf(out,"observation_colours %s\n",get_datapen_definition().c_str() );
    print_key( out, "key" );
    return OK;
}

int save_configuration( const std::string &cfgname )
{
    FILE *cfg =  fopen(cfgname.c_str(),"w");
    if( !cfg )
    {
        return 0;
    }

    write_config_file( cfg, 0 );
    fclose( cfg );
    return 1;
}

void add_config_menu_item( std::string_view filename, std::string_view text )
{
    config_menu.push_back( { std::string(text), std::string(filename) } );
}

int config_menu_item_count()
{
    return static_cast<int>( config_menu.size() );
}

static const config_menu_item *get_config_menu( int i )
{
    if( i < 0 || static_cast<size_t>(i) >= config_menu.size() ) return nullptr;
    return &config_menu[i];
}

std::string_view config_menu_text( int i )
{
    const config_menu_item *menu = get_config_menu(i);
    return menu ? std::string_view(menu->menu_text) : std::string_view("");
}

std::string_view config_menu_filename( int i )
{
    const config_menu_item *menu = get_config_menu(i);
    return menu ? std::string_view(menu->file_name) : std::string_view("");
}
