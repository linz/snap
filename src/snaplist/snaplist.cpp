#include "snapconfig.h"

/*
   $Log: snaplist.c,v $
   Revision 1.6  2003/10/27 21:59:36  ccrook
   Fixed calculation of ellipsoidal distance to account for curvature of the earth:wq

   Revision 1.5  2003/05/14 01:50:59  ccrook
   Added facility to include coordinate changes in output listing

   Revision 1.4  2003/03/13 02:45:40  ccrook
   Updated snaplist to allow the from and to mark names to be included in the
   output listing.

   Revision 1.3  2002/10/09 00:58:21  ccrook
   Fixed bug in handling of column names in configuration file.

   Revision 1.2  2001/06/27 23:31:52  ccrook
   Updated snaplist to include facility to tabulate station data.

   Revision 1.1  1996/01/03 22:49:56  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <string>
#include <string_view>
#include <boost/algorithm/string/predicate.hpp>
#include "util/fieldscanner.hpp"
#include "util/snapctype.h"

#include "util/errdef.h"
#include "util/chkalloc.h"
#include "util/fileutil.h"
#include "util/linklist.h"

#include "util/binfile.h"
#include "snapdata/survdata.h"
#include "snap/bindata.h"
#include "snap/stnadj.h"
#include "snap/snapglob.h"
#include "snap/snapglob_bin.h"
#include "util/classify.h"
#include "snap/survfile.h"
#include "snap/rftrans.h"
#include "snap/rftrndmp.h"
#include "network/network.h"
#include "coordsys/coordsys.h"
#include "util/readcfg.h"
#include "util/dstring.h"
#include "util/dms.h"
#include "util/pi.h"
#include "util/getversion.h"

static coord_conversion to_xyz;
static coord_conversion from_xyz;
static ellipsoid *el;
static station dummy1, dummy2;

struct covariance
{
    double emax, emin, az;
    double sehgt;
};

static covariance *covar;

enum {JST_LEFT, JST_CENTRE, JST_RIGHT };
enum {QT_NONE, QT_QUOTE, QT_LITERAL };
enum {TYPE_STRING, TYPE_PSTRING, TYPE_DOUBLE, TYPE_ANGLE };

#define MAX_HEADERS 3

struct column_def
{
    const char *name;
    int width;
    int ndp;
    int quote;
    int just;
    int type;
    void *data;
    void *format;
    char *prefix;
    char *suffix;
    char *header[MAX_HEADERS];
    int extralen;
};

static char *fromStn;
static char *toStn;
static const char *fromStnName;
static const char *toStnName;
static double obs_ell_dist;
static double calc_ell_dist;
static double ell_dist_err;
static double ppm_ell_dist_err;
static double rf_ell_dist_err;
static double obs_prj_brng;
static double calc_prj_brng;
static double prj_brng_err;
static double ppm_prj_brng_err;
static double rf_prj_brng_err;
static double hor_vec_err;
static double ppm_hor_vec_err;
static double rf_hor_vec_err;

#define MAX_DELIM 1
static char quote[MAX_DELIM+1] = { '"', 0 };
static char delim[MAX_DELIM+1] = { ',', 0 };
static char escape[MAX_DELIM+1] = { '"', 0 };
static char qescape[MAX_DELIM+MAX_DELIM+1];
static char qquote[MAX_DELIM+MAX_DELIM+1];
static char nqescape[MAX_DELIM+MAX_DELIM+1];
static char nqquote[MAX_DELIM+MAX_DELIM+1];
static char nqdelim[MAX_DELIM+MAX_DELIM+1];
static char nqnewline[MAX_DELIM+MAX_DELIM+1];
static char canquote = 0;

static FILE *out;
static BINARY_FILE *b;

#define MAXCLASS 20
static int classid[MAXCLASS];
static std::array<std::string, MAXCLASS> classname;
static const char *classvalue[MAXCLASS];
static const char *blankvalue = "";
static std::array<std::string, MAXCLASS> classvaluestore;
static int nclass = 0;

static column_def classcol = { "",0,0,0,JST_LEFT,TYPE_PSTRING,NULL,NULL };

static column_def obs_valid_columns[] =
{
    { "from",0,0,0,JST_LEFT,TYPE_PSTRING,&fromStn,NULL},
    { "to",0,0,0,JST_LEFT,TYPE_PSTRING,&toStn,NULL},
    { "from_name",0,0,0,JST_LEFT,TYPE_PSTRING,&fromStnName,NULL},
    { "to_name",0,0,0,JST_LEFT,TYPE_PSTRING,&toStnName,NULL},
    { "obs_ell_dist",0,2,0,JST_RIGHT,TYPE_DOUBLE,&obs_ell_dist,NULL},
    { "calc_ell_dist",0,2,0,JST_RIGHT,TYPE_DOUBLE,&calc_ell_dist,NULL},
    { "ell_dist_err",0,2,0,JST_RIGHT,TYPE_DOUBLE,&ell_dist_err,NULL},
    { "ppm_ell_dist_err",0,0,0,JST_RIGHT,TYPE_DOUBLE,&ppm_ell_dist_err,NULL},
    { "rf_ell_dist_err",0,0,0,JST_RIGHT,TYPE_DOUBLE,&rf_ell_dist_err,NULL},
    { "obs_prj_brng",0,0,0,JST_RIGHT,TYPE_ANGLE,&obs_prj_brng,NULL},
    { "calc_prj_brng",0,0,0,JST_RIGHT,TYPE_ANGLE,&calc_prj_brng,NULL},
    { "prj_brng_err",0,0,0,JST_RIGHT,TYPE_DOUBLE,&prj_brng_err,NULL},
    { "ppm_prj_brng_err",0,0,0,JST_RIGHT,TYPE_DOUBLE,&ppm_prj_brng_err,NULL},
    { "rf_prj_brng_err",0,0,0,JST_RIGHT,TYPE_DOUBLE,&rf_prj_brng_err,NULL},
    { "hor_vec_err",0,2,0,JST_RIGHT,TYPE_DOUBLE,&hor_vec_err,NULL},
    { "ppm_hor_vec_err",0,0,0,JST_RIGHT,TYPE_DOUBLE,&ppm_hor_vec_err,NULL},
    { "rf_hor_vec_err",0,0,0,JST_RIGHT,TYPE_DOUBLE,&rf_hor_vec_err,NULL},
    { NULL }
};

static const char *stn_code;
static const char *stn_name;
static const char *stn_order;
static std::string stn_order_store;
static double stn_northing;
static double stn_easting;
static double stn_height;
static double stn_h_max_error;
static double stn_h_min_error;
static double stn_h_max_brng;
static double stn_de;
static double stn_dn;
static double stn_dh;

static column_def stn_valid_columns[] =
{
    { "code",0,0,0,JST_LEFT,TYPE_PSTRING,&stn_code,NULL},
    { "name",0,0,0,JST_LEFT,TYPE_PSTRING,&stn_name,NULL},
    { "order",0,0,0,JST_LEFT,TYPE_PSTRING,&stn_order,NULL},
    { "northing",0,4,0,JST_RIGHT,TYPE_DOUBLE,&stn_northing,NULL},
    { "easting",0,4,0,JST_RIGHT,TYPE_DOUBLE,&stn_easting,NULL},
    { "latitude",0,9,0,JST_RIGHT,TYPE_DOUBLE,&stn_northing,NULL},
    { "longitude",0,9,0,JST_RIGHT,TYPE_DOUBLE,&stn_easting,NULL},
    { "height",0,4,0,JST_RIGHT,TYPE_DOUBLE,&stn_height,NULL},
    { "h_max_error",0,4,0,JST_RIGHT,TYPE_DOUBLE,&stn_h_max_error,NULL},
    { "h_min_error",0,4,0,JST_RIGHT,TYPE_DOUBLE,&stn_h_min_error,NULL},
    { "h_max_brng",0,3,0,JST_RIGHT,TYPE_DOUBLE,&stn_h_max_brng,NULL},
    { "change_east",0,4,0,JST_RIGHT,TYPE_DOUBLE,&stn_de,NULL},
    { "change_north",0,4,0,JST_RIGHT,TYPE_DOUBLE,&stn_dn,NULL},
    { "change_up",0,4,0,JST_RIGHT,TYPE_DOUBLE,&stn_dh,NULL},
    { NULL }
};

static column_def *valid_columns = 0;
static int stn_data = 0;

static void *table_columns = NULL;
static int table_header_rows = 0;

static column_def *get_column_def( std::string_view name )
{
    column_def *c;
    for( c = valid_columns; c && c->name; c++ )
    {
        if( boost::algorithm::iequals(c->name,name) ) return c;
    }
    return NULL;
}

static column_def *get_class_column_def( std::string_view cls )
{
    if( nclass >= MAXCLASS )
    {
        return 0;
    }
    int id;
    std::string name(cls);
    if( stn_data )
    {
        id = net->class_id( name, 0 );
        if( id ) name = net->class_name(id);
    }
    else
    {
        id = obs_classes.id( name, 0 );
        if( id ) name = obs_classes.name( id );
    }
    classid[nclass] = id;
    classname[nclass] = std::move(name);
    classvalue[nclass] = blankvalue;
    classcol.name = classname[nclass].c_str();
    classcol.data = &classvalue[nclass];
    nclass++;
    return &classcol;
}

static void delete_column_def( void *pcd )
{
    int i;
    column_def *cd = (column_def *) pcd;
    if( cd->type == TYPE_ANGLE && cd->format )
    {
        check_free( cd->format );
    }
    if( cd->prefix ) check_free( cd->prefix );
    if( cd->suffix ) check_free( cd->suffix );
    for( i = 0; i < MAX_HEADERS; i++ )
    {
        if( cd->header[i] ) {check_free( cd->header[i] ); cd->header[i] = NULL; }
    }
}

static void init_table( void )
{
    if( !table_columns ) table_columns = create_list( sizeof( column_def ));
    clear_list( table_columns, delete_column_def );
    table_header_rows = 0;
    valid_columns = 0;
    nclass = 0;
}


static void print_field( column_def *cd, const char *s, int quotefield, FILE *out )
{
    int left, right, spare;
    if( cd->extralen < 0 )
    {
        cd->extralen = 0;
        if( quotefield == QT_QUOTE) cd->extralen += 2 * strlen( quote );
        if( cd->prefix ) cd->extralen += strlen( cd->prefix );
        if( cd->suffix ) cd->extralen += strlen( cd->suffix );
    }
    spare = cd->width - strlen(s) - cd->extralen;
    left = right = 0;

    if( spare > 0 ) switch( cd->just )
        {
        case JST_CENTRE:  right = spare/2; left = spare - right; break;
        case JST_RIGHT:   left = spare; break;
        default:          right = spare; break;
        }
    while( left-- ) fputc( ' ', out );
    if( cd->prefix ) fputs( cd->prefix, out );
    if( quotefield == QT_LITERAL )
    {
        fputs(s,out);
    }
    else if( quotefield == QT_QUOTE && canquote )
    {
        fputs( quote, out );
        for( const char *sc = s; *sc; sc++ )
        {
            if( *sc == *quote ) fputs( qquote, out );
            else if( *sc == *escape ) fputs( qescape, out  );
            else fputc(*sc, out );
        }
        fputs( quote, out );
    }
    else
    {
        for( const char *sc = s; *sc; sc++ )
        {
            if( *sc == *quote ) fputs( nqquote, out );
            else if( *sc == *escape ) fputs( nqescape, out  );
            else if( *sc == *delim ) fputs( nqdelim, out );
            else if( *sc == '\n' ) fputs( nqnewline, out );
            else fputc(*sc, out );
        }
    }
    if( cd->suffix ) fputs( cd->suffix, out );
    while( right-- ) fputc( ' ', out );
}
void print_table_row( FILE *out )
{
    column_def *cd;
    int first;
    if( !table_columns || !out ) return;
    first = 1;
    for( reset_list_pointer( table_columns );
            NULL != (cd = (column_def *) next_list_item( table_columns ) ); )
    {

        char buf[80];
        char *s = NULL;
        switch( cd->type )
        {
        case TYPE_STRING: s = (char *) cd->data; break;
        case TYPE_PSTRING: s = * (char **) cd->data; break;
        case TYPE_DOUBLE:  sprintf(buf,"%.*lf",cd->ndp, *(double *)cd->data);
            s = buf;
            break;
        case TYPE_ANGLE:   if( cd->format )
            {
                double a = * (double *) cd->data;
                a *= RTOD;
                while( a > 360 ) a -= 360;
                while( a < 0 ) a += 360;
                dms_string( a, cd->format, buf );
                s = buf;
            }
            break;
        }
        if( !s ) { buf[0] = 0; s = buf; }
        if( first ) first = 0; else fputs( delim, out );
        print_field( cd, s, cd->quote, out );
    }
    fputc('\n',out);
}

void list_vecdata_residuals( FILE *out, survdata  *v )
{
    vecdata *t;
    station *from, *to;
    int axis, iobs;
    double brngdiff, brngppm, brngrf;
    double obs_dist, calc_dist, distdiff, distppm, distrf;
    double vecdiff, vecppm, vecrf;

    for(iobs = 0; iobs < v->nobs; iobs++ )
    {
        double d1xyz[3];
        double d2xyz[3];

        /* NOTE **** I have ignored instrument heights in this - it shouldn't
           make any difference to projection bearing or sea level distance */

        t = &v->obs.vdata[iobs];
        if( t->tgt.type != GB ) continue;
        for( axis = 0; axis < 3; axis++ ) { d1xyz[axis] = t->vector[axis]; }
        if( v->reffrm ) rftrans_correct_vector( v->reffrm, d1xyz, v->date );

        from = station_ptr( net, v->from );
        to = station_ptr( net, t->tgt.to );
        fromStn = from->Code;
        toStn = to->Code;
        fromStnName = from->Name.c_str();
        toStnName = to->Name.c_str();

        for( axis = 0; axis < 3; axis++ )
        {
            double x;
            x = (from->XYZ[axis] + to->XYZ[axis])/2.0;
            d2xyz[axis] = x + d1xyz[axis]/2.0;
            d1xyz[axis] = x - d1xyz[axis]/2.0;
        }

        dummy1 = *from;
        dummy2 = *to;
        modify_station_xyz( &dummy1, d1xyz, el );
        modify_station_xyz( &dummy2, d2xyz, el );

        convert_coords( &from_xyz, d1xyz, NULL, d1xyz, NULL );
        convert_coords( &from_xyz, d2xyz, NULL, d2xyz, NULL );

        obs_prj_brng = atan2( d2xyz[CRD_EAST]-d1xyz[CRD_EAST], d2xyz[CRD_NORTH]-d1xyz[CRD_NORTH] );
        /* obs_bearing = RTOD * atan2( d2xyz[CRD_EAST]-d1xyz[CRD_EAST], d2xyz[CRD_NORTH]-d1xyz[CRD_NORTH] );
        while( obs_bearing > 360.0 ) obs_bearing -= 360.0;
        while( obs_bearing < 0.0 ) obs_bearing += 360.0;
        */

        convert_coords( &from_xyz, from->XYZ, NULL, d1xyz, NULL );
        convert_coords( &from_xyz, to->XYZ, NULL, d2xyz, NULL );

        calc_prj_brng = atan2( d2xyz[CRD_EAST]-d1xyz[CRD_EAST], d2xyz[CRD_NORTH]-d1xyz[CRD_NORTH] );
        /*
        calc_bearing = RTOD * atan2( d2xyz[CRD_EAST]-d1xyz[CRD_EAST], d2xyz[CRD_NORTH]-d1xyz[CRD_NORTH] );
        while( calc_bearing > 360.0 ) calc_bearing -= 360.0;
        while( calc_bearing < 0.0 ) calc_bearing += 360.0;
        */

        brngdiff = (calc_prj_brng - obs_prj_brng) * RTOD;
        if( brngdiff > 180.0 ) brngdiff -= 360.0;
        if( brngdiff < -180.0 ) brngdiff += 360.0;
        brngdiff *= 3600.0;
        prj_brng_err = brngdiff;

        brngppm = fabs(brngdiff * STOR);
        if( brngppm < 1.0e-6 ) brngrf = 999999.0; else brngrf = 1.0/brngppm;
        brngppm *= 1.0e6;
        ppm_prj_brng_err = brngppm;
        rf_prj_brng_err = brngrf;

        obs_dist =  calc_distance( &dummy1, -dummy1.OHgt, &dummy2, -dummy2.OHgt, NULL,
                                   NULL );

        calc_dist =  calc_distance( from, -from->OHgt, to, -to->OHgt, NULL,
                                    NULL );

        obs_dist *= ellipsoidal_distance_correction(from,to);
        calc_dist *= ellipsoidal_distance_correction(from,to);

        distdiff = calc_dist - obs_dist;

        distppm = 0.0;
        if( obs_dist > 1.0e-6) distppm = fabs(distdiff / obs_dist);
        if( distppm < 1.0e-6 ) distrf = 999999.0; else distrf = 1.0/distppm;
        distppm *= 1.0e6;
        obs_ell_dist = obs_dist;
        calc_ell_dist = calc_dist;
        ell_dist_err = distdiff;
        ppm_ell_dist_err = distppm;
        rf_ell_dist_err = distrf;

        vecdiff = 2.0 * calc_distance( &dummy1, -dummy1.OHgt, from, -from->OHgt,
                                       NULL, NULL);
        vecppm = 0.0;
        if( obs_dist > 1.0e-6 ) vecppm = fabs(vecdiff/obs_dist);
        if( vecppm < 1.0e-6 ) vecrf = 999999.0; else vecrf = 1.0/vecppm;
        vecppm *= 1.0e6;
        hor_vec_err = vecdiff;
        ppm_hor_vec_err = vecppm;
        rf_hor_vec_err = vecrf;

        for( int i = 0; i < nclass; i++ )
        {
            int idclass = classid[i];
            classvalue[i] = blankvalue;
            if( idclass > 0 )
            {
                auto name = get_obs_classification_name( v,  &(t->tgt), idclass );
                if( name )
                {
                    classvaluestore[i] = std::move(*name);
                    classvalue[i] = classvaluestore[i].c_str();
                }
            }
        }

        print_table_row( out );
    }
}

