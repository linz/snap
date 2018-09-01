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

typedef struct
{
    double emax, emin, az;
    double sehgt;
} covariance;

static covariance *covar;

enum {JST_LEFT, JST_CENTRE, JST_RIGHT };
enum {QT_NONE, QT_QUOTE, QT_LITERAL };
enum {TYPE_STRING, TYPE_PSTRING, TYPE_DOUBLE, TYPE_ANGLE };

#define MAX_HEADERS 3

typedef struct
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
} column_def;

static char *fromStn;
static char *toStn;
static char *fromStnName;
static char *toStnName;
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
static char *classname[MAXCLASS];
static const char *classvalue[MAXCLASS];
static const char *blankvalue = "";
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

static column_def *get_column_def( char *name )
{
    column_def *c;
    for( c = valid_columns; c && c->name; c++ )
    {
        if( _stricmp(c->name,name) == 0 ) return c;
    }
    return NULL;
}

static column_def *get_class_column_def( char *cls )
{
    if( nclass >= MAXCLASS )
    {
        return 0;
    }
    int id;
    const char *name = cls;
    if( stn_data )
    {
        id = network_class_id( net, cls, 0 );
        if( id ) name = network_class_name(net,id);
    }
    else
    {
        id = classification_id( &obs_classes, cls, 0 );
        if( id ) name = classification_name( &obs_classes, id );
    }
    classid[nclass] = id;
    classname[nclass] = copy_string(name);
    classvalue[nclass] = blankvalue;
    classcol.name = classname[nclass];
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
    if( nclass > 0 )
    {
        for( int i = 0; i < nclass; i++ )
        {
            check_free( classname[i] );
            classname[i] = 0;
        }
    }
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
        fromStnName = from->Name;
        toStnName = to->Name;

        for( axis = 0; axis < 3; axis++ )
        {
            double x;
            x = (from->XYZ[axis] + to->XYZ[axis])/2.0;
            d2xyz[axis] = x + d1xyz[axis]/2.0;
            d1xyz[axis] = x - d1xyz[axis]/2.0;
        }

        memcpy( &dummy1, from, sizeof(station) );
        memcpy( &dummy2, to, sizeof(station) );
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
                classvalue[i] = get_obs_classification_name( v,  &(t->tgt), idclass );
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

    while( get_bindata( b ) == OK )
    {
        if( b->bintype != SURVDATA ) continue;
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
        stn_name = st->Name;
        stn_order = network_order( net, network_station_order( net, st ) );
        if( stn_order == NULL ) stn_order = "-";

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
                if(idvalue > 0 ) classvalue[i] = network_class_value( net, idclass, idvalue );
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

static const char *whitespace = " \r\t\n";

static int read_angle_format( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_text( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_table( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_data( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_delimiter( CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_column( CFG_FILE *cfg, char *string, void *value, int len, int code );

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

static char *interpret_escaped_string( char *source, char *target, int maxtgt )
{
    char *s = source;
    char *t = target;
    int nch = maxtgt;
    char escape;
    if( nch < 1 || !target ) {return target; }
    escape = 0;
    for( ; *s; s++ )
    {
        if( !escape && *s == '\\' )
        {
            escape = 1;
            continue;
        }
        else if( escape )
        {
            escape = 0;
            switch( *s )
            {
            case 'B': case 'b': *t = ' '; break;
            case 'T': case 't': *t = '\t'; break;
            case 'N': case 'n': *t = '\n'; break;
            case 'X': case 'x':
                if( ISXDIGIT(s[1]) && ISXDIGIT(s[2]))
                {
                    unsigned char c;
                    c = ISDIGIT(s[1]) ? (s[1] - '0') : (10 + TOUPPER(s[1]) - 'A');
                    c *= 16;
                    c += ISDIGIT(s[2]) ? (s[2] - '0') : (10 + TOUPPER(s[2]) - 'A');
                    s += 2;
                    *t = c;
                }
                else
                {
                    *t = *s;
                }
                break;

            default: *t = *s; break;
            }
        }
        else if( *s == '_' )
        {
            *t = ' ';
        }
        else
        {
            *t = *s;
        }
        t++; nch--;
        if( !nch ) break;
    }
    *t = 0;
    return target;
}

// #pragma warning(disable: 4100)

static int read_angle_format( CFG_FILE *, char *string, void *, int, int )
{
    char *sdeg, *smin, *ssec;
    char angle_delim[2];
    while( *string && ISSPACE(*string) ) string++;
    angle_delim[0] = *string;
    angle_delim[1] = 0;

    sdeg = strtok( string, angle_delim );
    smin = strtok( NULL, angle_delim );
    if( !smin ) return MISSING_DATA;
    interpret_escaped_string( sdeg, deg, 30 );
    interpret_escaped_string( smin, min, 30 );
    ssec = strtok( NULL, angle_delim );
    if( ssec )
    {
        interpret_escaped_string( ssec, sec, 30 );
    }
    else
    {
        sec[0] = 0;
    }
    return OK;
}

// #pragma warning(disable: 4100)

static int read_text( CFG_FILE *cfg, char *, void *, int, int )
{
    int finished, read_opts, overrun;
    static char buf[1024];
    read_opts = set_config_read_options( cfg, CFG_IGNORE_COMMENT );
    finished = 0;
    while( !finished  && get_config_line( cfg, buf, 1024, &overrun ) )
    {
        if( overrun )
        {
            send_config_error( cfg, INVALID_DATA,
                           "Text line too long in config file");
            finished=1;
            break;
        }
        if( _strnicmp( buf, "end_text", 8 ) == 0 )
        {
            finished = 1;
        }
        else
        {
            if( out ) fprintf(out,"%s\n",buf);
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

static int read_table( CFG_FILE *cfg, char *, void *, int, int )
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

static int read_data( CFG_FILE *cfg, char *string, void *, int, int )
{
    char *s;
    column_def *coltype = 0;
    s = strtok(string,whitespace);
    if( _stricmp( s, "stations" ) == 0 )
    {
        coltype = stn_valid_columns;
        stn_data = 1;
    }
    else if( _stricmp( s, "gps" ) == 0 )
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

static int read_delimiter( CFG_FILE *, char *string, void *, int, int code )
{
    char *s, *t;
    switch( code )
    {
    case CHAR_QUOTE: t=quote; break;
    case CHAR_DELIM: t=delim; break;
    case CHAR_ESCAPE: t=escape; break;
    default:
        return INVALID_DATA;
    }
    s = strtok( string, whitespace );
    if( !s ) return MISSING_DATA;
    if( _stricmp(s,"tab") == 0 )
    {
        strcpy(t,"\t");
        return OK;
    }
    if( _stricmp(s,"comma") == 0)
    {
        strcpy(t,",");
        return OK;
    }
    if( _stricmp(s,"blank") == 0 )
    {
        strcpy(t," ");
        return OK;
    }
    if( _stricmp(s,"none") == 0 )
    {
        strcpy(t,"");
        return OK;
    }
    interpret_escaped_string( s, t, MAX_DELIM );
    return OK;
}


// #pragma warning(disable: 4100)

#define MAX_PREFIX 30

static int read_column( CFG_FILE *cfg, char *string, void *, int, int )
{
    char *data;
    char *opt;
    char *val;
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
    data = strtok(string,whitespace);
    if( !data ) return MISSING_DATA;
    cd = 0;
    if( _strnicmp(data,"class=",6) == 0)
    {
        cd = get_class_column_def( data+6 );
    }
    else
    {
        cd = get_column_def( data );
    }
    if( !cd )
    {
        char errmess[80];
        sprintf(errmess,"Invalid column name %.20s specified",data );
        send_config_error( cfg, INVALID_DATA, errmess );
        return OK;
    }

    header[0] = 0;

    while( NULL != (opt = strtok(NULL, whitespace) ) )
    {
        if( _stricmp(opt,"quote") == 0 ) { quote = QT_QUOTE; continue; }
        if( _stricmp(opt,"literal") == 0 ) { quote = QT_LITERAL; continue; }
        for( val = opt; *val; val++ )
        {
            if( *val == '=' ) break;
        }
        if( !val[0] || !val[1])
        {
            char errmess[80];
            sprintf(errmess,"Missing value for option %.20s in column command",
                    opt);
            send_config_error( cfg, MISSING_DATA, errmess );
            return OK;
        }
        *val++ = 0;
        if( _stricmp(opt,"width") == 0 )
        {
            if( sscanf(val,"%d",&width) == 1 && width >= 0 ) continue;
        }
        else if( _stricmp(opt,"align") == 0 )
        {
            switch( val[0] )
            {
            case 'l': case 'L': just = JST_LEFT; continue;
            case 'c': case 'C': just = JST_CENTRE; continue;
            case 'r': case 'R': just = JST_RIGHT; continue;
            }
        }
        else if( _stricmp(opt,"ndp") == 0 )
        {
            if( sscanf(val,"%d",&ndp) == 1 && ndp >= 0 ) continue;
        }
        else if( _stricmp(opt,"header") == 0 )
        {
            interpret_escaped_string( val, header, 1024 );
            continue;
        }
        else if( _stricmp(opt,"prefix") == 0 )
        {
            interpret_escaped_string( val, prefix, MAX_PREFIX );
            continue;
        }
        else if( _stricmp(opt,"suffix") == 0 )
        {
            interpret_escaped_string( val, suffix, MAX_PREFIX );
            continue;
        }
        else
        {
            char errmess[80];
            sprintf(errmess,"Invalid option %.20s in column command",opt);
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }
        {
            char errmess[80];
            sprintf(errmess,"Invalid value %.20s for option %.20s",val,opt);
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
    char *bfn;
    coordsys *xyzcs;
    double lat, lon;
    CFG_FILE *cfg = 0;
    const char *cfn;
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

    bfn = argv[1];
    b = open_binary_file( bfn, BINFILE_SIGNATURE );

    if( !b ||
            reload_snap_globals( b ) != OK ||
            reload_stations( b ) != OK ||
            reload_filenames( b ) != OK  ||
            reload_rftransformations( b ) != OK )
    {

        printf( "Cannot reload data from binary file %s\n", bfn);
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

    cfn = find_file( basecfn, ".tbf", bfn, FF_TRYALL, "snaplist" );
    if( cfn ) { cfg = open_config_file( cfn, '!' );}
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

    printf("\nUsing configuration file %s\n",cfn);
    read_config_file( cfg, main_commands );
    close_config_file( cfg );

    fclose(out);
    return 0;
}
