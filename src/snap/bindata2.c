#include "snapconfig.h"


/*
   $Log: bindata2.c,v $
   Revision 1.6  2004/04/22 02:35:42  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.5  2003/11/25 01:29:58  ccrook
   Updated SNAP to allow calculation of projection bearings in coordinate
   systems other than that of the coordinate file

   Revision 1.4  1998/06/15 02:24:29  ccrook
   Modified to handle long integer number of observations.

   Revision 1.3  1996/07/12 20:28:24  CHRIS
   Added date field as a possible output in the residual columns

   Revision 1.2  1996/02/23 17:06:10  CHRIS
   Added support for MDE in residual listing.

   Revision 1.1  1996/01/03 21:56:07  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "snap/snapglob.h"
#include "snapdata/datatype.h"
#include "snapdata/survdata.h"
#include "snap/bearing.h"
#include "snap/rftrans.h"
#include "util/chkalloc.h"
#include "snap/bindata.h"
#include "bindata2.h"
#include "obsdata.h"
#include "vecdata.h"
#include "pntdata.h"
#include "util/classify.h"
#include "snapdata/gpscvr.h"
#include "snapdata/stnrecode.h"
#include "adjparam.h"
#include "util/errdef.h"
#include "output.h"
#include "util/leastsqu.h"
#include "util/lsobseq.h"
#include "snap/survfile.h"
#include "sortobs.h"
#include "util/progress.h"
#include "residual.h"
#include "snap/stnadj.h"
#include "snap/genparam.h"
#include "stnobseq.h"
#include "coefs.h"
#include "snap/datastat.h"
#include "util/dms.h"
#include "util/dateutil.h"
#include "util/dstring.h"
#include "util/pi.h"

/* Definition of output fields that may be put in a residual listing file */

typedef struct
{
    int id;
    const char *code;
    const char *title1;  /* Default titles - title2 used for vector formats only */
    const char *title2;
    const char *source;
    char *buffer;
    int width;
    char justify;
    char vector_title2;  /* title2 is for vector formats only */
    int requested;

} listing_field_def;

static char obs[30];
static char obserr[20];
static char calc[30];
static char calcerr[20];
static char res[20];
static char reserr[20];
static char altres[20];
static char stdres[20];
static char redundancy[20];
static char typecode[10];
static char flags[10];
static char hgti[20];
static char hgtt[20];
static char slpdst[20];
static char arcdst[20];
static char hgtdiff[20];
static char azimuth[20];
static char prjaz[20];
static char mde[20];
static char obsdate[20];
static char obsid[20];
static char significance[20];
static char vecrescmp[6];

#define LEFT_JUST 1
#define RIGHT_JUST 2

static listing_field_def fields[] =
{
    { OF_FROM,       "from",         "From",     NULL,    NULL, NULL,      0, LEFT_JUST, 0, 0},
    { OF_TO,         "to",           "To",       NULL,    NULL, NULL,      0, LEFT_JUST, 0, 0},
    { OF_FROMNAME,   "from_name",    "From",     NULL,    NULL, NULL,     20, LEFT_JUST, 0, 0},
    { OF_TONAME,     "to_name",      "To",       NULL,    NULL, NULL,     20, LEFT_JUST, 0, 0},
    { OF_HI,         "hgt_inst",     "H.I.",     NULL,    NULL, hgti,      6, 0, 0, 0},
    { OF_HT,         "hgt_trgt",     "H.T.",     NULL,    NULL, hgtt,      6, 0, 0, 0},
    { OF_TYPE,       "type",         "Type",     NULL,    NULL, typecode,  4, LEFT_JUST, 0, 0},
    { OF_FILENAME,   "file",         "File",     NULL,    NULL, NULL,     20, LEFT_JUST, 0, 0},
    { OF_FILENO,     "file_no",      "Fl",       NULL,    NULL, NULL,      2, 0, 0, 0},
    { OF_LINENO,     "line_no",      "Lin",      NULL,    NULL, NULL,      3, 0, 0, 0},
    { OF_OBS,        "obs_val",      "Value",    "X,Y,Z", NULL, obs,       0, 0, 1, 0},
    { OF_OBSERR,     "obs_err",      "+/- ",     NULL,    NULL, obserr,    0, 0, 0, 0},
    { OF_CALC,       "calc_val",     "Calc",     "X,Y,Z", NULL, calc,      0, 0, 1, 0},
    { OF_CALCERR,    "calc_err",     "+/- ",     NULL,    NULL, calcerr,   0, 0, 0, 0},
    { OF_RES,        "res_val",      "Res",      vecrescmp, NULL, res,       0, 0, 1, 0},
    { OF_RESERR,     "res_err",      "+/- ",     NULL,    NULL, reserr,    0, 0, 0, 0},
    { OF_ALTRES,     "alt_res",      "Res*",     NULL,    NULL, altres,    0, 0, 0, 0},
    { OF_SRES,       "std_res",      "S.R.",     NULL,    NULL, stdres,    6, 0, 0, 0},
    { OF_REDUNDANCY, "redundancy",   "Rdncy",    NULL,    NULL, redundancy,6, 0, 0, 0},
    { OF_FLAGS,      "flags",        NULL,       NULL,    NULL, flags,     4, LEFT_JUST, 0, 0},
    { OF_AZIMUTH,    "azimuth",      "Azimuth",  NULL,    NULL, azimuth,   0, 0, 0, 0},
    { OF_PRJAZ,      "prj_azimuth",  "Projection", "Azimuth", NULL, prjaz, 0, 0, 0, 0},
    { OF_HGTDIFF,    "hgt_diff",     "Hgt dif",  NULL,    NULL, hgtdiff,   0, 0, 0, 0},
    { OF_ARCDST,     "arc_dist",     "Arc dst",  NULL,    NULL, arcdst,    0, 0, 0, 0},
    { OF_SLPDST,     "slp_dist",     "Slp dst",  NULL,    NULL, slpdst,    0, 0, 0, 0},
    { OF_MDE,        "mde",          "MDE",      NULL,    NULL, mde,       6, 0, 0, 0},
    { OF_SIG,        "significance", "sig(%)",   NULL,    NULL, significance,8, 0, 0, 0},
    { OF_DATE,       "date",         "Date",     NULL,    NULL, obsdate     ,10, 0, 0, 0},
    { OF_OBSID,      "id",           "Id",       NULL,    NULL, obsid        ,8, RIGHT_JUST, 0, 0},
};

#define WANT(fld) (fields[fld].requested)

/* Default formats defined by field/width pairs, terminated by -1 */

static int default_line_format[] =
{
    OF_FROM, 0, OF_TO, 0, OF_OBSID, 0, OF_TYPE, 0,
    OF_OBS, 11, OF_OBSERR, 6, OF_CALC, 11, OF_CALCERR, 6,
    OF_RES, 7, OF_RESERR, 6,
    OF_SRES, 6, OF_FLAGS, 4, -1
};

static int default_point_format[] =
{
    OF_FROM, 0, OF_OBSID, 0, OF_TYPE, 0,
    OF_OBS, 14, OF_OBSERR, 5, OF_CALC, 14, OF_CALCERR, 5,
    OF_RES, 6, OF_RESERR, 5,
    OF_SRES, 6, OF_FLAGS, 4, -1
};

#define MAX_COLUMNS 40
#define SPACE_FIELD 256
#define NEWLINE_FIELD 257
#define CLASSIFICATION_FIELD 512
#define INVALID_FIELD -1

typedef struct
{
    int column;
    int width;
    const char *title1;
    const char *title2;
    char *data;
} listing_column;

typedef struct
{
    int ncolumn;
    listing_column col[MAX_COLUMNS];
} listing_def;

typedef struct column_heading_def_s
{
    char *heading;
    struct column_heading_def_s *next;
} column_heading_def;

static listing_def *listing_format = NULL;
static listing_def data_format[NOBSTYPE] = {0};
static int listing_title = -1;
static int title_id[NOBSTYPE] = {0};
static int defining_format[NOBSTYPE] = {0};
column_heading_def *headings;


