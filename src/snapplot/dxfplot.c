#include "snapconfig.h"
/* Routines for plotting obstacles to a DXF file */

/*
   $Log: dxfplot.c,v $
   Revision 1.2  1996/09/06 20:13:36  CHRIS
   Expanded DXF file to include header information - especially table of layers.

   Revision 1.1  1996/01/03 22:15:53  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "dxfplot.h"
#include "util/chkalloc.h"
#include "snap/snapglob.h"
#include "snap/stnadj.h"
#include "plotscal.h"
#include "plotstns.h"
#include "plotconn.h"
#include "plotpens.h"
#include "plotfunc.h"
#include "backgrnd.h"
#include "trimmer.h"

#include "util/dstring.h"
#include "util/errdef.h"
#include "util/pi.h"

static char rcsid[]="$Id: dxfplot.c,v 1.2 1996/09/06 20:13:36 CHRIS Exp $";

static FILE *dxf = NULL;

static double save_x1, save_y1;
static double save_x2, save_y2;
static int nppt;
static int inpoly;
static int precision = 2;

static char **layer_name = NULL;
static int nlayer = 0;
static char *cur_layer;
static char *default_layer = "0";

static Trimmer tr;
static char TrimLines;

static void clear_dxf_layers( void )
{
    int i;
    if( layer_name )
    {
        for( i = 0; i<nlayer; i++ ) check_free( layer_name[i] );
        check_free( layer_name );
        layer_name = NULL;
        nlayer = 0;
    }
}

static int setup_dxf_layers( void )
{
    int i;
    char *c;
    int npen;
    clear_dxf_layers();
    npen = pen_count();
    if( npen <= 0 ) return 0;
    layer_name = (char **) check_malloc( sizeof( char * ) * npen );
    for( i=0; i<npen; i++)
    {
        layer_name[i] = copy_string(pen_name(i));
        _strupr( layer_name[i] );
        for( c = layer_name[i]; *c; c++ ) if( *c == ' ') *c = '_';
    }
    nlayer = npen;
    return 0;
}

static void set_layer( int pen )
{
    if( pen > 0 && pen <= nlayer )
    {
        cur_layer = layer_name[pen-1];
    }
    else
    {
        cur_layer = default_layer;
    }
}


static void write_symbol_blocks();

int open_dxf_file( const char *dxfname )
{
    int i;
    dxf = fopen(dxfname,"w");
    inpoly = 0;
    nppt = 0;
    cur_layer = default_layer;
    if( dxf )
    {
        /* Initiallize layers etc.. */

        setup_dxf_layers( );
        fprintf(dxf,"  0\nSECTION\n");
        fprintf(dxf,"  2\nHEADER\n");
        fprintf(dxf,"  9\n$LTSCALE\n");
        fprintf(dxf," 40\n1.0\n");
        fprintf(dxf,"  9\n$LUNITS\n");
        fprintf(dxf," 70\n2\n");
        fprintf(dxf,"  9\n$LUPREC\n");
        fprintf(dxf," 70\n2\n");
        fprintf(dxf,"  9\n$AUNITS\n");
        fprintf(dxf," 70\n0\n");
        fprintf(dxf,"  9\n$AUPREC\n");
        fprintf(dxf," 70\n6\n");
        fprintf(dxf,"  0\nENDSEC\n");

        fprintf(dxf,"  0\nSECTION\n");
        fprintf(dxf,"  2\nTABLES\n");

        fprintf(dxf,"  0\nTABLE\n");
        fprintf(dxf,"  2\nLTYPE\n");
        fprintf(dxf," 70\n     2\n");
        fprintf(dxf,"  0\nLTYPE\n");
        fprintf(dxf,"  2\nCONTINUOUS\n");
        fprintf(dxf," 70\n    64\n");
        fprintf(dxf,"  3\nSolid line\n");
        fprintf(dxf," 72\n    65\n");
        fprintf(dxf," 73\n     0\n");
        fprintf(dxf," 40\n0.0\n");
        fprintf(dxf,"  0\nENDTAB\n");

        fprintf(dxf,"  0\nTABLE\n");
        fprintf(dxf,"  2\nLAYER\n");
        fprintf(dxf," 70\n43\n");

        for( i = 0; i < nlayer; i++ )
        {
            fprintf(dxf,"  0\nLAYER\n  2\n%s\n 70\n0\n 62\n0\n", layer_name[i]);
        }
        fprintf(dxf,"  0\nENDTAB\n");

        fprintf(dxf,"  0\nENDSEC\n");

        fprintf(dxf,"  0\nSECTION\n");
        fprintf(dxf,"  2\nBLOCKS\n");

        write_symbol_blocks();


        fprintf(dxf,"  0\nENDSEC\n");

        fprintf(dxf,"  0\nSECTION\n");
        fprintf(dxf,"  2\nENTITIES\n");
    }
    return dxf ? OK : FILE_OPEN_ERROR;
}