static int list_observations( FILE *out, BINARY_FILE *bf )
{
    bindata *b;
    survdata *sd;

    if( find_section( bf, "OBSERVATIONS" ) != OK ) return MISSING_DATA;

    init_bindata( bf->f );

    b = create_bindata();
    init_get_bindata( 0L );

    while( get_bindata( SURVDATA, b ) == OK )
    {
        sd = (survdata *) b->data;
        if( sd->format != SD_VECDATA ) continue;
        list_vecdata_residuals( out, sd );
    }

    delete_bindata(b);
    return OK;
}

static int list_stations( FILE *out )
{
    int nstns;
    int istn;
    station *st;
    stn_adjustment *sa;
    double enh[3];
    projection *prj;

    unsigned char projection_coords;

    projection_coords = is_projection( net->crdsys ) ? 1 : 0;
    prj = net->crdsys->prj;

    nstns = number_of_stations( net );
    for( istn = 0; istn++ < nstns; )
    {
        st = stnptr(istn);
        sa = stnadj(st);

        convert_coords( &from_xyz, st->XYZ, NULL, enh, NULL );
        stn_code = st->Code;
        stn_name = st->Name.c_str();
        stn_order_store = net->order( net->station_order( st ) );
        stn_order = stn_order_store.empty() ? "-" : stn_order_store.c_str();

        if( projection_coords )
        {
            geog_to_proj( prj, st->ELon, st->ELat, &stn_easting, &stn_northing );
        }
        else
        {
            stn_northing =  st->ELon*RTOD;
            stn_easting = st->ELat*RTOD;
        }
        stn_height = enh[CRD_HGT];
        if( covar )
        {
            stn_h_max_error = covar[istn].emax;
            stn_h_min_error = covar[istn].emin;
            stn_h_max_brng = RTOD * covar[istn].az;
        }
        else
        {
            stn_h_max_error = 0;
            stn_h_min_error = 0;
            stn_h_max_brng = 0;
        }
        stn_dn = ( st->ELat - sa->initELat ) * st->dNdLt;
        stn_de = ( st->ELon - sa->initELon ) * st->dEdLn;
        stn_dh = st->OHgt - sa->initOHgt;

        for( int i = 0; i < nclass; i++ )
        {
            int idclass = classid[i];
            classvalue[i] = blankvalue;
            if( idclass > 0 )
            {
                int idvalue = get_station_class( st, idclass );
                if(idvalue > 0 )
                {
                    classvaluestore[i] = net->class_value( idclass, idvalue );
                    classvalue[i] = classvaluestore[i].c_str();
                }
            }
        }
        print_table_row( out );
    }
    return OK;
}