static int maxrow, maxlt, last_file_loc;

static void program_error( const char *msg, const char *routine )
{
    char msg1[120];
    char msg2[80];
    sprintf(msg1,"Internal program error: %.100s",msg);
    sprintf(msg2,"Occurred in %.60s",routine);
    handle_error( INTERNAL_ERROR, msg1, msg2 );
}

static void list_datatypes_used( FILE *out )
{
    int type;

    fprintf(out,"\nThe following codes are used to identify data types\n");
    for( type=0; type<NOBSTYPE; type++ ) if( obstypecount[type] )
        {
            fprintf(out,"   %-2s  %ss\n",datatype[type].code,datatype[type].name);
        }
}



void print_input_data( FILE *out )
{
    print_section_header( out, "INPUT DATA" );
    print_json_start(out,"observations_types");
    print_json_observation_types( out );
    print_json_end(out,"observations_types");
    print_json_start(out,"observations");
    print_json_observations( out );
    print_json_end(out,"observations");
    print_section_footer( out );
}


void print_station_recoding( FILE *out )
{
    if( output_stn_recode && stnrecode && recodes_used(stnrecode) )
    {
        /* Note: for the moment print all recoding.  Ultimately may only want those used. */
        print_section_header( out, "RECODED STATIONS" );
        fprintf(out,"\nThe following stations are being recoded as they are read from data files\n\n");
        print_stn_recode_list( out, stnrecode, 1, stn_name_width,"     ");
        fprintf(out,"\n");
        print_section_footer( out );
    }
}


int max_syserr_params( survdata *sd )
{
    int iobs;
    int maxsys;
    if( !sd->nsyserr ) return 0;
    maxsys = 0;
    for( iobs = 0; iobs < sd->nobs; iobs++ )
    {
        trgtdata *t;
        t = get_trgtdata( sd, iobs);
        if( t->nsyserr > maxsys ) maxsys = t->nsyserr;
    }
    return maxsys;
}

static void syserr_obseq( survdata *sd, void *hA )
{
    int iobs, nvec;
    int irow;

    for( iobs = 0, irow = 1; iobs < sd->nobs; iobs++, irow += nvec )
    {
        trgtdata *t;
        syserrdata *se;
        int isys, iaxis;

        t = get_trgtdata( sd, iobs );
        nvec = datatype[t->type].isvector ? 3 : 1;
        if( !t->nsyserr ) continue;
        se = sd->syserr+t->isyserr;
        for( isys=0; isys < t->nsyserr; isys+=nvec )
            for( iaxis = 0; iaxis < nvec; iaxis++, se++ )
            {
                double effect;
                effect = syserr_correction(se->prm_id,se->influence,hA,irow+iaxis);
                oe_add_value( hA, irow+iaxis, -effect );
            }
    }
}

static int bindata_obseq( bindata *b, void *hA )
{
    survdata *sd;
    int nsyserr;
    int status = INTERNAL_ERROR;
    if( b->bintype == SURVDATA )
    {
        sd = (survdata *) b->data;
        nsyserr = max_syserr_params( sd );
        switch( sd->format )
        {
        case SD_OBSDATA: status = obsdata_obseq( sd, hA, nsyserr ); break;
        case SD_VECDATA: status = vecdata_obseq( sd, hA, nsyserr ); break;
        case SD_PNTDATA: status = pntdata_obseq( sd, hA, nsyserr ); break;
        default: program_error("Invalid survdata format","bindata_obseq");
            break;
        }
        if( nsyserr ) syserr_obseq( sd, hA );
    }
    else
    {
        program_error("Invalid binary data format","bindata_obseq");
    }
    return status;
}


/*
static void print_obsheader( FILE *lst, bindata *b )
{
    survdata *sd;
    trgtdata *tgt=0;
    int ntgt;

    sd = (survdata *) b->data;
    ntgt = sd->nobs;

    tgt=get_trgtdata(sd,0);
    fprintf(lst,"\nFile %s: line %d: Station ",
            survey_data_file_name(sd->file),(int)(tgt->lineno));
    if( sd->from ) { fprintf(lst,"%s ",stnptr(sd->from)->Code ); }
    if( tgt->to ) { fprintf( lst, "%s%s ",(sd->from ? "to " : ""),stnptr(tgt->to)->Code);}
    fprintf(lst,": %s",datatype[tgt->type].code);
    if( ntgt > 1 ) fprintf(lst," ...");
    fprintf(lst,"\n\n");
}
*/


int sum_bindata( int iteration )
{
    char header[30];
    void *hA;
    bindata *b;
    int nrow;
    long nbin;
    int sts=OK;

    if( output_observation_equations )
    {
        sprintf(header,"obs_equation_%d",iteration);
        print_section_header(lst, "OBSERVATION EQUATIONS");
        print_json_start(lst,header);
        fprintf(lst,"{\n");
        print_json_params(lst,2);
        fprintf(lst,",\n  \"obs_equations\": [\n");
    }

    maxrow = maxlt = 0;
    hA = create_oe( nprm );
    b = create_bindata();
    init_get_bindata( 0L );
    init_progress_meter( nbindata );
    nbin = 0;
    while( get_bindata( b ) == OK )
    {
        nbin++;
        update_progress_meter( nbin );
        if ( b->bintype != NOTEDATA )
        {
            int stsobs=bindata_obseq( b, hA );
            if( stsobs != OK ) 
            {
                sts=stsobs;
                continue;
            }
            if( output_observation_equations )
            {
                char source[200];
                survdata *sd = (survdata *) b->data;
                trgtdata *tgt=get_trgtdata(sd,0);
                sprintf(source,"{\"file\": \"%.80s\",\"lineno\": %d, \"station\": \"%s%s%s\", \"obsid\": %d, \"type\": \"%s\",\"nobs\": %d}",
                    survey_data_file_name(sd->file),
                    (int)(tgt->lineno),
                    sd->from ? stnptr(sd->from)->Code : "",
                    sd->from && tgt->to ? " - " : "",
                    tgt->to ? stnptr(tgt->to)->Code : "",
                    tgt->obsid,
                    datatype[tgt->type].code,
                    sd->nobs
                    );
                if( nbin > 1 )  fprintf(lst,",\n");
                print_obseqn_json( lst, hA, source, 0 );
            }
            stsobs=lsq_sum_obseqn( hA );
            if( stsobs != OK )
            {
                char location[200];
                survdata *sd = (survdata *) b->data;
                trgtdata *tgt=get_trgtdata(sd,0);
                sprintf(location,"Cannot sum observation from %.80s line %d\n",
                        survey_data_file_name(sd->file),
                        (int)(tgt->lineno)
                       );
                handle_error(INVALID_DATA,"Observation error",location);
                sts=stsobs;
            }
            nrow = obseqn_rows( hA );
            if( nrow > maxrow ) maxrow = nrow;
            if( !obseqn_cvr_diagonal(hA) && nrow > maxlt ) maxlt = nrow;
        }
    }
    end_progress_meter();

    delete_bindata(b);
    delete_oe( hA );

    if( output_observation_equations )
    {
        fprintf(lst,"\n]}\n");
        print_json_end(lst,header);
        print_section_footer( lst );
    }
    return sts;
}


