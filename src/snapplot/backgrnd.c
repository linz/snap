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

#include "coordsys/coordsys.h"
#include "backgrnd.h"
#include "plotstns.h"
#include "plotpens.h"
#include "plotscal.h"
#include "plotfunc.h"
#include "util/errdef.h"
#include "util/dstring.h"
#include "util/linklist.h"
#include "util/pi.h"
#include "util/progress.h"
#include "util/fileutil.h"
#include "snapplot_util.h"

static char rcsid[]="$Id: backgrnd.c,v 1.1 1996/01/03 22:15:07 CHRIS Exp $";

typedef struct
{
    char *filename;
    char *crdsysdef;
    char *layer_name;
} background_file;

typedef struct
{
    char *layer_name;
    int input_id;
    int pen_id;
} background_layer;

typedef struct
{
    int pen;
    double x, y;
} bkg_point;

static FILE *bkg_file = NULL;
static void *bkg_list = NULL;
static void *bkg_layers = NULL;
static int npens = 0;
static long npts = 0;
static char *whitespace = " \r\n\t";

void add_background_file( char *fname, char *crdsysdef, char *layer )
{
    background_file *bf;
    if( !bkg_list )
    {
        bkg_list = create_list( sizeof( background_file ) );
    }
    bf = (background_file *) add_to_list( bkg_list, NEW_ITEM );
    bf->filename = copy_string( fname );
    bf->crdsysdef = crdsysdef ? copy_string( crdsysdef ) : NULL;
    bf->layer_name = layer ? copy_string( layer ) : NULL;
}

static int add_layer( char *name, int id )
{
    background_layer *bl;
    if( ! bkg_layers )
    {
        bkg_layers = create_list( sizeof( background_layer ) );
    }
    bl = (background_layer *) add_to_list( bkg_layers, NEW_ITEM );
    bl->layer_name = copy_string( name );
    bl->input_id = id;
    bl->pen_id = ++npens;
    return npens;
}

static int pen_id_from_id( int id )
{
    background_layer *bl;
    char pen_name[40];
    if( bkg_layers )
    {
        reset_list_pointer( bkg_layers );
        while( NULL != (bl = (background_layer *) next_list_item( bkg_layers )) )
        {
            if( bl->input_id == id ) return bl->pen_id;
        }
    }
    sprintf(pen_name,"Background %d",id);
    return add_layer(pen_name,id);
}


static int pen_id_from_name( char *name )
{
    background_layer *bl;
    if( bkg_layers )
    {
        reset_list_pointer( bkg_layers );
        while( NULL != (bl = (background_layer *) next_list_item( bkg_layers )) )
        {
            if( _stricmp( bl->layer_name, name ) == 0 ) return bl->pen_id;
        }
    }
    return add_layer(name,0);
}

int background_layer_count( void )
{
    return npens;
}

char *background_layer_name( int pen_id )
{
    background_layer *bl;
    if( pen_id > npens || pen_id <= 0 ) return NULL;
    reset_list_pointer( bkg_layers );
    while( NULL != (bl = (background_layer *) next_list_item( bkg_layers )) )
    {
        if( bl->pen_id == pen_id ) return bl->layer_name;
    }
    return NULL;
}

static void load_background_file( background_file *bf )
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

    in = fopen( bf->filename, "r" );
    if( !in ) return;

    csp = plot_projection();
    bad_coordsys = 1;
    got_conversion = 0;
    cs = NULL;
    need_conversion = 0;
    input_latlon = 0;

    if( bf->crdsysdef )
    {
        cs = load_coordsys( bf->crdsysdef );
        if( cs )
        {
            need_conversion = ! identical_coordinate_systems( csp, cs );
            input_latlon = is_geodetic(cs);
            bad_coordsys = 0;
        }
    }

    if( bf->layer_name )
    {
        file_pen_id = pen_id_from_name( bf->layer_name );
    }
    else
    {
        file_pen_id = 0;
    }

    print_log("\nLoading background file %s\n",bf->filename );
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
        if( _strnicmp( inrec, "#layer", 6 ) == 0 )
        {
            char *layer;
            layer = strtok( inrec+6, whitespace );
            file_pen_id = 0;
            if( layer ) file_pen_id = pen_id_from_name( layer );
            continue;
        }
        if( _strnicmp( inrec, "#coordsys", 9 ) == 0 )
        {
            char *newcrdsys;
            newcrdsys = strtok( inrec+9, whitespace );
            if( cs ) { delete_coordsys( cs ); cs = NULL; }
            got_conversion = 0;
            bad_coordsys = 1;
            if( newcrdsys )
            {
                cs = load_coordsys( newcrdsys );
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
                if( !cs || define_coord_conversion( &cnv, cs, csp ) != OK )
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
    if( cs ) delete_coordsys( cs );
}

void load_background_files( void )
{
    background_file *bf;
    if( ! bkg_list ) return;
    if( !bkg_file ) bkg_file = snaptmpfile();
    if( !bkg_file ) return;
    reset_list_pointer( bkg_list );
    npts = 0;
    while( NULL != (bf = (background_file *) next_list_item( bkg_list )) )
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