static void print_table_header( FILE *out )
{
    column_def *cd;
    const char *blank = "";
    int row;
    int first;
    for( row = 0; row < table_header_rows; row++ )
    {
        first = 1;
        for( reset_list_pointer( table_columns );
                NULL != ( cd = (column_def *) next_list_item( table_columns ));
           )
        {
            const char *s;
            s = cd->header[row];
            if( !s ) s = blank;
            if( first ) first = 0; else fputs( delim, out );
            print_field( cd, s, QT_QUOTE, out );
        }
        fputc('\n',out);
    }
}

static void print_table( void )
{


    if( ! stn_data && !is_projection( net->crdsys ) )
    {
        printf( "Cannot print snaplist data for coordinate systems without projections\n");
        return;
    }

    // Sort out delimiters
    if( *delim == 0 ) strcpy(delim,",");
    if( *quote == *delim ) *quote = 0;
    if( *escape == *delim ) *escape = 0;
    canquote = *quote ? 1 : 0;

    const char *replace = *delim == ' ' ? "_" : " ";

    if( *escape  && *escape != *quote )
    {
        strcpy(nqescape,escape);
        strcat(nqescape,escape);
        strcpy(nqdelim,escape);
        strcat(nqdelim,delim);
        strcpy(nqnewline,escape);
        strcat(nqnewline,"\n");
        strcpy(nqquote,escape);
        strcat(nqquote,quote);
    }
    else
    {
        strcat(nqescape,replace);
        strcat(nqdelim,replace);
        strcat(nqnewline,replace);
        strcat(nqquote,replace);
    }

    if( canquote )
    {
        if( *escape )
        {
            strcpy(qquote,escape);
            strcat(qquote,quote);
            strcpy(qescape,escape);
            strcat(qescape,escape);
        }
        else
        {
            strcpy(qquote,replace);
        }
    }

    printf("\nPrinting table...\n");
    print_table_header( out );

    if( valid_columns == obs_valid_columns )
    {
        list_observations( out, b );
    }
    else if ( valid_columns == stn_valid_columns )
    {
        list_stations( out );
    }
}