void calc_residuals( void )
{
    void *hA;
    bindata *b;
    lsdata l;
    long maxelt;
    long nbin;
    survdata *sd;

    /* Allocate space for the least squares results */

    if( maxrow <= 0 ) return;

    l.calc = (double *) check_malloc( maxrow * sizeof(double) );
    l.res  = (double *) check_malloc( maxrow * sizeof(double) );

    maxelt = ( (long)maxlt * (maxlt+1) ) / 2;
    if( maxelt < maxrow ) maxelt = maxrow;

    l.calccvr = (ltmat) malloc( maxelt * sizeof( double ) );
    l.rescvr  = (ltmat) malloc( maxelt * sizeof( double ) );
    l.sch=0.0;
    l.schvar=0.0;
    l.diagonal=0;

    hA = create_oe( nprm );
    b = create_bindata();

    init_get_bindata( 0L );

    nbin = 0;
    init_progress_meter( nbindata );

    for(;;)
    {

        nbin++;
        update_progress_meter( nbin );

        if( get_bindata( b ) != OK ) break;

        if ( b->bintype == NOTEDATA ) continue;
        if ( b->bintype != SURVDATA )
        {
            program_error("Invalid binary data format","calc_residuals");
            continue;
        }

        if( bindata_obseq( b, hA ) != OK ) continue;
        l.diagonal = obseqn_cvr_diagonal( hA );

        lsq_calc_obs( hA, l.calc, l.res, &l.sch, &l.schvar,
                      l.diagonal, l.calccvr, l.rescvr );

        sd = (survdata *) b->data;

        switch( sd->format )
        {
        case SD_OBSDATA: calc_obsdata_residuals( sd, &l ); break;

        case SD_VECDATA: calc_vecdata_residuals( sd, &l ); break;

        case SD_PNTDATA: calc_pntdata_residuals( sd, &l ); break;

        default: program_error("Invalid survdata format","calc_residuals");
        }

        update_bindata( b );
    }

    end_progress_meter();

    delete_bindata( b );
    delete_oe( hA );

    free(l.rescvr);
    free(l.calccvr);
    check_free(l.res);
    check_free(l.calc);
}

/* Print residual title is the same as print residual line, except that
   fields which are not left justified are centred */

static void print_title( FILE *out )
{
    static const char *blank = "";
    static const char *lftjst = "%-*s";
    static const char *centrejst = "%-*s%-*s";
    listing_column *column;
    int ncolumn;
    int need_space;
    int ipass, justify;
    fputc('\n',out);
    for( ipass = 0; ipass < 2; ipass++ )
    {
        int needpass2 = 0;
        need_space = 0;
        column = listing_format->col;
        ncolumn = listing_format->ncolumn;
        for( ; ncolumn--; column++ )
        {
            int width = column->width;
            const char *source = (ipass == 0) ? column->title1 : column->title2;
            if( column->title2 ) needpass2 = 1;
            if( !source ) source = blank;
            if( column->column == SPACE_FIELD )
            {
                fprintf(out,"%-*s",width," ");
                need_space = 0;
            }
            if( column->column == NEWLINE_FIELD ) fputc('\n',out);
            if( column->column == SPACE_FIELD || column->column == NEWLINE_FIELD )
            {
                need_space = 0;
                justify = LEFT_JUST;
            }
            else if( column->column & CLASSIFICATION_FIELD )
            {
                justify=LEFT_JUST;
                need_space = 1;
            }
            else
            {
                listing_field_def *fld;
                fld = fields + column->column;
                if( fld->id == OF_OBSID && ! have_obs_ids ) continue;
                if( need_space ) fputc( ' ', out );
                need_space = 1;
                justify = fld->justify;
            }

            if( justify == LEFT_JUST )
            {
                fprintf(out,lftjst,width,source);
            }
            else
            {
                int len1, len2;
                len1 = width - strlen(source);
                if( justify != RIGHT_JUST ) len1 /= 2;
                if( len1 < 0 ) len1 = 0;
                len2 = width - len1;
                fprintf(out,centrejst,len1,blank,len2,source);
            }
        }
        fprintf(out,"\n");
        if( !needpass2 ) break;
    }
    clear_residual_fields();
}

static void setup_default_format( int type );

int define_residual_formats( char *typelist, int add_columns )
{
    int status = MISSING_DATA;
    char *tp, *end, save;
    int itype;

    for( itype = 0; itype < NOBSTYPE; itype++ ) defining_format[itype] = 0;

    tp = typelist;
    while( *tp )
    {
        end = tp;
        while( *end && *end != '/' ) end++;
        save = *end;
        *end = 0;
        if( _stricmp( tp, "ALL" ) == 0 )
        {
            for( itype = NOBSTYPE; itype--; )
            {
                defining_format[itype] = 1;
            }
            status = OK;
        }
        else if( _stricmp( tp, "POINT" ) == 0 )
        {
            for( itype = NOBSTYPE; itype--; ) if( datatype[itype].ispoint )
                {
                    defining_format[itype] = 1;
                }
            status = OK;
        }
        else if( _stricmp( tp, "VECTOR" ) == 0 )
        {
            for( itype = NOBSTYPE; itype--; ) if( datatype[itype].isvector )
                {
                    defining_format[itype] = 1;
                }
            status = OK;
        }
        else if( _stricmp( tp, "LINE" ) == 0 )
        {
            for( itype = NOBSTYPE; itype--; )
            {
                if( datatype[itype].isvector ) continue;
                if( datatype[itype].ispoint ) continue;
                defining_format[itype] = 1;
            }
            status = OK;
        }
        else
        {
            for( itype = NOBSTYPE; itype--; )
            {
                if( _stricmp(datatype[itype].code,tp) == 0 )
                {
                    defining_format[itype] = 1;
                    status = OK;
                    break;
                }
            }
            if( itype < 0 ) status = INVALID_DATA;
        }
        *end = save;
        tp = end;
        if( *tp ) tp++;
        if( status != OK ) break;
    }
    /* If string doesn't define a type, then assume all types are being defined
     * and return INVALID_DATA
     */
    if( status != OK )
    {
        for( itype = 0; itype < NOBSTYPE; itype++ ) defining_format[itype] = 1;
    }
    for( itype = 0; itype < NOBSTYPE; itype++ )
    {
        if( ! defining_format[itype] ) continue;
        if( add_columns )
        {
            if( ! data_format[itype].ncolumn ) setup_default_format( itype );
        }
        else
        {
            data_format[itype].ncolumn = 0;
        }
    }
    return status;
}

int set_residual_listing_data_type( FILE *out, int newtype )
{
    if( listing_format != &data_format[newtype] )
    {
        listing_format = &data_format[newtype];
        if( title_id[newtype] != listing_title ) print_title( out );
        listing_title = title_id[newtype];
        return 1;
    }
    return 0;
}

static char *get_column_heading( const char *text )
{
    column_heading_def *hdr, **phdr;
    if( !text ) return NULL;
    for( hdr = headings, phdr = &headings; hdr; phdr = &hdr->next, hdr = hdr->next )
    {
        if( strcmp(hdr->heading,text) == 0 ) return hdr->heading;
    }
    *phdr = (column_heading_def *) check_malloc( sizeof( column_heading_def ) );
    hdr = *phdr;
    hdr->next = NULL;
    hdr->heading = copy_string( text );
    return hdr->heading;
}

void clear_residual_field_defs()
{
    listing_format->ncolumn = 0;
}

static int add_residual_field_def( int type, const char *code, int width, const char *title1, const char *title2 )
{
    int i;
    int column;
    listing_column *lc;
    listing_def *listing_format;

    listing_format = &data_format[type];
    if( listing_format->ncolumn >= MAX_COLUMNS ) return TOO_MUCH_DATA;

    column = INVALID_FIELD;
    title1 = get_column_heading( title1 );
    title2 = get_column_heading( title2 );

    for( i = 0; i < OF_COUNT; i++ )
    {
        if( _stricmp( code, fields[i].code ) == 0 )
        {
            column = i;
            if( !title1 && !title2 )
            {
                title1 = get_column_heading( fields[i].title1 );
                title2 = (!fields[i].vector_title2 || datatype[type].isvector) ?
                         get_column_heading( fields[i].title2 ) :
                         NULL;
            }
            break;
        }
    }

    if( column == INVALID_FIELD && _stricmp(code,"S") == 0 )
    {
        column = SPACE_FIELD;
    }

    if( column == INVALID_FIELD && _stricmp(code,"NL") == 0 )
    {
        column = NEWLINE_FIELD;
    }

    if( column == INVALID_FIELD && _strnicmp(code,"C=",2) == 0 )
    {
        column = classification_id( &obs_classes, code+2, 1 );
        if( !title1 && !title2 )
        {
            title1 = get_column_heading( classification_name( &obs_classes, column ) );
        }
        column |= CLASSIFICATION_FIELD;
    }

    if( column != INVALID_FIELD )
    {
        lc = listing_format->col + listing_format->ncolumn;
        lc->column = column;
        lc->width = width;
        lc->title1 = title1;
        lc->title2 = title2;
        listing_format->ncolumn++;
    }

    return column == INVALID_FIELD ? INVALID_DATA : OK;
}

