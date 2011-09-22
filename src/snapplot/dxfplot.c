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

struct dxfcolour
{
    unsigned char id;
    unsigned char R;
    unsigned char G;
    unsigned char B;
};

static dxfcolour colourtable[] =
{
    {1,255,0,0},
    {2,255,255,0},
    {3,0,255,0},
    {4,0,255,255},
    {5,0,0,255},
    {6,255,0,255},
    {7,255,255,255},
    {8,65,65,65},
    {9,128,128,128},
    {10,255,0,0},
    {11,255,170,170},
    {12,189,0,0},
    {13,189,126,126},
    {14,129,0,0},
    {15,129,86,86},
    {16,104,0,0},
    {17,104,69,69},
    {18,79,0,0},
    {19,79,53,53},
    {20,255,63,0},
    {21,255,191,170},
    {22,189,46,0},
    {23,189,141,126},
    {24,129,31,0},
    {25,129,96,86},
    {26,104,25,0},
    {27,104,78,69},
    {28,79,19,0},
    {29,79,59,53},
    {30,255,127,0},
    {31,255,212,170},
    {32,189,94,0},
    {33,189,157,126},
    {34,129,64,0},
    {35,129,107,86},
    {36,104,52,0},
    {37,104,86,69},
    {38,79,39,0},
    {39,79,66,53},
    {40,255,191,0},
    {41,255,234,170},
    {42,189,141,0},
    {43,189,173,126},
    {44,129,96,0},
    {45,129,118,86},
    {46,104,78,0},
    {47,104,95,69},
    {48,79,59,0},
    {49,79,73,53},
    {50,255,255,0},
    {51,255,255,170},
    {52,189,189,0},
    {53,189,189,126},
    {54,129,129,0},
    {55,129,129,86},
    {56,104,104,0},
    {57,104,104,69},
    {58,79,79,0},
    {59,79,79,53},
    {60,191,255,0},
    {61,234,255,170},
    {62,141,189,0},
    {63,173,189,126},
    {64,96,129,0},
    {65,118,129,86},
    {66,78,104,0},
    {67,95,104,69},
    {68,59,79,0},
    {69,73,79,53},
    {70,127,255,0},
    {71,212,255,170},
    {72,94,189,0},
    {73,157,189,126},
    {74,64,129,0},
    {75,107,129,86},
    {76,52,104,0},
    {77,86,104,69},
    {78,39,79,0},
    {79,66,79,53},
    {80,63,255,0},
    {81,191,255,170},
    {82,46,189,0},
    {83,141,189,126},
    {84,31,129,0},
    {85,96,129,86},
    {86,25,104,0},
    {87,78,104,69},
    {88,19,79,0},
    {89,59,79,53},
    {90,0,255,0},
    {91,170,255,170},
    {92,0,189,0},
    {93,126,189,126},
    {94,0,129,0},
    {95,86,129,86},
    {96,0,104,0},
    {97,69,104,69},
    {98,0,79,0},
    {99,53,79,53},
    {100,0,255,63},
    {101,170,255,191},
    {102,0,189,46},
    {103,126,189,141},
    {104,0,129,31},
    {105,86,129,96},
    {106,0,104,25},
    {107,69,104,78},
    {108,0,79,19},
    {109,53,79,59},
    {110,0,255,127},
    {111,170,255,212},
    {112,0,189,94},
    {113,126,189,157},
    {114,0,129,64},
    {115,86,129,107},
    {116,0,104,52},
    {117,69,104,86},
    {118,0,79,39},
    {119,53,79,66},
    {120,0,255,191},
    {121,170,255,234},
    {122,0,189,141},
    {123,126,189,173},
    {124,0,129,96},
    {125,86,129,118},
    {126,0,104,78},
    {127,69,104,95},
    {128,0,79,59},
    {129,53,79,73},
    {130,0,255,255},
    {131,170,255,255},
    {132,0,189,189},
    {133,126,189,189},
    {134,0,129,129},
    {135,86,129,129},
    {136,0,104,104},
    {137,69,104,104},
    {138,0,79,79},
    {139,53,79,79},
    {140,0,191,255},
    {141,170,234,255},
    {142,0,141,189},
    {143,126,173,189},
    {144,0,96,129},
    {145,86,118,129},
    {146,0,78,104},
    {147,69,95,104},
    {148,0,59,79},
    {149,53,73,79},
    {150,0,127,255},
    {151,170,212,255},
    {152,0,94,189},
    {153,126,157,189},
    {154,0,64,129},
    {155,86,107,129},
    {156,0,52,104},
    {157,69,86,104},
    {158,0,39,79},
    {159,53,66,79},
    {160,0,63,255},
    {161,170,191,255},
    {162,0,46,189},
    {163,126,141,189},
    {164,0,31,129},
    {165,86,96,129},
    {166,0,25,104},
    {167,69,78,104},
    {168,0,19,79},
    {169,53,59,79},
    {170,0,0,255},
    {171,170,170,255},
    {172,0,0,189},
    {173,126,126,189},
    {174,0,0,129},
    {175,86,86,129},
    {176,0,0,104},
    {177,69,69,104},
    {178,0,0,79},
    {179,53,53,79},
    {180,63,0,255},
    {181,191,170,255},
    {182,46,0,189},
    {183,141,126,189},
    {184,31,0,129},
    {185,96,86,129},
    {186,25,0,104},
    {187,78,69,104},
    {188,19,0,79},
    {189,59,53,79},
    {190,127,0,255},
    {191,212,170,255},
    {192,94,0,189},
    {193,157,126,189},
    {194,64,0,129},
    {195,107,86,129},
    {196,52,0,104},
    {197,86,69,104},
    {198,39,0,79},
    {199,66,53,79},
    {200,191,0,255},
    {201,234,170,255},
    {202,141,0,189},
    {203,173,126,189},
    {204,96,0,129},
    {205,118,86,129},
    {206,78,0,104},
    {207,95,69,104},
    {208,59,0,79},
    {209,73,53,79},
    {210,255,0,255},
    {211,255,170,255},
    {212,189,0,189},
    {213,189,126,189},
    {214,129,0,129},
    {215,129,86,129},
    {216,104,0,104},
    {217,104,69,104},
    {218,79,0,79},
    {219,79,53,79},
    {220,255,0,191},
    {221,255,170,234},
    {222,189,0,141},
    {223,189,126,173},
    {224,129,0,96},
    {225,129,86,118},
    {226,104,0,78},
    {227,104,69,95},
    {228,79,0,59},
    {229,79,53,73},
    {230,255,0,127},
    {231,255,170,212},
    {232,189,0,94},
    {233,189,126,157},
    {234,129,0,64},
    {235,129,86,107},
    {236,104,0,52},
    {237,104,69,86},
    {238,79,0,39},
    {239,79,53,66},
    {240,255,0,63},
    {241,255,170,191},
    {242,189,0,46},
    {243,189,126,141},
    {244,129,0,31},
    {245,129,86,96},
    {246,104,0,25},
    {247,104,69,78},
    {248,79,0,19},
    {249,79,53,59},
    {250,51,51,51},
    {251,80,80,80},
    {252,105,105,105},
    {253,130,130,130},
    {254,190,190,190},
    {255,255,255,255},
    {0,0,0}
};