static void end_line( void )
{
    if( inpoly )
    {
        fprintf(dxf,"  0\nSEQEND\n");
        inpoly = 0;
    }
    else if( nppt > 1 )
    {
        fprintf(dxf,"  0\nLINE\n  8\n%s\n",cur_layer);
        fprintf(dxf," 10\n%.*lf\n 20\n%.*lf\n 11\n%.*lf\n 21\n%.*lf\n",
                precision, save_x1, precision, save_y1, precision, save_x2, precision, save_y2 );
    }
    nppt = 0;
}

static void start_polyline( void )
{
    fprintf(dxf,"  0\nPOLYLINE\n  8\n%s\n 66\n1\n",cur_layer);
    inpoly = 1;
}


static void write_poly_vertex( double x, double y )
{
    fprintf(dxf,"  0\nVERTEX\n  8\n%s\n", cur_layer);
    fprintf(dxf," 10\n%.*lf\n 20\n%.*lf\n",precision,x,precision,y);
}

static int write_dxf_point( double x, double y, int pen )
{
    if( !dxf ) { return FILE_OPEN_ERROR; }

    if( pen != 0 ) { end_line(); set_layer(pen); }

    if( inpoly )
    {
        write_poly_vertex( x, y );
    }
    else if( nppt == 2 )
    {
        start_polyline();
        write_poly_vertex( save_x1, save_y1 );
        write_poly_vertex( save_x2, save_y2 );
        write_poly_vertex( x, y );
        nppt = 0;
    }
    else if( nppt == 1 )
    {
        save_x2 = x; save_y2 = y; nppt = 2;
    }
    else
    {
        save_x1 = x; save_y1 = y; nppt = 1;
    }
    return OK;
}


static int write_dxf_text( double x, double y, int pen, double size,
                           double angle, char *text )
{

    if( !dxf ) return FILE_OPEN_ERROR;
    end_line();
    set_layer( pen );

    fprintf(dxf,"  0\nTEXT\n  8\n%s\n",cur_layer);
    fprintf(dxf," 10\n%.*lf\n 20\n%.*lf\n",precision,x,precision,y);
    fprintf(dxf," 40\n%.*lf\n  1\n%s\n",precision,size,text);
    fprintf(dxf," 50\n%.6lf\n",angle);

    return OK;
}

static int write_dxf_circle( double x, double y, double rad, int pen )
{
    if( !dxf ) return FILE_OPEN_ERROR;
    end_line();
    set_layer( pen );
    fprintf(dxf,"  0\nCIRCLE\n  8\n%s\n",cur_layer);
    fprintf(dxf,"  10\n%.*lf\n 20\n%.*lf\n",precision,x, precision,y );
    fprintf(dxf,"  40\n%.*lf\n",precision,rad);

    return OK;
}

static int write_dxf_blockref( double x, double y, char *blkname, double blocksize, int pen )
{
    if( !dxf ) return FILE_OPEN_ERROR;
    end_line();
    set_layer( pen );

    fprintf(dxf,"  0\nINSERT\n");
    fprintf(dxf,"  8\n%s\n",cur_layer);
    fprintf(dxf," 10\n%.*lf\n 20\n%.*lf\n",precision,x, precision,y );
    fprintf(dxf,"  2\n%s\n",blkname);
    fprintf(dxf," 41\n%.*lf\n 42\n%.*lf\n",precision,blocksize,precision,blocksize );

    return OK;
}


static void start_block( char *blockname )
{
    fprintf(dxf,"  0\nBLOCK\n");
    fprintf(dxf,"  8\n0\n");
    fprintf(dxf,"  2\n%s\n",blockname);
    fprintf(dxf," 10\n0.0\n");
    fprintf(dxf," 20\n0.0\n");
    fprintf(dxf,"  3\n%s\n",blockname);
}

static void end_block( void )
{
    fprintf(dxf,"  0\nENDBLK\n");
}