int add_residual_field( const char *code, int width, const char *title1, const char *title2 )
{
    int itype;
    for( itype = 0; itype < NOBSTYPE; itype++ )
    {
        if( defining_format[itype] )
        {
            int sts;
            sts = add_residual_field_def( itype, code, width, title1, title2 );
            if( sts != OK ) return sts;
        }
    }
    return OK;
}

static void merge_residual_titles( void )
{
    /* First pass - find all formats with identical title1, title2, and
       number of fields */
    int itype, jtype, icol;
    listing_def *idef, *jdef;
    for( itype = 0; itype < NOBSTYPE; itype++ )
    {
        idef = &data_format[itype];
        title_id[itype] = itype;
        for( jtype = 0; jtype < itype; jtype++ )
        {
            int same = 1;
            int icol;
            if( title_id[jtype] != jtype ) continue;
            jdef = &data_format[jtype];
            if( idef->ncolumn != jdef->ncolumn ) same = 0;
            for( icol = 0; same && icol < idef->ncolumn; icol++ )
            {
                if( idef->col[icol].title1 != jdef->col[icol].title1 ||
                        idef->col[icol].title2 != jdef->col[icol].title2 ) same = 0;
            }
            if( same ) { title_id[itype] = jtype; break; }
        }
    }
    /* Second pass, merge column widths */

    for( itype = 0; itype < NOBSTYPE; itype++ )
    {
        /* Are the column headings identical to those for another data type */
        if( title_id[itype] != itype ) continue;

        idef = &data_format[itype];
        for( icol = 0; icol < idef->ncolumn; icol++ )
        {
            int maxwidth = idef->col[icol].width;
            const char *title;
            int ttlen;
            for( jtype = itype+1; jtype < NOBSTYPE; jtype++ )
            {
                if( title_id[jtype] != itype ) continue;
                if( data_format[jtype].col[icol].width > maxwidth )
                    maxwidth = data_format[jtype].col[icol].width;
            }
            title = idef->col[icol].title1;
            ttlen = title ? strlen(title) : 0;
            if( ttlen > maxwidth ) maxwidth = ttlen;
            title = idef->col[icol].title2;
            ttlen = title ? strlen(title) : 0;
            if( ttlen > maxwidth ) maxwidth = ttlen;

            for( jtype = itype; jtype < NOBSTYPE; jtype++ )
            {
                if( title_id[jtype] != itype ) continue;
                data_format[jtype].col[icol].width = maxwidth;
            }
        }
    }
}

void print_residual_line( FILE *out )
{
    static const char *blank = "";
    static const char *lftjst = "%-*s";
    static const char *rgtjst = "%*s";
    listing_column *col;
    int ncolumn = listing_format->ncolumn;
    int need_space = 0;
    int i;
    for( i = 0, col = listing_format->col; i<ncolumn; i++, col++ )
    {
        if( col->column == SPACE_FIELD )
        {
            fprintf(out,"%-*s",col->width," ");
            need_space = 0;
        }
        else if( col->column == NEWLINE_FIELD )
        {
            if( need_space ) fputc(' ',out);
            fprintf(out,"%-*s",col->width+1,"\n");
            need_space = 0;
        }
        else if( col->column & CLASSIFICATION_FIELD )
        {
            fprintf(out,"%-*s",col->width,col->data ? col->data : "" );
            need_space = 1;
        }
        else
        {
            listing_field_def *fld;
            const char *source;
            fld = fields + col->column;
            if( fld->id == OF_OBSID && ! have_obs_ids ) continue;
            source = fld->source;
            if( !source ) source = blank;
            if( need_space ) fputc(' ',out);
            fprintf(out,fld->justify == LEFT_JUST ? lftjst : rgtjst, col->width, source );
            need_space = 1;
        }
    }
    fprintf(out,"\n");
    clear_residual_fields();
}

void clear_residual_fields( void )
{
    int i;
    for( i=0; i < OF_COUNT; i++ )
    {
        fields[i].source = NULL;
    }
}

void set_residual_field( int field_id, const char *value )
{
    fields[field_id].source = value;
}

void clear_residual_field( int field_id )
{
    fields[field_id].source = NULL;
}

static void set_calculated_fields( survdata *sd, trgtdata *t )
{
    station *stf, *stt;
    if( ! sd->from || ! t->to ) return;
    stf = stnptr( sd->from );
    stt = stnptr( t->to );
    if( WANT(OF_SLPDST) )
    {
        double value;
        value = calc_distance( stf, 0.0, stt, 0.0, NULL, NULL );
        set_residual_field_value(OF_SLPDST,obs_precision[SD],value);
    }
    if( WANT(OF_ARCDST) )
    {
        double value;
        double hgtf;
        double hgtt;
        hgtf = - (stf->OHgt + stf->GUnd);
        hgtt = - (stt->OHgt + stt->GUnd);
        value = calc_distance( stf, hgtf, stt, hgtt, NULL, NULL );
        value *= ellipsoidal_distance_correction( stf, stt );
        set_residual_field_value(OF_ARCDST,obs_precision[ED],value);
    }
    if( WANT(OF_AZIMUTH) || WANT(OF_PRJAZ) )
    {
        double value;
        static void *azfmt = NULL;
        static char isproj;
        if( !azfmt )
        {
            isproj = is_projection(net->crdsys);
            azfmt = create_dms_format( 3,obs_precision[AZ],0,NULL,NULL,NULL,NULL,NULL);
        }
        if( WANT(OF_AZIMUTH) )
        {
            value = calc_azimuth( stf, 0.0, stt, 0.0, 0, NULL, NULL );
            while( value > TWOPI ) value -= TWOPI;
            while( value < 0.0 ) value += TWOPI;
            set_residual_field_dms( OF_AZIMUTH, azfmt, value*RTOD );
        }
        if( WANT( OF_PRJAZ ) && isproj)
        {
            value = calc_prj_azimuth( net, stf, 0.0, stt, 0.0, NULL, NULL );
            while( value > TWOPI ) value -= TWOPI;
            while( value < 0.0 ) value += TWOPI;
            set_residual_field_dms( OF_PRJAZ, azfmt, value*RTOD );
        }
    }
    if( WANT(OF_HGTDIFF) )
    {
        double value;
        value = calc_hgt_diff( stf, 0.0, stt, 0.0, NULL, NULL );
        set_residual_field_value( OF_HGTDIFF, obs_precision[LV], value );
    }
}


static void set_date_field( survdata *sd )
{
    if( WANT(OF_DATE) )
    {
        if( sd->date == UNDEFINED_DATE )
        {
            strcpy(obsdate,"Unknown");
        }
        else
        {
            int dy,mn,yr;
            date_as_ymd(sd->date,&yr,&mn,&dy);
            sprintf(get_field_buffer(OF_DATE),"%2d/%02d/%04d",
                    (int)dy, (int)mn, (int)yr);
        }
    }
}

void set_residual_field_value( int id, int ndp, double value )
{
    char buffer[256];
    char *buf = get_field_buffer(id);
    sprintf( buffer, "%.*lf", (int) ndp, value );
    if( strlen(buffer) < 20 ) strcpy(buf,buffer);
    else sprintf(buf,"%.8e",value);
}

void set_survdata_fields( survdata *sd )
{
    if( sd->from )
    {
        set_residual_field( OF_FROM, station_code( sd->from ));
        set_residual_field( OF_FROMNAME, stnptr(sd->from)->Name );
        set_residual_field_value( OF_HI, 3, sd->fromhgt );
    }
    set_date_field( sd );
}

