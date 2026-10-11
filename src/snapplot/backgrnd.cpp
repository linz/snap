#include "snapconfig.h"
/* Code to draw a background to a plot.  Background consists of a series of data
   files containing lines
     id x y    Start of feature on layer id
     0 x y     continuation of feature
   Any line not containing this is ignored.

   The command file specifies the coordinate system of the background - SNAP
   will attempt to convert this to the plot coordinate system */

/*
   $Log: backgrnd.c,v $
   Revision 1.1  1996/01/03 22:15:07  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include "coordsys/coordsys.h"
#include "backgrnd.h"
#include "plotstns.h"
#include "plotpens.h"
#include "plotscal.h"
#include "plotfunc.h"
#include "util/errdef.h"
#include "util/dstring.h"
#include "util/fieldscanner.hpp"
#include "util/pi.h"
#include "util/progress.h"
#include "util/fileutil.h"
#include "snapplot_util.h"

struct background_file
{
    std::string filename;
    std::optional<std::string> crdsysdef;
    std::optional<std::string> layer_name;
};

struct background_layer
{
    std::string layer_name;
    int input_id;
    int pen_id;
};

struct bkg_point
{
    int pen;
    double x, y;
};

static FILE *bkg_file = nullptr;
static std::vector<background_file> bkg_list;
static std::vector<background_layer> bkg_layers;
static long npts = 0;

void add_background_file( std::string_view fname, std::optional<std::string_view> crdsysdef, std::optional<std::string_view> layer )
{
    background_file bf;
    bf.filename = std::string( fname );
    if( crdsysdef ) bf.crdsysdef = std::string( *crdsysdef );
    if( layer ) bf.layer_name = std::string( *layer );
    bkg_list.push_back( std::move( bf ) );
}

static int add_layer( std::string_view name, int id )
{
    const int pen_id = boost::numeric_cast<int>( bkg_layers.size() ) + 1;
    bkg_layers.push_back( { std::string( name ), id, pen_id } );
    return pen_id;
}

static int pen_id_from_id( int id )
{
    for( const background_layer &bl : bkg_layers )
    {
        if( bl.input_id == id ) return bl.pen_id;
    }
    return add_layer( "Background " + std::to_string( id ), id );
}


static int pen_id_from_name( std::string_view name )
{
    for( const background_layer &bl : bkg_layers )
    {
        if( boost::algorithm::iequals( bl.layer_name, name ) ) return bl.pen_id;
    }
    return add_layer( name, 0 );
}

int background_layer_count( void )
{
    return boost::numeric_cast<int>( bkg_layers.size() );
}

const std::string &background_layer_name( int pen_id )
{
    static const std::string no_name;
    if( pen_id > background_layer_count() || pen_id <= 0 ) return no_name;
    return bkg_layers[pen_id - 1].layer_name;
}

static void load_background_file( const background_file &bf )
{
    FILE *in;
    coordsys *cs, *csp;
    coord_conversion cnv;
    char need_conversion;
    char got_conversion;
    char bad_coordsys;
    int file_pen_id;
    char firstpt;
    char inrec[256];
    double xyz[3];
    int nlines;
    long fpts;
    long flines;
    char input_latlon;

    in = fopen( bf.filename.c_str(), "r" );
    if( !in ) return;
    skip_utf8_bom(in);

    csp = plot_projection();
    bad_coordsys = 1;
    got_conversion = 0;
    cs = nullptr;
    need_conversion = 0;
    input_latlon = 0;

    if( bf.crdsysdef )
    {
        cs = load_coordsys( *bf.crdsysdef );
        if( cs )
        {
            need_conversion = ! identical_coordinate_systems( csp, cs );
            input_latlon = is_geodetic(cs);
            bad_coordsys = 0;
        }
    }

    if( bf.layer_name )
    {
        file_pen_id = pen_id_from_name( *bf.layer_name );
    }
    else
    {
        file_pen_id = 0;
    }

    print_log("\nLoading background file %s\n",bf.filename.c_str() );
    firstpt = 1;
    fpts = 0;
    flines = 0;
    nlines = 0;
    init_file_display(in);
    while( fgets(inrec,256,in) )
    {
        int pen;
        char oldfirstpt;
        bkg_point pt;
        oldfirstpt = firstpt;
        firstpt = 1;
        if( nlines++ == 50 )
        {
            update_file_display();
            nlines = 0;
        }
        if( compare_ignoring_case( inrec, "#layer", 6 ) == 0 )
        {
            FieldScanner scanner( std::string_view( inrec ).substr( 6 ) );
            const std::optional<std::string_view> layer = scanner.next();
            file_pen_id = 0;
            if( layer ) file_pen_id = pen_id_from_name( *layer );
            continue;
        }
        if( compare_ignoring_case( inrec, "#coordsys", 9 ) == 0 )
        {
            FieldScanner scanner( std::string_view( inrec ).substr( 9 ) );
            const std::optional<std::string_view> newcrdsys = scanner.next();
            if( cs ) { delete cs; cs = nullptr; }
            got_conversion = 0;
            bad_coordsys = 1;
            if( newcrdsys )
            {
                cs = load_coordsys( *newcrdsys );
                if( cs )
                {
                    need_conversion = ! identical_coordinate_systems( csp, cs );
                    input_latlon = is_geodetic(cs);
                    bad_coordsys = 0;
                }
            }
            continue;
        }
        if( bad_coordsys ) continue;
        if( sscanf(inrec,"%d%lf%lf",&pen,xyz+0,xyz+1) != 3 ) continue;
        if( input_latlon ) { xyz[0] *= DTOR; xyz[1] *= DTOR; }
        xyz[2] = 0.0;
        if( need_conversion )
        {
            if( !got_conversion )
            {
                if( cs )
                {
                    cnv = coord_conversion( cs, csp );
                }
                if( !cs || !cnv.valid )
                {
                    bad_coordsys = 1;
                    continue;
                }
                got_conversion = 1;
            }
            if( convert_coords(&cnv,xyz,NULL,xyz,NULL) != OK ) continue;
        }
        if( oldfirstpt && pen == 0 ) pen = 1;
        firstpt = 0;
        if( pen )
        {
            if( file_pen_id )
            {
                pen = file_pen_id;
            }
            else
            {
                pen = pen_id_from_id( pen );
            }
        }
        pt.pen = pen;
        pt.x = xyz[0];
        pt.y = xyz[1];
        fwrite( &pt, sizeof(pt), 1, bkg_file );
        npts++;
        fpts++;
        if( pen ) flines++;
    }
    end_file_display();
    print_log("%ld lines loaded\n",flines);
    fclose(in);
    if( cs ) delete cs;
}

void load_background_files( void )
{
    if( bkg_list.empty() ) return;
    if( !bkg_file ) bkg_file = snaptmpfile();
    if( !bkg_file ) return;
    npts = 0;
    for( const background_file &bf : bkg_list )
    {
        load_background_file( bf );
    }
}

int plot_background( map_plotter *plotter, int start )
{
    static int current_colour;
    long count;
    int visiblelayercount;
    int i;

    count = 250;
    if( start <= 0 )
    {
        if( !bkg_file ) return ALL_DONE;

        visiblelayercount = 0;
        for( i = 1; i <= background_layer_count(); i++ )
        {
            if( background_option(i) ) visiblelayercount++;
        }
        if( ! visiblelayercount ) return ALL_DONE;

        fseek( bkg_file, 0L, SEEK_SET );
        if( start < 0 ) count = npts+2;
        current_colour = 0;
    }

    while( count-- )
    {
        bkg_point pt;
        if( fread( &pt,sizeof(pt),1,bkg_file) != 1 ) return ALL_DONE;
        if( pt.pen )
        {
            current_colour = background_pen( pt.pen );
            if( ! background_option( pt.pen ) ) current_colour = 0;
        }
        if( !current_colour ) continue;
        LINE( plotter, pt.x, pt.y, pt.pen ? current_colour : CONTINUE_LINE );
    }

    return 1;
}