static void write_symbol_blocks()
{

    double s1;
    double s2;
    double s3;
    double px;
    double py;

    s1 = 0.5;
    s2 = 0.25;
    s3 = 0.43333333333333;
    px = 0;
    py = 0;

    /* Creates a DXF block for each symbol type */

    start_block("free_station");
    write_dxf_circle( 0.0, 0.0, s1, -1 );
    end_block();

    start_block("fixed_station");
    write_dxf_point( px, py+s1, -1 );
    write_dxf_point( px+s3, py-s2, 0 );
    write_dxf_point( px-s3, py-s2, 0 );
    write_dxf_point( px, py+s1, 0 );
    write_dxf_point( px, py-s1, -1 );
    write_dxf_point( px+s3, py+s2, 0 );
    write_dxf_point( px-s3, py+s2, 0 );
    write_dxf_point( px, py-s1, 0 );
    end_line();
    end_block();

    start_block("hor_fixed_station");
    write_dxf_point( px, py+s1, -1 );
    write_dxf_point( px+s3, py-s2, 0 );
    write_dxf_point( px-s3, py-s2, 0 );
    write_dxf_point( px, py+s1, 0 );
    end_line();
    end_block();

    start_block("vrt_fixed_station");
    write_dxf_point( px, py-s1, -1 );
    write_dxf_point( px+s3, py+s2, 0 );
    write_dxf_point( px-s3, py+s2, 0 );
    write_dxf_point( px, py-s1, 0 );
    end_line();
    end_block();

    start_block("rejected_station");
    write_dxf_point( px-s1, py-s1, -1 );
    write_dxf_point( px+s1, py-s1, 0 );
    write_dxf_point( px+s1, py+s1, 0 );
    write_dxf_point( px-s1, py+s1, 0 );
    write_dxf_point( px-s1, py-s1, 0 );
    end_line();
    end_block();


}

/*===================================================================*/
/* DXF plotting routines                                             */


#pragma warning (disable: 4100)

static void dxf_line( void *dummy, double x, double y, int pen, int dashed )
{
    int ntrpt;
    static int lastPen;
    char start;
    if( !TrimLines )
    {
        write_dxf_point( x, y, pen == CONTINUE_LINE ? 0 : pen + 1 );
        return;
    }
    if( pen != CONTINUE_LINE ) lastPen = pen + 1;
    ntrpt = AddTrimmerPoint( &tr, x, y, pen == CONTINUE_LINE  ? 0 : 1 );
    while( ntrpt-- )
    {
        NextTrimmedPoint( &tr, &x, &y, &start );
        write_dxf_point( x, y, start ? lastPen : 0 );
    }
}

static void dxf_text( void *dummy, double x, double y, double size, int pen, char *text )
{
    write_dxf_text( x, y, pen+1, size, 0.0, text );
}

#define MAXSEP 0.1
#define MININC 0.1


static void dxf_ellipse( void *dummy, double x, double y, double a, double b, double az, int pen )
{

    int i,ncd;
    double sep,r,rad,ai,ca,sa,co,so,ci,si,xe,ye;

    if( a == b )
    {
        write_dxf_circle( x, y, a, pen+1 );
        return;
    }

    /* Determine the acceptable chord arc separation */

    sep = MAXSEP;
    r = a>b ? a : b;
    rad = r;
    if( rad < sep )
    {
        ai = PI;
    }
    else
    {
        ai = sqrt( 8.0 * sep / rad );
    }
    if( ai < MININC ) ai = MININC;
    ncd = ceil( 2.0*PI / ai );
    if( ncd < 1 ) ncd = 1;
    ai = 2.0*PI/ncd;
    ca = 1.0;
    sa = 0.0;
    co = cos(az);
    so = sin(az);
    ci = cos(ai);
    si = sin(ai);

    pen = pen+1;

    for( i=0; i<=ncd; i++ )
    {

        xe = ca * a;
        ye = sa * b;
        write_dxf_point( x + xe*co - ye*so, y + xe*so + ye*co, pen );
        pen = 0;
        xe = ca*ci-sa*si;
        sa = ca*si+sa*ci;
        ca = xe;
    }

}


static void dxf_symbol( void *dummy, double px, double py, int pen, int symbol )
{
    char *blockname;
    switch( symbol )
    {
    case FREE_STN_SYM: blockname="free_station"; break;
    case FIXED_STN_SYM: blockname="fixed_station"; break;
    case HOR_FIXED_STN_SYM: blockname="hor_fixed_station"; break;
    case VRT_FIXED_STN_SYM: blockname="vrt_fixed_station"; break;
    case REJECTED_STN_SYM: blockname="rejected_station"; break;
    default: blockname="free_station"; break;
    }

    write_dxf_blockref( px, py, blockname, stn_symbol_size, pen );

}

int close_dxf_file()
{
    if( !dxf ) return FILE_OPEN_ERROR;
    end_line();
    fprintf(dxf,"  0\nENDSEC\n  0\nEOF\n");
    fclose(dxf);
    dxf = NULL;

    /* Clear out the memory used for dxf pen names */

    clear_dxf_layers();
    return OK;
}




#define NOTE(txt)  dxf_text(NULL, xt,yt,size,pen,txt); yt += spacing