void set_trgtdata_fields( trgtdata *t, survdata *sd )
{
    static char lineno[10];
    static char fileno[10];
    int i;

    if( sd->from )
    {
        if( t->to )
        {
            set_residual_field( OF_TO, station_code( t->to ));
            set_residual_field( OF_TONAME, stnptr(t->to)->Name );
            set_residual_field_value(OF_HT,3,t->tohgt);
            set_calculated_fields(sd,t);
        }
    }
    else
    {
        set_residual_field( OF_FROM, station_code( t->to ));
        set_residual_field( OF_FROMNAME, stnptr(t->to)->Name );
        set_residual_field_value(OF_HI,3,t->tohgt);
    }
    sprintf( fileno, "%d", (int) (sd->file) );
    set_residual_field( OF_FILENO, fileno );
    set_residual_field( OF_FILENAME, survey_data_file_name( sd->file ));
    sprintf( lineno, "%d", (int) (t->lineno) );
    set_residual_field( OF_LINENO, lineno );
    sprintf(obsid,"%d",(int) (t->id));
    set_residual_field( OF_OBSID, obsid);

    for( i = 0; i < listing_format->ncolumn; i++ )
    {
        if( listing_format->col[i].column & CLASSIFICATION_FIELD )
        {
            int class_id;
            class_id = listing_format->col[i].column & ~CLASSIFICATION_FIELD;
            listing_format->col[i].data = get_obs_classification_name( sd, t, class_id );
        }
    }
}

char *get_field_buffer( int id )
{
    fields[id].source = fields[id].buffer;
    fields[id].buffer[0] = 0;
    return fields[id].buffer;
}

void set_residual_field_dms( int id, void *format, double value )
{
    dms_string(value, format, get_field_buffer(id));
}


static void setup_data_field_widths( int itype )
{
    int icol;
    int obswid, errwid, reswid;
    listing_def *format;
    int change;

    if( obstypecount[itype] == 0 ) return;
    change = obs_precision[itype] - datatype[itype].dfltndp;
    if( !obs_precision[itype] ) change++;
    obswid = datatype[itype].dfltwidth + change;
    errwid = datatype[itype].dflterrwid + change;
    reswid = datatype[itype].dfltreswid + change;

    format = &data_format[itype];
    for( icol = 0; icol < format->ncolumn; icol++ )
    {
        int width = format->col[icol].width;
        int width0 = width;
        // if( width > 0 ) continue;
        switch( format->col[icol].column )
        {
        case OF_OBS:
        case OF_CALC:     width = obswid; break;

        case OF_ALTRES:
        case OF_RES:      width = reswid; break;

        case OF_OBSERR:
        case OF_CALCERR:
        case OF_RESERR:   width = errwid; break;
        }
        if( width > width0 ) format->col[icol].width = width;
    }
}

static void setup_calculated_field_widths( void )
{
    fields[OF_AZIMUTH].width = datatype[AZ].dfltwidth - datatype[AZ].dfltndp + obs_precision[AZ];
    fields[OF_PRJAZ].width   = datatype[AZ].dfltwidth - datatype[AZ].dfltndp + obs_precision[AZ];
    fields[OF_HGTDIFF].width = datatype[LV].dfltwidth - datatype[LV].dfltndp + obs_precision[LV];
    fields[OF_ARCDST] .width = datatype[ED].dfltwidth - datatype[ED].dfltndp + obs_precision[ED];
    fields[OF_SLPDST] .width = datatype[SD].dfltwidth - datatype[SD].dfltndp + obs_precision[SD];
}


static void setup_format_columns( listing_def *format )
{
    int ncolumn;
    listing_column *col = format->col;
    for( ncolumn = format->ncolumn; ncolumn--; col++ )
    {
        if( col->column == SPACE_FIELD ) continue;
        if( col->column == NEWLINE_FIELD ) continue;
        if( col->column & CLASSIFICATION_FIELD )
        {
            int class_id, class_count, ic, len, width;
            if( col->width ) continue;
            class_id = col->column & ~CLASSIFICATION_FIELD;
            class_count = class_value_count( &obs_classes, class_id );
            width = 0;
            for( ic = 0; ic < class_count; ic++ )
            {
                len = strlen( class_value_name( &obs_classes, class_id, ic ) );
                if( len > width ) width = len;
            }
            col->width = width;
        }
        else
        {
            fields[col->column].requested = 1;
            if( !col->width ) col->width = fields[col->column].width;
        }
    }
}


static void setup_default_format( int type )
{
    int *cols;
    listing_def *format = &data_format[type];

    if( output_xyz_vector_residuals )
    {
        strcpy(vecrescmp,"X,Y,Z");
    }
    else
    {
        strcpy(vecrescmp,"E,N,U");
    }

    if( format->ncolumn ) return;
    if( datatype[type].ispoint )
    {
        cols = default_point_format;
    }
    else
    {
        cols = default_line_format;
    }
    for(;;)
    {
        int fld, width;
        fld = *cols++;
        if( fld < 0 ) break;
        width = *cols++;
        if( width < 0 ) break;
        add_residual_field_def( type, fields[fld].code, width, NULL, NULL );
    }
}

// #pragma warning (disable : 4127)

static void setup_format_definitions( void )
{
    int i;
    int error;
    int itype;


    error = 0;
    if( sizeof(fields)/sizeof(fields[0]) != OF_COUNT ) error = 1;

    if( !error ) for( i=0; i < OF_COUNT; i++ )  if( fields[i].id != i ) { error = 1; break; }

    if( error )
    {
        program_error("Definition of fields does not match enumeration",
                      "setup_residual_field_definitions");
    }

    for( i = 0; i < OF_COUNT; i++ )
    {
        fields[i].requested = 0;
    }

    fields[OF_FROM].width = stn_name_width;
    fields[OF_TO].width = stn_name_width;
    setup_calculated_field_widths();

    /* Formats not explicitly defined are set to point or line
       formats */

    for( itype = 0; itype < NOBSTYPE; itype++ )
    {
        if( obstypecount[itype] <= 0 ) continue;
        setup_default_format( itype );
        setup_format_columns( data_format + itype );
        setup_data_field_widths( itype );
    }

    merge_residual_titles();
    clear_residual_fields();
}

int got_vector_data()
{
    int i;
    for( i = 0; i < NOBSTYPE; i++ )
    {
        if( datatype[i].isvector && obstypecount[i] > 0 ) return 1;
    }
    return 0;
}