/*======================================================================*/

static char deg[30] = {' ', 0 };
static char min[30] = {' ', 0 };
static char sec[30] = {0};

static int read_angle_format( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_text( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_table( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_data( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_delimiter( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_column( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );

static config_item main_commands[] =
{
    {"angle_format",NULL,CFG_ABSOLUTE,0,read_angle_format,0,0},
    {"text",NULL,CFG_ABSOLUTE,0,read_text,0,0},
    {"table",NULL,CFG_ABSOLUTE,0,read_table,0,0},
    {NULL}
};

enum { CHAR_DELIM, CHAR_QUOTE, CHAR_ESCAPE };

static config_item table_commands[] =
{
    {"data",NULL,CFG_ABSOLUTE,0,read_data,CFG_ONEONLY | CFG_REQUIRED,0},
    {"delimiter",NULL,CFG_ABSOLUTE,0,read_delimiter,CFG_ONEONLY | CFG_REQUIRED,CHAR_DELIM},
    {"quote",NULL,CFG_ABSOLUTE,0,read_delimiter,CFG_ONEONLY,CHAR_QUOTE},
    {"escape",NULL,CFG_ABSOLUTE,0,read_delimiter,CFG_ONEONLY,CHAR_ESCAPE},
    {"column",NULL,CFG_ABSOLUTE,0,read_column,CFG_REQUIRED,0},
    {"angle_format",NULL,CFG_ABSOLUTE,0,read_angle_format,0,0},
    {"end_table",NULL,CFG_ABSOLUTE,0,STORE_AS_STRING,CFG_END,0},
    {NULL}
};

namespace {
/// Decodes the 2-hex-digit escape byte starting right after source[xi]
/// (the 'x'/'X' character itself), if both following characters are
/// present and are hex digits.
std::optional<char> parseHexEscapeByte( std::string_view source, std::size_t xi )
{
    if( xi+2 >= source.size() || ! ISXDIGIT(source[xi+1]) || ! ISXDIGIT(source[xi+2]) ) return std::nullopt;
    unsigned char hx = ISDIGIT(source[xi+1]) ? (source[xi+1] - '0') : (10 + TOUPPER(source[xi+1]) - 'A');
    hx = hx*16 + ( ISDIGIT(source[xi+2]) ? (source[xi+2] - '0') : (10 + TOUPPER(source[xi+2]) - 'A') );
    return static_cast<char>(hx);
}
}

/// Decodes one escape unit from source starting at position i - a single
/// plain/underscore character, a two-character \\b/\\t/\\n/\\X-with-no-valid-
/// hex-pair escape, or a four-character \\xNN hex escape. Sets target to the
/// character to emit, or leaves it nullopt if this step produces no output
/// at all (a lone trailing '\\' with nothing following it).
/// \return the number of source characters consumed (always at least 1).
static std::size_t interpret_escaped_string( std::string_view source, std::size_t i, std::optional<char> &target )
{
    target = std::nullopt;
    const char c = source[i];
    if( c == '\\' )
    {
        if( i+1 >= source.size() ) return 1;  // dangling backslash - no output
        const char next = source[i+1];
        switch( next )
        {
        case 'B': case 'b': target = ' '; return 2;
        case 'T': case 't': target = '\t'; return 2;
        case 'N': case 'n': target = '\n'; return 2;
        case 'X': case 'x':
            if( auto hx = parseHexEscapeByte(source,i+1) )
            {
                target = *hx;
                return 4;  // '\', 'x', and 2 hex digits
            }
            target = next;
            return 2;
        default: target = next; return 2;
        }
    }
    if( c == '_' )
    {
        target = ' ';
        return 1;
    }
    target = c;
    return 1;
}

/// Runs interpret_escaped_string() over the whole of source, writing the
/// decoded result into buf, truncating without error if it doesn't fit -
/// the caller-facing entry point for the 7 not-yet-converted fixed-buffer
/// destinations (deg/min/sec, quote/delim/escape, header/prefix/suffix).
static char *fill_escaped_buffer( std::string_view source, char *buf, int maxbuf )
{
    char *t = buf;
    int nch = maxbuf;
    if( nch < 1 || !buf ) {return buf; }
    for( std::size_t i=0; i<source.size(); )
    {
        std::optional<char> ch;
        i += interpret_escaped_string( source, i, ch );
        if( ! ch ) continue;
        *t++ = *ch;
        nch--;
        if( !nch ) break;
    }
    *t = 0;
    return buf;
}

// #pragma warning(disable: 4100)

static int read_angle_format( CFG_FILE *, std::string_view string, void *, int, int )
{
    // The format's own field separator is whatever character comes first
    // after leading whitespace (e.g. "." or ":") - matches the original's
    // *string after the same skip. strtok(string,angle_delim) then skips
    // that leading occurrence of the separator itself before returning the
    // first real field, so the scanner below starts one character later.
    const auto firstNonSpace = std::find_if( string.begin(), string.end(),
        []( const char c ){ return ! ISSPACE(c); } );
    if( firstNonSpace == string.end() ) return MISSING_DATA;
    const char delim = *firstNonSpace;
    const std::size_t start = std::distance( string.begin(), firstNonSpace );
    FieldScanner scanner( string.substr(start+1) );

    // Matches strtok's own semantics for a mid-string field - when no
    // further delimiter is found, the rest of the string becomes the final
    // field rather than the split failing (unlike FieldScanner::next(char),
    // which fails without consuming in that case).
    auto degField = scanner.next(delim);
    if( ! degField ) return MISSING_DATA;
    std::string_view smin;
    std::string_view ssec;
    bool haveSec = false;
    if( auto minField = scanner.next(delim) )
    {
        smin = *minField;
        std::string_view rest = scanner.remainder();
        if( ! rest.empty() ) { ssec = rest; haveSec = true; }
    }
    else
    {
        smin = scanner.remainder();
        if( smin.empty() ) return MISSING_DATA;
    }

    fill_escaped_buffer( *degField, deg, 30 );
    fill_escaped_buffer( smin, min, 30 );
    if( haveSec )
    {
        fill_escaped_buffer( ssec, sec, 30 );
    }
    else
    {
        sec[0] = 0;
    }
    return OK;
}

// #pragma warning(disable: 4100)

static int read_text( CFG_FILE *cfg, std::string_view, void *, int, int )
{
    bool finished = false;
    const int read_opts = set_config_read_options( cfg, CFG_IGNORE_COMMENT );
    ConfigLine line;
    while( !finished && cfg->get_config_line( line, CFG_MAX_LINE_LENGTH ) )
    {
        if( line.overrun )
        {
            send_config_error( cfg, INVALID_DATA,
                           "Text line too long in config file");
            finished=true;
            break;
        }
        if( boost::algorithm::istarts_with( line.content, "end_text" ) )
        {
            finished = true;
        }
        else
        {
            if( out ) fprintf(out,"%s\n",line.content.c_str());
        }
    }
    if( !finished )
    {
        send_config_error( cfg, INVALID_DATA,
                           "Text definition not terminated in config file");
    }
    set_config_read_options( cfg, read_opts );
    return OK;
}

// #pragma warning(disable: 4100)

static int read_table( CFG_FILE *cfg, std::string_view, void *, int, int )
{
    int read_opts;
    int sts;
    init_table();
    strcpy(quote,"\"");
    strcpy(delim,",");
    strcpy(escape,"\"");
    read_opts = set_config_read_options( cfg, CFG_INIT_ITEMS | CFG_CHECK_MISSING | CFG_POSITIONAL_COMMENT );
    sts = read_config_file( cfg, table_commands );
    if( sts == OK ) print_table();
    set_config_read_options( cfg, read_opts );
    return OK;
}

// #pragma warning(disable: 4100)

static int read_data( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    column_def *coltype = 0;
    FieldScanner scanner(string);
    auto s = scanner.next();
    if( s && boost::algorithm::iequals( *s, "stations" ) )
    {
        coltype = stn_valid_columns;
        stn_data = 1;
    }
    else if( s && boost::algorithm::iequals( *s, "gps" ) )
    {
        coltype = obs_valid_columns;
        stn_data = 0;
    }
    else
    {
        send_config_error( cfg, INVALID_DATA, "Program only handles GPS and station data currently" );
    }
    if( valid_columns == 0 ) valid_columns = coltype;
    if( valid_columns != coltype )
    {
        send_config_error( cfg, INVALID_DATA, "Inconsistent data types defined" );
    }
    return OK;
}

// #pragma warning(disable: 4100)

static int read_delimiter( CFG_FILE *, std::string_view string, void *, int, int code )
{
    char *t;
    switch( code )
    {
    case CHAR_QUOTE: t=quote; break;
    case CHAR_DELIM: t=delim; break;
    case CHAR_ESCAPE: t=escape; break;
    default:
        return INVALID_DATA;
    }
    FieldScanner scanner(string);
    auto s = scanner.next();
    if( !s ) return MISSING_DATA;
    if( boost::algorithm::iequals(*s,"tab") )
    {
        strcpy(t,"\t");
        return OK;
    }
    if( boost::algorithm::iequals(*s,"comma") )
    {
        strcpy(t,",");
        return OK;
    }
    if( boost::algorithm::iequals(*s,"blank") )
    {
        strcpy(t," ");
        return OK;
    }
    if( boost::algorithm::iequals(*s,"none") )
    {
        strcpy(t,"");
        return OK;
    }
    fill_escaped_buffer( *s, t, MAX_DELIM );
    return OK;
}


// #pragma warning(disable: 4100)

#define MAX_PREFIX 30

static int read_column( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    column_def *cd, *tblcol;
    int width = -1;
    int just = -1;
    int ndp = -1;
    int quote = QT_NONE;
    static char header[1024];
    char *h;
    int nh;
    int i;
    char prefix[MAX_PREFIX];
    char suffix[MAX_PREFIX];

    prefix[0] = 0;
    suffix[0] = 0;
    FieldScanner scanner(string);
    auto data = scanner.next();
    if( !data ) return MISSING_DATA;
    cd = 0;
    if( boost::algorithm::istarts_with(*data,"class="))
    {
        cd = get_class_column_def( data->substr(6) );
    }
    else
    {
        cd = get_column_def( *data );
    }
    if( !cd )
    {
        char errmess[80];
        sprintf(errmess,"Invalid column name %.20s specified",std::string(*data).c_str() );
        send_config_error( cfg, INVALID_DATA, errmess );
        return OK;
    }

    header[0] = 0;

    for( auto opt = scanner.next(); opt; opt = scanner.next() )
    {
        if( boost::algorithm::iequals(*opt,"quote") ) { quote = QT_QUOTE; continue; }
        if( boost::algorithm::iequals(*opt,"literal") ) { quote = QT_LITERAL; continue; }
        const auto eqPos = opt->find('=');
        if( eqPos == std::string_view::npos || eqPos == opt->size()-1 )
        {
            char errmess[80];
            sprintf(errmess,"Missing value for option %.20s in column command",
                    std::string(*opt).c_str());
            send_config_error( cfg, MISSING_DATA, errmess );
            return OK;
        }
        const std::string_view key = opt->substr(0,eqPos);
        const std::string_view val = opt->substr(eqPos+1);
        if( boost::algorithm::iequals(key,"width") )
        {
            auto w = parse_leading<int>(val);
            if( w && *w >= 0 ) { width = *w; continue; }
        }
        else if( boost::algorithm::iequals(key,"align") )
        {
            switch( val[0] )
            {
            case 'l': case 'L': just = JST_LEFT; continue;
            case 'c': case 'C': just = JST_CENTRE; continue;
            case 'r': case 'R': just = JST_RIGHT; continue;
            }
        }
        else if( boost::algorithm::iequals(key,"ndp") )
        {
            auto d = parse_leading<int>(val);
            if( d && *d >= 0 ) { ndp = *d; continue; }
        }
        else if( boost::algorithm::iequals(key,"header") )
        {
            fill_escaped_buffer( val, header, 1024 );
            continue;
        }
        else if( boost::algorithm::iequals(key,"prefix") )
        {
            fill_escaped_buffer( val, prefix, MAX_PREFIX );
            continue;
        }
        else if( boost::algorithm::iequals(key,"suffix") )
        {
            fill_escaped_buffer( val, suffix, MAX_PREFIX );
            continue;
        }
        else
        {
            char errmess[80];
            sprintf(errmess,"Invalid option %.20s in column command",std::string(key).c_str());
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }
        {
            char errmess[80];
            sprintf(errmess,"Invalid value %.20s for option %.20s",std::string(val).c_str(),std::string(key).c_str());
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }

    }

    tblcol = (column_def *) add_to_list( table_columns, NEW_ITEM );
    memcpy( tblcol, cd, sizeof(column_def) );
    if( width >= 0 ) tblcol->width = width;
    if( just > 0 ) tblcol->just = just;
    if( ndp >= 0 ) tblcol->ndp = ndp;
    tblcol->quote = quote;
    if( tblcol->type == TYPE_ANGLE )
    {
        tblcol->format = create_dms_format( 0, ndp, 0, deg, min, sec, NULL, NULL );
    }
    h = header;
    if ( !*h ) h = NULL;
    nh = 0;
    for( i = 0; i < MAX_HEADERS; i++ )
    {
        tblcol->header[i] = NULL;
        if( h )
        {
            char *end, more;
            for( end = h; *end && *end != '\n'; end++ ) {}
            more = *end;
            *end = 0;
            tblcol->header[i] = copy_string( h );
            h = more ? end+1 : NULL;
            nh++;
        }
    }
    if( nh > table_header_rows ) table_header_rows = nh;
    tblcol->prefix = prefix[0] ? copy_string( prefix ) : NULL;
    tblcol->suffix = suffix[0] ? copy_string( suffix ) : NULL;
    tblcol->extralen = -1;

    return OK;
}


void calc_error_ellipse( double cvr[], double *emax, double *emin, double *azemax )
{
    double v1, v2, v3, v4;

    v1 = (cvr[2]+cvr[0])/2.0;
    v2 = (cvr[2]-cvr[0])/2.0;
    v3 = cvr[1];
    v4 = v2*v2+v3*v3;
    if( v4 > 0.0 ) v4 = sqrt(v4);
    *azemax = v4 > 0.0 ? atan2( v3, v2 ) / 2.0 : 0.0 ;
    v2 = v1-v4;
    v1 = v1+v4;
    *emax = v1 > 0.0 ? sqrt(v1) : 0.0;
    *emin = v2 > 0.0 ? sqrt(v2) : 0.0;
}


int reload_covariances( BINARY_FILE *b )
{
    int istn;
    double cvr[6];
    int nstns;

    if( find_section( b, "STATION_COVARIANCES" ) != OK ) return MISSING_DATA;

    nstns = number_of_stations( net );
    covar = (covariance *) check_malloc( sizeof(covariance) * (nstns+1) );

    for( istn = 0; istn++ < nstns; )
    {
        fread( cvr, sizeof(cvr), 1, b->f );
        calc_error_ellipse( cvr, &covar[istn].emax, &covar[istn].emin,
                            &covar[istn].az );
        covar[istn].sehgt = cvr[5] > 0.0 ? sqrt(cvr[5]) : 0.0;
    }

    return check_end_section( b );
}



/*======================================================================*/



static const char *default_cfg_name = "snaplist";

int main( int argc, char *argv[] )
{
    coordsys *xyzcs;
    double lat, lon;
    CFG_FILE *cfg = 0;
    std::optional<std::string> cfn;
    const char *basecfn, *ofn;

    CONFIGURE_RUNTIME();

    printf( "snaplist:  Tabulates observations and stations in a SNAP binary file\n");

    if( argc != 3 && argc != 4 )
    {
        printf("Syntax: snaplist binary_file_name [config_file_name] listing_file_name\n");
        return 0;
    }

    init_snap_globals();
    install_default_projections();
    install_default_crdsys_file( );

    const std::string bfn = argv[1];
    b = open_binary_file( bfn, BINFILE_SIGNATURE ).file;

    if( !b ||
            reload_snap_globals( b ) != OK ||
            reload_stations( b ) != OK ||
            reload_filenames( b ) != OK  ||
            reload_rftransformations( b ) != OK )
    {

        printf( "Cannot reload data from binary file %s\n", bfn.c_str());
        return 0;
    }

    /* Only reload covariances if available */

    reload_covariances( b );


    /* Set up the bits we need to calculate ellipsoidal distances
       and projection bearings */

    xyzcs = related_coordsys( net->crdsys, CSTP_CARTESIAN );
    define_coord_conversion( &to_xyz, net->crdsys, xyzcs );
    define_coord_conversion( &from_xyz, xyzcs, net->crdsys );
    el = net->crdsys->rf->el;

    get_network_topocentre( net, &lat, &lon );
    init_station( &dummy1, "0", "0", lat, lon, 0.0, 0.0, 0.0, 0.0, el );
    init_station( &dummy2, "0", "0", lat, lon, 0.0, 0.0, 0.0, 0.0, el );

    reload_obs_classes( b );

    if( argc == 3 )
    {
        basecfn = default_cfg_name;
        ofn = argv[2];
    }
    else
    {
        basecfn = argv[2];
        ofn = argv[3];
    }

    cfn = find_file( basecfn, ".tbf", std::optional<std::string>(bfn), FF_TRYALL, "snaplist" );
    if( cfn ) { cfg = open_config_file( *cfn, '!' );}
    if( !cfn || !cfg )
    {
        printf("Cannot open configuration file %s\n",basecfn);
        return 0;
    }

    out = fopen( ofn, "w" );
    if( !out )
    {
        printf("Cannot open output file %s\n",ofn );
        return 0;
    }

    printf("\nUsing configuration file %s\n",cfn->c_str());
    read_config_file( cfg, main_commands );
    close_config_file( cfg );

    fclose(out);
    return 0;
}