static void clear_dxf_layers( void )
{
    int i;
    if( layer_name )
    {
        for( i = 0; i<=nlayer; i++ ) { if( layer_name[i] ) check_free( layer_name[i] ); }
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
    int nused = 0;
    clear_dxf_layers();
    npen = pen_count();
    if( npen <= 0 ) return nused;
    layer_name = (char **) check_malloc( sizeof( char * ) * (npen+1) );
    layer_name[0] = copy_string("ANNOTATION");
    nused = 1;
    for( i=1; i<=npen; i++)
    {
        char *name = pen_name(i-1);
        layer_name[i] = 0;
        if( !name || ! name[0] || ! pen_has_colour(i-1) ) continue;
        nused++;
        layer_name[i] = copy_string(name);
        _strupr( layer_name[i] );
        for( c = layer_name[i]; *c; c++ ) if( *c == ' ') *c = '_';
        for( int j = 0; j < i; j++ )
        {
            if( layer_name[j] && stricmp(layer_name[j],layer_name[i]) == 0 )
            {
                char buf[40];
                sprintf(buf,"_LAYER_%04d",i);
                check_free( layer_name[i] );
                layer_name[i] = copy_string(buf);
            }
        }
    }
    nlayer = npen;
    return nused;
}

static int dxf_colour_id( unsigned char red, unsigned char green, unsigned char blue )
{
    int distance = 256*3;
    int colour = 1;
    for( dxfcolour *c = colourtable; c->id > 0; c++ )
    {
        int dist = abs(red - c->R) + abs(green - c->G) + abs(blue - c->B);
        if( dist < distance ) { colour = c->id; distance = dist; }
    }
    return colour;
}

static void set_layer( int pen )
{
    if( pen > 0 && pen <= nlayer && layer_name[pen] )
    {
        cur_layer = layer_name[pen];
    }
    else if( pen < 0 )
    {
        cur_layer = default_layer;
    }
    else
    {
        cur_layer = layer_name[0];
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

        int usedlayers = setup_dxf_layers( );
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
        fprintf(dxf," 70\n1\n");
        fprintf(dxf,"  0\nLTYPE\n");
        fprintf(dxf,"  2\nCONTINUOUS\n");
        fprintf(dxf," 70\n64\n");
        fprintf(dxf,"  3\nSolid line\n");
        fprintf(dxf," 72\n65\n");
        fprintf(dxf," 73\n0\n");
        fprintf(dxf," 40\n0.0\n");
        fprintf(dxf,"  0\nENDTAB\n");

        fprintf(dxf,"  0\nTABLE\n");
        fprintf(dxf,"  2\nLAYER\n");
        fprintf(dxf," 70\n%d\n",usedlayers);

        for( i = 0; i <= nlayer; i++ )
        {
            unsigned char red, green, blue;
            int colourid = 1;
            if( ! layer_name[i] ) continue;
            if( i > 0 )
            {
                get_pen_colour( i-1, red, green, blue );
                int colourid = dxf_colour_id( red, green, blue );
            }
            fprintf(dxf,"  0\nLAYER\n  2\n%s\n 70\n64\n 62\n%d\n  6\nCONTINUOUS\n",
                    layer_name[i],colourid);
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
    fprintf(dxf," 70\n0\n");
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