void print_residuals( FILE *out )
{
    bindata *b;
    double semult;
    long loc;
    long nbin;
    survdata *sd;

    /* Allocate space for the least squares results */

    setup_format_definitions();

    b = create_bindata();

    print_section_header(out,"OBSERVATION RESIDUALS");
    print_zero_inverse_warning(out);
    print_convergence_warning(out);

    if( WANT(OF_OBSERR) || WANT(OF_RESERR) || WANT(OF_CALCERR) )
    {
        fprintf(out,"\nThis table lists %s errors\n",apriori ? "apriori" : "aposteriori");
    }
    semult = apriori ? 1.0 : seu;


    if( WANT(OF_FLAGS) )
    {
        fprintf(out,"Residuals outside the %6.3lf%% confidence limit %sare flagged %s\n",
                flag_level[0], taumax[0] ? "for the maximum ":"", FLAG1 );
        if( taumax[0] )
        {
            double prob = (100-flag_level[0])/100.0;
            prob = prob_of_maximum( prob, 1 );
            prob = 100 * (1-prob);
            fprintf(out,"(Corresponds to %6.4lf%% confidence limit)\n",prob);
        }

        fprintf(out,"Residuals outside the %6.3lf%% confidence limit %sare flagged %s\n",
                flag_level[1], taumax[1] ? "for the maximum ":"", FLAG2 );
        if( taumax[1] )
        {
            double prob = (100-flag_level[1])/100.0;
            prob = prob_of_maximum( prob, 1 );
            prob = 100 * (1-prob);
            fprintf(out,"(Corresponds to %6.4lf%% confidence limit)\n",prob);
        }

        if( apriori )
        {
            fprintf(out,"\nSignificance is based on the Normal distribution function\n");
        }
        else if( dof < 1 )
        {
            fprintf(out,"The a posteriori significance of residuals cannot be calculated\n");
        }
        else
        {
            fprintf(out,"\nSignificance is based on the Tau distribution with %ld degrees of freedom\n",
                    (long) dof);
            fprintf(out,"(the Students t distribution is used for rejected observations)\n");
        }

        fprintf(out,"\nRejected observations are flagged %c\n",REJECTED_OBS_FLAG);
        if( output_rejected_stations )
        {
            fprintf(out,"Observations to or from rejected stations are flagged %c\n",
                    REJECTED_STN_FLAG );
        }
        else
        {
            fprintf(out,"Observations to or from rejected stations are not listed\n");
        }
        fprintf(out,"Observations with redundancy less than %.2lf are flagged %c\n",
                redundancy_flag_level,LOW_REDUNDANCY_FLAG);
    }

    if( WANT(OF_MDE) )
    {
        double prob;
        prob = flag_level[0];
        if( taumax[0] )
        {
            prob = (1-prob)/100.0;
            prob = prob_of_maximum( prob, 1 );
            prob = 100 * (1-prob);
        }
        fprintf(out,"\nThe marginal detectable error (MDE) is calculated for \n");
        fprintf(out,"signficance %6.3lf%% and power %6.1lf%%\n",prob,mde_power);
        set_mde_level( prob, mde_power );
    }


    if( got_vector_data() && ! output_xyz_vector_residuals )
    {
        if( gps_vertical_fixed() )
        {
            double topolat, topolon;
            void *latfmt, *lonfmt;
            latfmt = create_dms_format( 3, 5, 0, NULL, NULL, NULL, "N", "S" );
            lonfmt = create_dms_format( 3, 5, 0, NULL, NULL, NULL, "E", "W" );
            get_network_topocentre( net, &topolat, &topolon );
            fprintf(out,"\nVector residual east, north, up directions are calculated at\n   ");
            fputs( dms_string( topolat* RTOD, latfmt, NULL ), out );
            fputs( "    ", out );
            fputs( dms_string( topolon* RTOD, lonfmt, NULL ), out );
            fputs( "\n", out );
            check_free( latfmt );
            check_free( lonfmt );
        }
        else
        {
            fprintf(out,"\nVector residual east, north, up directions are calculated at baseline midpoint\n");
        }
    }

    if( WANT(OF_TYPE) ) list_datatypes_used( out );

    if( WANT(OF_CALC) )
    {
        int first = 1;
        if( obstypecount[SD] || obstypecount[DR] )
        {
            fputs( first ? "Note: " : "      " , out ); first = 0;
            fprintf(out,"Calculated values for slope distances include equipment heights\n");
        }
        if( obstypecount[ZD] )
        {
            fputs( first ? "Note: " : "      " , out ); first = 0;
            fprintf(out,"Calculated values for zenith distances include equipment heights\n");
        }
    }

    if( WANT(OF_ALTRES) )
    {
        fprintf(out,"\nThe column headed Res* lists the residuals in an alternative form.\n");
        if( obstypecount[SD] || obstypecount[ED] || obstypecount[MD] || obstypecount[HD] ||
                obstypecount[DR] )
        {
            fputs("   For distances the residual is expressed in ppm\n",out);
        }
        if( obstypecount[HA] || obstypecount[AZ] || obstypecount[PB] )
        {
            fputs("   For horizontal angles, azimuths, and bearings the residual is in metres\n",out);
        }
        if( obstypecount[ZD] )
        {
            fputs("   For zenith distances the residual is in metres\n",out);
        }
        if( obstypecount[LV] )
        {
            fputs("   For height differences the residual is in ppm\n",out);
        }
        if( obstypecount[GB] )
        {
            fputs("   For GPS baselines the residual is in ppm of baseline length\n",out);
        }
    }

    listing_format = NULL;
    listing_title = -1;
    last_file_loc = -1;

    /* The residuals will be listed either in the input order, or if
       sort_obs is not zero, to the order defined by get_sorted_obs_loc */


    if( sort_obs )
    {
        init_get_sorted_obs_loc();
        if( file_location_frequency != 1 ) file_location_frequency = 0;
    }
    else
    {
        init_get_bindata( 0L );
    }

    nbin = 0;
    init_progress_meter( nbindata );

    for(;;)
    {
        if( sort_obs )
        {
            loc = get_sorted_obs_loc();
            if( loc < 0 ) break;
            init_get_bindata( loc );
        }

        nbin++;
        update_progress_meter( nbin );

        if( get_bindata( b ) != OK ) break;

        if ( b->bintype == NOTEDATA ) continue;
        if ( b->bintype != SURVDATA )
        {
            program_error("Invalid binary data type","print_residuals");
            continue;
        }

        sd = (survdata *) b->data;
        switch( sd->format )
        {
        case SD_OBSDATA: list_obsdata_residuals( out, sd, semult ); break;

        case SD_VECDATA: list_vecdata_residuals( out, sd, semult ); break;

        case SD_PNTDATA: list_pntdata_residuals( out, sd, semult ); break;

        default: program_error("Invalid survdata format","print_residuals");
        }
    }

    end_progress_meter();

    delete_bindata( b );

    print_section_footer(out);
}


void list_file_location( FILE *out, int file, int lineno )
{
    static int nwait;
    if( ! output_file_locations ) return;
    if( file_location_frequency <= 0 ) return;
    if( file != last_file_loc )
    {
        last_file_loc = file;
        nwait = 1;
    }
    if( --nwait ) return;
    fprintf(out,"\nFile %s: line %d\n",survey_data_file_name(file),(int)lineno);
    nwait = file_location_frequency;
}


static int obsset = -1;

static void write_observation_csv_common_start( output_csv *csv, survdata *sd, trgtdata *tgt, const char *component )
{
    char type[16];
    station *from = stnptr(sd->from);
    station *to = stnptr(tgt->to);
    if( ! from ) { from = to; to = 0; }
    if( obsset < 0 ) obsset=tgt->obsid;
    strcpy( type, datatype[tgt->type].code);
    if(component && strlen(type)+strlen(component)+2 < 16) { strcat(type,"-"); strcat(type,component); }
    write_csv_int( csv, tgt->obsid );
    if( have_obs_ids ) write_csv_int( csv, tgt->id );
    write_csv_string(csv,from->Code);
    write_csv_string(csv,to ? to->Code : 0);
    write_csv_date(csv,sd->date);
    write_csv_double(csv,sd->fromhgt,3);
    write_csv_double(csv,tgt->tohgt,3);
    write_csv_string(csv,type);
    write_csv_int(csv,obsset);
    if( to ) write_csv_double(csv,calc_distance( from, 0.0, to, 0.0, NULL, NULL ),3);
    else write_csv_null_field(csv);
    write_csv_string(csv,tgt->unused ? "rej" : "use" );
    write_csv_double(csv,tgt->errfct,3);
}

static void write_observation_csv_common_end( output_csv *csv, survdata *sd, trgtdata *tgt )
{
    int i;
    station *from = stnptr(sd->from);
    station *to = stnptr(tgt->to);
    if( ! from ) { from = to; to = 0; }

    for( i = 0; i < classification_count(&obs_classes); i++ )
    {
        write_csv_string(csv,get_obs_classification_name(sd,tgt,i+1));
    }
    write_csv_string(csv,survey_data_file_name(sd->file));
    write_csv_int(csv,tgt->lineno);

    if( output_csv_shape )
    {
        char wkt[128];
        double ef, nf, et=0, nt=0;
        projection *prj = is_projection(net->crdsys) ? net->crdsys->prj : 0;
        int ndp = prj ? 4 : 9;
        if( prj )
        {
            geog_to_proj( prj, from->ELon, from->ELat, &ef, &nf );
            if( to ) geog_to_proj( prj, to->ELon, to->ELat, &et, &nt );

        }
        else
        {
            ef = from->ELon*RTOD;
            nf = from->ELat*RTOD;
            if( to )
            {
                et = to->ELon*RTOD;
                nt = to->ELat*RTOD;
            }
        }
        if( to )
        {
            sprintf(wkt,"LINESTRING(%.*lf %.*lf, %.*lf %.*lf)",
                    ndp,ef,ndp,nf,ndp,et,ndp,nt);
        }
        else
        {
            sprintf(wkt,"POINT(%.*lf %.*lf)",ndp,ef,ndp,nf);
        }
        write_csv_string(csv,wkt);
    }

    end_output_csv_record(csv);
}