void write_dxf_title_block( map_plotter *plotter )
{
    int nch, pen;
    double spacing, size, xt, yt, xs;
    char text[80];
    double oldsymsize = stn_symbol_size;

    pen = nlayer + 1;  /* Not one of the defined layers - text goes to default */

    stn_symbol_size = stn_name_size;

    write_dxf_point( plot_emin, plot_nmin, pen );
    write_dxf_point( plot_emin, plot_nmax, 0 );
    write_dxf_point( plot_emax, plot_nmax, 0 );
    write_dxf_point( plot_emax, plot_nmin, 0 );
    write_dxf_point( plot_emin, plot_nmin, 0 );

    size = stn_symbol_size;
    if( size <= 0.0 ) size = 10.0;
    size *= 1.5;
    spacing = - size * 1.5;

    xt = plot_emin;
    yt = plot_nmin + spacing * 3;

    NOTE( job_title );

    if(run_time[0])
    {
        sprintf(text,"Run at %s",run_time);
        NOTE( text );
    }

    sprintf(text,"Coordinate system: %.60s",plot_projection()->name);
    NOTE( text );

    if( got_covariances() )
    {
        sprintf(text,"Displaying %s ", aposteriori_errors ? "aposteriori" : "apriori");
        nch = strlen( text );
        if( use_confidence_limit )
        {
            sprintf(text+nch,"%.2lf%% confidence limits",confidence_limit);
        }
        else
        {
            strcpy(text+nch,"standard errors");
        }
        NOTE( text );

        if( dimension != 1 )
        {
            sprintf(text,"Ellipses exaggerated %.0lf times",errell_scale);
            NOTE( text );
        }

        if( dimension != 2 )
        {
            sprintf(text,"Height errors exaggerated %.0lf times",hgterr_scale);
            NOTE( text );
        }
    }
    yt += spacing;
    xs = xt + stn_symbol_size/2.0;
    xt += size*3;

    switch ( dimension )
    {
    case 1: dxf_symbol( NULL, xs, yt+stn_symbol_size/2.0, VRT_FIXED_STN_PEN, HOR_FIXED_STN_SYM); break;
    case 2: dxf_symbol( NULL, xs, yt+stn_symbol_size/2.0, HOR_FIXED_STN_PEN, HOR_FIXED_STN_SYM); break;
    case 3: dxf_symbol( NULL, xs, yt+stn_symbol_size/2.0, FIXED_STN_PEN, FIXED_STN_SYM); break;
    }
    NOTE( "Fixed station" );

    if( dimension == 3 )
    {
        dxf_symbol( NULL, xs, yt+stn_symbol_size/2.0, HOR_FIXED_STN_PEN, HOR_FIXED_STN_SYM );
        NOTE("Fixed horizontally");
        dxf_symbol( NULL, xs, yt+stn_symbol_size/2.0, VRT_FIXED_STN_PEN, VRT_FIXED_STN_SYM );
        NOTE("Fixed vertically");
    }

    dxf_symbol( NULL, xs, yt+stn_symbol_size/2.0, FREE_STN_PEN, FREE_STN_SYM );
    NOTE("Adjusted station");
    dxf_symbol( NULL, xs, yt+stn_symbol_size/2.0, REJECTED_STN_PEN, REJECTED_STN_SYM );
    NOTE("Rejected station");

    stn_symbol_size = oldsymsize;
}


int plot_dxf( void )
{
    int pconn_option;
    double data_offset;

    /* Set pointers to dxf functions */

    map_plotter plotter;
    plotter.plotobj = NULL;
    plotter.line_func = dxf_line;
    plotter.text_func = dxf_text;
    plotter.ellipse_func = dxf_ellipse;
    // TODO: Fix up symbols in DXF plotting
    // Could use code from original plotstns?
    plotter.symbol_func = dxf_symbol;
    plotter.symbol_size_func = NULL;

    pconn_option = merge_common_obs;
    if( show_oneway_obs ) pconn_option |= PCONN_SHOW_ONEWAY_OBS;
    data_offset = calc_obs_offset();

    SetTrimmerExtents( &tr, plot_emin, plot_nmin, plot_emax, plot_nmax );
    stn_symbol_size = (fabs( plot_emax-plot_emin) + fabs( plot_nmax-plot_nmin))/500.0;


    /* Plot the data */

    TrimLines = 1;
    plot_background(&plotter, -1);
    plot_stations(&plotter, -1, 0);
    plot_connections(&plotter, -1, pconn_option, data_offset, 0);
    plot_covariances(&plotter, -1);
    plot_height_errors(&plotter, -1);
    plot_adjustments( &plotter, -1 );
    plot_station_names(&plotter, -1);

    /* Plot the descriptive text */

    TrimLines = 0;
    write_dxf_title_block( &plotter );

    return 0;
}