//&output_csv_veccomp
//&output_csv_vecsum
//&output_csv_vecinline
//&output_csv_vecenu
//&output_csv_correlations

static void skip_csv_fields( output_csv *csv, int nskip )
{
    while( nskip-- > 0) { write_csv_null_field(csv); }
}

void write_obsdata_csv( output_csv *csv, survdata *sd, obsdata *o, double semult )
{
    trgtdata *t = &(o->tgt);
    int ndp = obs_precision[t->type];
    double mult = 1.0;
    double sres;
    int nskip1=0, nskip2=0;

    if( output_csv_vecinline )
    {
        nskip1 = (output_csv_vecsum ? 3 : 2);
        if( output_csv_correlations ) nskip2 = 3;
    }

    if( datatype[t->type].isangle ) { ndp+=4; mult=RTOD; }

    write_observation_csv_common_start( csv, sd, t, 0 );
    write_csv_double( csv, o->value*mult, ndp );
    skip_csv_fields( csv, nskip1 );
    write_csv_double( csv, o->error*mult*semult, ndp );
    skip_csv_fields( csv, nskip1+nskip2 );
    write_csv_double( csv, o->residual*mult, ndp );
    skip_csv_fields( csv, nskip1 );
    write_csv_double( csv, o->reserr*mult*semult, ndp );
    skip_csv_fields( csv, nskip1+nskip2 );
    sres = o->sres;
    if( sres >= 0.0 && semult > 0.0 ) sres /= semult;
    write_csv_double( csv,sres, 3 );
    skip_csv_fields( csv, nskip1 );
    if( o->error > 0 && ! t->unused )
    {
        write_csv_double( csv, o->reserr/o->error, 3 );
    }
    else
    {
        skip_csv_fields( csv, 1 );
    }
    skip_csv_fields( csv, nskip1 );
    write_observation_csv_common_end( csv, sd, t );
}

void write_pntdata_csv( output_csv *csv, survdata *sd, pntdata *p, double semult )
{
    trgtdata *t = &(p->tgt);
    int ndp = obs_precision[t->type];
    double mult = 1.0;
    double sres;
    int nskip1=0, nskip2=0;

    if( output_csv_vecinline )
    {
        nskip1 = (output_csv_vecsum ? 3 : 2);
        if( output_csv_correlations ) nskip2 = 3;
    }

    if( datatype[t->type].isangle ) { ndp+=4; mult=RTOD; }

    write_observation_csv_common_start( csv, sd, t, 0 );
    write_csv_double( csv, p->value*mult, ndp );
    skip_csv_fields( csv, nskip1 );
    write_csv_double( csv, p->error*mult*semult, ndp );
    skip_csv_fields( csv, nskip1+nskip2 );
    write_csv_double( csv, p->residual*mult, ndp );
    skip_csv_fields( csv, nskip1 );
    write_csv_double( csv, p->reserr*mult*semult, ndp );
    skip_csv_fields( csv, nskip1+nskip2 );
    sres = p->sres;
    if( sres >= 0.0 && semult > 0.0 ) sres /= semult;
    write_csv_double( csv,sres, 3 );
    skip_csv_fields( csv, nskip1 );
    if( p->error > 0 && ! t->unused )
    {
        write_csv_double( csv, p->reserr/p->error, 3 );
    }
    else
    {
        skip_csv_fields( csv, 1 );
    }
    skip_csv_fields( csv, nskip1 );
    write_observation_csv_common_end( csv, sd, t );
}

static void convert_cvr_to_secorr( double cvr[6] )
{
    cvr[0] = cvr[0] > 0.0 ? sqrt(cvr[0]) : 0.0;
    cvr[2] = cvr[2] > 0.0 ? sqrt(cvr[2]) : 0.0;
    cvr[5] = cvr[5] > 0.0 ? sqrt(cvr[5]) : 0.0;
    if( cvr[0] > 0 ) { cvr[1] /= cvr[0]; cvr[3] /= cvr[0]; }
    if( cvr[2] > 0 ) { cvr[1] /= cvr[2]; cvr[4] /= cvr[2]; }
    if( cvr[5] > 0 ) { cvr[3] /= cvr[5]; cvr[4] /= cvr[5]; }
}

void write_vecdata_csv_components( output_csv *csv, survdata *sd, int iobs, double semult )
{
    double sres;
    double vec[3],veccvr[6],res[3],rescvr[6];
    vecdata *vd = &(sd->obs.vdata[iobs]);
    trgtdata *t = &(vd->tgt);
    int ndp = obs_precision[t->type];
    const char *xyzcomp[3] = {"X","Y","Z"};
    const char *topocomp[3] = {"X-E","Y-N","Z-U"};
    const char **comp = output_csv_vecenu ? topocomp : xyzcomp;
    int topo = output_csv_vecenu ? VD_TOPOCENTRIC : 0;

    calc_vecdata_vector(sd,VD_REF_STN,iobs,VD_OBSVEC,vec, 0);
    calc_vecdata_vector(sd,VD_REF_STN,iobs,VD_OBSVEC | topo,0,veccvr);
    calc_vecdata_vector(sd,VD_REF_STN,iobs,VD_RESVEC | topo,res,rescvr);
    convert_cvr_to_secorr( veccvr );
    convert_cvr_to_secorr( rescvr );

    if( output_csv_veccomp )
    {
        int dim;
        int cvridx[3] = {0,2,5};
        for( dim = 0; dim < 3; dim++ )
        {
            write_observation_csv_common_start( csv, sd, t, comp[dim] );
            write_csv_double( csv, vec[dim], ndp );
            write_csv_double( csv, veccvr[cvridx[dim]]*semult, ndp+2 );
            write_csv_double( csv, res[dim], ndp );
            write_csv_double( csv, rescvr[cvridx[dim]]*semult, ndp+2 );
            sres = rescvr[cvridx[dim]];
            if( sres <= 0.0 ) sres = 1.0;
            sres = res[dim] / sres;
            if( sres >= 0.0 && semult > 0.0 ) sres /= semult;
            write_csv_double( csv,sres, 3 );
            if( veccvr[cvridx[dim]] > 0 && ! t->unused )
            {
                write_csv_double( csv, rescvr[cvridx[dim]]/veccvr[cvridx[dim]], 3 );
            }
            else
            {
                skip_csv_fields( csv, 1 );
            }
            write_observation_csv_common_end( csv, sd, t );
        }
    }
    if( output_csv_vecsum )
    {
        double length = 0.0;
        if( ! datatype[t->type].ispoint )
            length = sqrt(vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2]);
        write_observation_csv_common_start( csv, sd, t, 0 );
        write_csv_double( csv, length, ndp );
        skip_csv_fields( csv, 1 );
        length = sqrt( res[0]*res[0]+res[1]*res[1]+res[2]*res[2] );
        write_csv_double( csv, length, ndp );
        skip_csv_fields( csv, 1 );

        sres = vd->vsres;
        if( sres >= 0.0 && semult > 0.0 ) sres /= semult;
        write_csv_double( csv,sres, 3 );
        skip_csv_fields( csv, 1 );
        write_observation_csv_common_end( csv, sd, t );
    }
}

void write_vecdata_csv_inline( output_csv *csv, survdata *sd, int iobs, double semult )
{
    double sres;
    double vec[3],veccvr[6],res[3],rescvr[6];
    vecdata *vd = &(sd->obs.vdata[iobs]);
    trgtdata *t = &(vd->tgt);
    int ndp = obs_precision[t->type];
    int cvridx[3] = {0,2,5};
    int topo = output_csv_vecenu ? VD_TOPOCENTRIC : 0;
    int dim;


    calc_vecdata_vector(sd,VD_REF_STN,iobs,VD_OBSVEC,vec, 0);
    calc_vecdata_vector(sd,VD_REF_STN,iobs,VD_OBSVEC | topo,0,veccvr);
    calc_vecdata_vector(sd,VD_REF_STN,iobs,VD_RESVEC | topo,res,rescvr);
    convert_cvr_to_secorr( veccvr );
    convert_cvr_to_secorr( rescvr );

    write_observation_csv_common_start( csv, sd, t, 0 );
    if( output_csv_vecsum )
    {
        double length = sqrt(vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2]);
        write_csv_double(csv, length, ndp);
    }
    write_csv_double( csv, vec[0], ndp );
    write_csv_double( csv, vec[1], ndp );
    write_csv_double( csv, vec[2], ndp );
    if( output_csv_vecsum ) skip_csv_fields(csv,1);

    write_csv_double( csv, veccvr[0]*semult, ndp+2 );
    write_csv_double( csv, veccvr[2]*semult, ndp+2 );
    write_csv_double( csv, veccvr[5]*semult, ndp+2 );
    if( output_csv_correlations )
    {
        write_csv_double( csv, veccvr[1], 4 );
        write_csv_double( csv, veccvr[3], 4 );
        write_csv_double( csv, veccvr[4], 4 );
    }
    if( output_csv_vecsum )
    {
        double length = sqrt(res[0]*res[0]+res[1]*res[1]+res[2]*res[2]);
        write_csv_double(csv,length,ndp);
    }
    write_csv_double( csv, res[0], ndp );
    write_csv_double( csv, res[1], ndp );
    write_csv_double( csv, res[2], ndp );
    if( output_csv_vecsum ) skip_csv_fields(csv,1);
    write_csv_double( csv, rescvr[0]*semult, ndp+2 );
    write_csv_double( csv, rescvr[2]*semult, ndp+2 );
    write_csv_double( csv, rescvr[5]*semult, ndp+2 );
    if( output_csv_correlations )
    {
        write_csv_double( csv, rescvr[1], 4 );
        write_csv_double( csv, rescvr[3], 4 );
        write_csv_double( csv, rescvr[4], 4 );
    }
    if( output_csv_vecsum )
    {
        sres = vd->vsres;
        if( sres >= 0.0 && semult > 0.0 ) sres /= semult;
        write_csv_double( csv,sres, 3 );
    }
    for( dim = 0; dim < 3; dim++ )
    {

        sres = rescvr[cvridx[dim]];
        if( sres <= 0.0 ) sres = 1.0;
        sres = res[dim] / sres;
        if( sres >= 0.0 && semult > 0.0 ) sres /= semult;
        write_csv_double( csv,sres, 3 );
    }
    if( output_csv_vecsum ) skip_csv_fields(csv,1);
    for( dim = 0; dim < 3; dim++ )
    {
        if( veccvr[cvridx[dim]] > 0 && ! t->unused )
        {
            write_csv_double( csv, rescvr[cvridx[dim]]/veccvr[cvridx[dim]], 3 );
        }
        else
        {
            skip_csv_fields( csv, 1 );
        }
    }
    write_observation_csv_common_end( csv, sd, t );


}

void write_observation_csv()
{
    bindata *b;
    double semult;
    long nbin;
    survdata *sd;
    output_csv *csv;
    int i, iobs;

    /* Allocate space for the least squares results */

    if( ! got_vector_data() ) output_csv_veccomp = 0;
    if( ! output_csv_veccomp )
    {
        output_csv_vecinline = 0;
        output_csv_vecsum = 1;
    }

    b = create_bindata();

    csv = open_snap_output_csv( "obs" );
    if( ! csv ) return;

    write_csv_header(csv,"obsid");
    if( have_obs_ids ) write_csv_header(csv,"srcid");
    write_csv_header(csv,"fromstn");
    write_csv_header(csv,"tostn");
    write_csv_header(csv,"date");
    write_csv_header(csv,"fromhgt");
    write_csv_header(csv,"tohgt");
    write_csv_header(csv,"obstype");
    write_csv_header(csv,"obsset");
    write_csv_header(csv,"length");
    write_csv_header(csv,"status");
    write_csv_header(csv,"errfct");
    if( output_csv_vecinline )
    {
        if( output_csv_vecsum ) write_csv_header( csv,"value");
        write_csv_header( csv,"value1");
        write_csv_header( csv,"value2");
        write_csv_header( csv,"value3");
        if( output_csv_vecsum ) write_csv_header( csv,"error");
        write_csv_header( csv,"error1");
        write_csv_header( csv,"error2");
        write_csv_header( csv,"error3");
        if( output_csv_correlations )
        {
            write_csv_header( csv,"corr12");
            write_csv_header( csv,"corr13");
            write_csv_header( csv,"corr23");
        }
        if( output_csv_vecsum ) write_csv_header( csv,"residual");
        write_csv_header(csv,"residual1");
        write_csv_header(csv,"residual2");
        write_csv_header(csv,"residual3");
        if( output_csv_vecsum ) write_csv_header( csv,"reserror");
        write_csv_header(csv,"reserror1");
        write_csv_header(csv,"reserror2");
        write_csv_header(csv,"reserror3");
        if( output_csv_correlations )
        {
            write_csv_header( csv,"rescorr12");
            write_csv_header( csv,"rescorr13");
            write_csv_header( csv,"rescorr23");
        }
        if( output_csv_vecsum ) write_csv_header( csv,"stdres");
        write_csv_header(csv,"stdres1");
        write_csv_header(csv,"stdres2");
        write_csv_header(csv,"stdres3");
        if( output_csv_vecsum ) write_csv_header( csv,"redundancy");
        write_csv_header(csv,"redundancy1");
        write_csv_header(csv,"redundancy2");
        write_csv_header(csv,"redundancy3");
    }
    else
    {
        write_csv_header( csv,"value");
        write_csv_header( csv,"error");
        write_csv_header( csv,"residual");
        write_csv_header( csv,"reserror");
        write_csv_header( csv,"stdres");
        write_csv_header( csv,"redundancy");
    }
    /* write_csv_header(csv,"flags"); */

    for( i = 0; i < classification_count(&obs_classes); i++ )
    {
        char fieldname[33];
        strcpy(fieldname,"c_");
        strncpy(fieldname+2,classification_name(&obs_classes,i+1),30);
        fieldname[32] = 0;
        write_csv_header(csv,fieldname);
    }

    write_csv_header(csv,"sourcefile");
    write_csv_header(csv,"sourcelineno");
    if( output_csv_shape ) write_csv_header(csv,"shape");
    end_output_csv_record(csv);

    semult = apriori ? 1.0 : seu;

    /* The residuals will be listed either in the input order, or if
       sort_obs is not zero, to the order defined by get_sorted_obs_loc */

    nbin = 0;
    init_progress_meter( nbindata );

    init_get_bindata( 0L );
    for(;;)
    {
        nbin++;
        update_progress_meter( nbin );

        if( get_bindata( b ) != OK ) break;

        if ( b->bintype == NOTEDATA ) continue;
        if ( b->bintype != SURVDATA )
        {
            program_error("Invalid binary data type","write_observation_csv");
            continue;
        }

        sd = (survdata *) b->data;
        /* Set obsset to -1 so that it gets reset on first call to write_csv_common_start */
        obsset = -1;
        for( iobs = 0; iobs < sd->nobs; iobs++ )
        {
            switch( sd->format )
            {
            case SD_OBSDATA: write_obsdata_csv( csv, sd, &sd->obs.odata[iobs], semult ); break;

            case SD_VECDATA:
                if( output_csv_vecinline ) write_vecdata_csv_inline( csv, sd, iobs, semult );
                else write_vecdata_csv_components( csv, sd, iobs, semult );
                break;

            case SD_PNTDATA: write_pntdata_csv( csv, sd, &sd->obs.pdata[iobs], semult ); break;

            default: program_error("Invalid survdata format","write_observation_csv");
            }
        }
    }

    end_progress_meter();

    close_output_csv(csv);

    delete_bindata( b );
}
