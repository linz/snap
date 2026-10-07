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
#include <array>
#include <forward_list>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/numeric/conversion/cast.hpp>
using boost::numeric_cast;

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
#include "util/textformat.hpp"

/* Definition of output fields that may be put in a residual listing file */

#define LEFT_JUST 1
#define RIGHT_JUST 2

struct listing_field_def
{
    listing_field_def( const int id, const std::string_view code,
                       const std::optional<std::string_view> title1,
                       const std::optional<std::string_view> title2,
                       const int width, const char justify, const bool vector_title2 )
        : id( id ), code( code ), title1( title1 ), title2( title2 ),
          width( width ), justify( justify ), vector_title2( vector_title2 )
    {
    }

    const int id;
    const std::string code;
    const std::optional<std::string_view> title1;  /* Default titles - title2 used for vector formats only */
    std::optional<std::string_view> title2;  /* Not const: OF_RES's is set when the default format is set up */
    std::string value;  /* Text printed for this field on the current line, empty if there is none */
    int width;
    const char justify;
    const bool vector_title2;  /* title2 is for vector formats only */
    int requested = 0;
};

static listing_field_def fields[] =
{
    listing_field_def( OF_FROM,       "from",         "From",       std::nullopt, 0, LEFT_JUST, false ),
    listing_field_def( OF_TO,         "to",           "To",         std::nullopt, 0, LEFT_JUST, false ),
    listing_field_def( OF_FROMNAME,   "from_name",    "From",       std::nullopt, 20, LEFT_JUST, false ),
    listing_field_def( OF_TONAME,     "to_name",      "To",         std::nullopt, 20, LEFT_JUST, false ),
    listing_field_def( OF_HI,         "hgt_inst",     "H.I.",       std::nullopt, 6, 0, false ),
    listing_field_def( OF_HT,         "hgt_trgt",     "H.T.",       std::nullopt, 6, 0, false ),
    listing_field_def( OF_TYPE,       "type",         "Type",       std::nullopt, 4, LEFT_JUST, false ),
    listing_field_def( OF_FILENAME,   "file",         "File",       std::nullopt, 20, LEFT_JUST, false ),
    listing_field_def( OF_FILENO,     "file_no",      "Fl",         std::nullopt, 2, 0, false ),
    listing_field_def( OF_LINENO,     "line_no",      "Lin",        std::nullopt, 3, 0, false ),
    listing_field_def( OF_OBS,        "obs_val",      "Value",      "X,Y,Z", 0, 0, true ),
    listing_field_def( OF_OBSERR,     "obs_err",      "+/- ",       std::nullopt, 0, 0, false ),
    listing_field_def( OF_CALC,       "calc_val",     "Calc",       "X,Y,Z", 0, 0, true ),
    listing_field_def( OF_CALCERR,    "calc_err",     "+/- ",       std::nullopt, 0, 0, false ),
    listing_field_def( OF_RES,        "res_val",      "Res",        std::nullopt, 0, 0, true ),
    listing_field_def( OF_RESERR,     "res_err",      "+/- ",       std::nullopt, 0, 0, false ),
    listing_field_def( OF_ALTRES,     "alt_res",      "Res*",       std::nullopt, 0, 0, false ),
    listing_field_def( OF_SRES,       "std_res",      "S.R.",       std::nullopt, 6, 0, false ),
    listing_field_def( OF_REDUNDANCY, "redundancy",   "Rdncy",      std::nullopt, 6, 0, false ),
    listing_field_def( OF_FLAGS,      "flags",        std::nullopt, std::nullopt, 4, LEFT_JUST, false ),
    listing_field_def( OF_AZIMUTH,    "azimuth",      "Azimuth",    std::nullopt, 0, 0, false ),
    listing_field_def( OF_PRJAZ,      "prj_azimuth",  "Projection", "Azimuth", 0, 0, false ),
    listing_field_def( OF_HGTDIFF,    "hgt_diff",     "Hgt dif",    std::nullopt, 0, 0, false ),
    listing_field_def( OF_ARCDST,     "arc_dist",     "Arc dst",    std::nullopt, 0, 0, false ),
    listing_field_def( OF_SLPDST,     "slp_dist",     "Slp dst",    std::nullopt, 0, 0, false ),
    listing_field_def( OF_MDE,        "mde",          "MDE",        std::nullopt, 6, 0, false ),
    listing_field_def( OF_SIG,        "significance", "sig(%)",     std::nullopt, 8, 0, false ),
    listing_field_def( OF_DATE,       "date",         "Date",       std::nullopt, 10, 0, false ),
    listing_field_def( OF_OBSID,      "id",           "Id",         std::nullopt, 8, RIGHT_JUST, false ),
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

struct listing_column
{
    int column;
    int width;
    std::optional<std::string_view> title1;
    std::optional<std::string_view> title2;
    std::string data;  /* Classification value printed for this column on the current line */
};

struct listing_def
{
    int ncolumn;
    listing_column col[MAX_COLUMNS];
};

static listing_def *listing_format = NULL;
static listing_def data_format[NOBSTYPE] = {0};
static int listing_title = -1;
static int title_id[NOBSTYPE] = {0};
static int defining_format[NOBSTYPE] = {0};
// Interned column headings - get_column_heading() below returns a stable
// pointer into this list, safe to keep past this call (forward_list never
// relocates existing elements on push_front).
static std::forward_list<std::string> headings;


static int maxrow, maxlt, last_file_loc;

static void program_error( const char *msg, const char *routine )
{
    char msg1[150];
    char msg2[100];
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
            fprintf(out,"   %-2s  %ss\n",datatype[type].code.data(),datatype[type].name.data());
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
            survey_data_file_name(sd->file).c_str(),(int)(tgt->lineno));
    if( sd->from ) { fprintf(lst,"%s ",stnptr(sd->from)->Code.c_str() ); }
    if( tgt->to ) { fprintf( lst, "%s%s ",(sd->from ? "to " : ""),stnptr(tgt->to)->Code.c_str());}
    fprintf(lst,": %s",datatype[tgt->type].code.data());
    if( ntgt > 1 ) fprintf(lst," ...");
    fprintf(lst,"\n\n");
}
*/


int sum_bindata( int iteration )
{
    const std::string header = "obs_equation_" + std::to_string(iteration);
    void *hA;
    bindata *b;
    int nrow;
    long nbin;
    int sts=OK;

    if( output_observation_equations )
    {
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
    while( get_bindata( SURVDATA, b ) == OK )
    {
        nbin++;
        update_progress_meter( nbin );
        int stsobs=bindata_obseq( b, hA );
        if( stsobs != OK ) 
        {
            sts=stsobs;
            continue;
        }
        if( output_observation_equations )
        {
            const survdata *sd = static_cast<survdata *>(b->data);
            const trgtdata *tgt=get_trgtdata(sd,0);
            std::ostringstream source;
            source << "{\"file\": \"" << survey_data_file_name(sd->file).substr(0,80)
                   << "\",\"lineno\": " << tgt->lineno
                   << ", \"station\": \"" << (sd->from ? stnptr(sd->from)->Code.c_str() : "")
                   << (sd->from && tgt->to ? " - " : "")
                   << (tgt->to ? stnptr(tgt->to)->Code.c_str() : "")
                   << "\", \"obsid\": " << tgt->obsid
                   << ", \"type\": \"" << datatype[tgt->type].code
                   << "\",\"nobs\": " << sd->nobs << "}";
            if( nbin > 1 )  fprintf(lst,",\n");
            print_obseqn_json( lst, hA, source.str(), 0 );
        }
        stsobs=lsq_sum_obseqn( hA );
        if( stsobs != OK )
        {
            char location[200];
            survdata *sd = (survdata *) b->data;
            trgtdata *tgt=get_trgtdata(sd,0);
            sprintf(location,"Cannot sum observation from %.80s line %d\n",
                    survey_data_file_name(sd->file).c_str(),
                    (int)(tgt->lineno)
                   );
            handle_error(INVALID_DATA,"Observation error",location);
            sts=stsobs;
        }
        nrow = obseqn_rows( hA );
        if( nrow > maxrow ) maxrow = nrow;
        if( !obseqn_cvr_diagonal(hA) && nrow > maxlt ) maxlt = nrow;
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

        if( get_bindata( SURVDATA, b ) != OK ) break;

        nbin++;
        update_progress_meter( nbin );


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
    constexpr std::string_view blank = "";
    constexpr std::string_view lftjst = "%-*s";
    constexpr std::string_view centrejst = "%-*s%-*s";
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
            std::optional<std::string_view> titleOpt = (ipass == 0) ? column->title1 : column->title2;
            if( column->title2 ) needpass2 = 1;
            std::string_view source = titleOpt.value_or(blank);
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
                fprintf(out,lftjst.data(),width,source.data());
            }
            else
            {
                int len1, len2;
                len1 = width - source.size();
                if( justify != RIGHT_JUST ) len1 /= 2;
                if( len1 < 0 ) len1 = 0;
                len2 = width - len1;
                fprintf(out,centrejst.data(),len1,blank.data(),len2,source.data());
            }
        }
        fprintf(out,"\n");
        if( !needpass2 ) break;
    }
    clear_residual_fields();
}

static void setup_default_format( int type );

int define_residual_formats( const std::string_view typelist, const int add_columns )
{
    int status = MISSING_DATA;
    int itype;

    for( itype = 0; itype < NOBSTYPE; itype++ ) defining_format[itype] = 0;

    std::string_view remaining = typelist;
    while( ! remaining.empty() )
    {
        const std::size_t slash = remaining.find( '/' );
        const std::string_view type = remaining.substr( 0, slash );
        remaining = slash == std::string_view::npos ? std::string_view() : remaining.substr( slash + 1 );
        if( boost::algorithm::iequals( type, "ALL" ) )
        {
            for( itype = NOBSTYPE; itype--; )
            {
                defining_format[itype] = 1;
            }
            status = OK;
        }
        else if( boost::algorithm::iequals( type, "POINT" ) )
        {
            for( itype = NOBSTYPE; itype--; ) if( datatype[itype].ispoint )
                {
                    defining_format[itype] = 1;
                }
            status = OK;
        }
        else if( boost::algorithm::iequals( type, "VECTOR" ) )
        {
            for( itype = NOBSTYPE; itype--; ) if( datatype[itype].isvector )
                {
                    defining_format[itype] = 1;
                }
            status = OK;
        }
        else if( boost::algorithm::iequals( type, "LINE" ) )
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
                if( boost::algorithm::iequals( datatype[itype].code, type ) )
                {
                    defining_format[itype] = 1;
                    status = OK;
                    break;
                }
            }
            if( itype < 0 ) status = INVALID_DATA;
        }
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

// Interns text - a title supplied by a caller whose own storage may not
// outlive this call (a config-parsed title, a computed classification
// name) - copying it into headings, which lives for the rest of the
// program, and returning a stable view of that copy instead. fields[]'s own
// title1/title2 entries never need this: they're always either a string
// literal (program-lifetime already) or absent.
static std::optional<std::string_view> get_column_heading( std::optional<std::string_view> text )
{
    if( !text ) return std::nullopt;
    for( const std::string &heading : headings )
    {
        if( heading == *text ) return heading;
    }
    headings.push_front( std::string(*text) );
    return headings.front();
}

void clear_residual_field_defs()
{
    listing_format->ncolumn = 0;
}

static int add_residual_field_def( int type, std::string_view code, int width,
                                    std::optional<std::string_view> title1,
                                    std::optional<std::string_view> title2 )
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
        if( boost::algorithm::iequals( code, fields[i].code ) )
        {
            column = i;
            if( !title1 && !title2 )
            {
                title1 = fields[i].title1;
                title2 = (!fields[i].vector_title2 || datatype[type].isvector) ?
                         fields[i].title2 : std::nullopt;
            }
            break;
        }
    }

    if( column == INVALID_FIELD && boost::algorithm::iequals(code,"S") )
    {
        column = SPACE_FIELD;
    }

    if( column == INVALID_FIELD && boost::algorithm::iequals(code,"NL") )
    {
        column = NEWLINE_FIELD;
    }

    if( column == INVALID_FIELD && boost::algorithm::istarts_with(code,"C=") )
    {
        column = obs_classes.id( code.substr(2), 1 );
        if( !title1 && !title2 )
        {
            title1 = get_column_heading( obs_classes.name( column ) );
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

int add_residual_field( std::string_view code, int width,
                         std::optional<std::string_view> title1,
                         std::optional<std::string_view> title2 )
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
            int ttlen;
            for( jtype = itype+1; jtype < NOBSTYPE; jtype++ )
            {
                if( title_id[jtype] != itype ) continue;
                if( data_format[jtype].col[icol].width > maxwidth )
                    maxwidth = data_format[jtype].col[icol].width;
            }
            ttlen = idef->col[icol].title1 ? idef->col[icol].title1->size() : 0;
            if( ttlen > maxwidth ) maxwidth = ttlen;
            ttlen = idef->col[icol].title2 ? idef->col[icol].title2->size() : 0;
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
    constexpr std::string_view lftjst = "%-*s";
    constexpr std::string_view rgtjst = "%*s";
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
            fprintf(out,"%-*s",col->width,col->data.c_str() );
            need_space = 1;
        }
        else
        {
            const listing_field_def &fld = fields[col->column];
            if( fld.id == OF_OBSID && ! have_obs_ids ) continue;
            if( need_space ) fputc(' ',out);
            fprintf(out,(fld.justify == LEFT_JUST ? lftjst : rgtjst).data(), col->width, fld.value.c_str() );
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
        fields[i].value.clear();
    }
}

void set_residual_field( const int field_id, const std::string_view value )
{
    fields[field_id].value = value;
}

void clear_residual_field( const int field_id )
{
    fields[field_id].value.clear();
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
        static std::optional<DmsFormat> azimuthFormat;
        static char isproj;
        if( !azimuthFormat )
        {
            isproj = is_projection(net->crdsys);
            azimuthFormat.emplace( 3, obs_precision[AZ] );
        }
        if( WANT(OF_AZIMUTH) )
        {
            value = calc_azimuth( stf, 0.0, stt, 0.0, 0, NULL, NULL );
            while( value > TWOPI ) value -= TWOPI;
            while( value < 0.0 ) value += TWOPI;
            set_residual_field_dms( OF_AZIMUTH, *azimuthFormat, value*RTOD );
        }
        if( WANT( OF_PRJAZ ) && isproj)
        {
            value = calc_prj_azimuth( net, stf, 0.0, stt, 0.0, NULL, NULL );
            while( value > TWOPI ) value -= TWOPI;
            while( value < 0.0 ) value += TWOPI;
            set_residual_field_dms( OF_PRJAZ, *azimuthFormat, value*RTOD );
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
    /* An undefined date leaves the field blank */
    if( WANT(OF_DATE) && sd->date != UNDEFINED_DATE )
    {
        int dy,mn,yr;
        date_as_ymd(sd->date,&yr,&mn,&dy);
        std::ostringstream text;
        text << std::setw(2) << dy << std::setfill('0') << '/' << std::setw(2) << mn << '/' << std::setw(4) << yr;
        set_residual_field( OF_DATE, text.str() );
    }
}

void set_residual_field_value( const int id, const int ndp, const double value )
{
    const std::string fixedText = format_fixed( value, ndp );
    set_residual_field( id, fixedText.size() < 20 ? fixedText : format_scientific( value, 8 ) );
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
    set_residual_field( OF_FILENO, std::to_string( sd->file ) );
    set_residual_field( OF_FILENAME, survey_data_file_name( sd->file ) );
    set_residual_field( OF_LINENO, std::to_string( t->lineno ) );
    set_residual_field( OF_OBSID, std::to_string( t->id ) );

    for( i = 0; i < listing_format->ncolumn; i++ )
    {
        if( listing_format->col[i].column & CLASSIFICATION_FIELD )
        {
            int class_id;
            class_id = listing_format->col[i].column & ~CLASSIFICATION_FIELD;
            auto name = get_obs_classification_name( sd, t, class_id );
            listing_format->col[i].data = name ? std::move(*name) : std::string();
        }
    }
}

void set_residual_type_field( const std::string_view code, const char flag )
{
    std::ostringstream text;
    text << std::setw( 2 ) << code << flag;
    set_residual_field( OF_TYPE, text.str() );
}

void set_residual_field_dms( const int id, const DmsFormat &format, const double value )
{
    set_residual_field( id, dms_string( value, format ) );
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
            class_count = obs_classes.value_count( class_id );
            width = 0;
            for( ic = 0; ic < class_count; ic++ )
            {
                len = numeric_cast<int>( obs_classes.value_name( class_id, ic ).size() );
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

    fields[OF_RES].title2 = output_xyz_vector_residuals ? "X,Y,Z" : "E,N,U";

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
        add_residual_field_def( type, fields[fld].code, width, std::nullopt, std::nullopt );
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
        fprintf(out,"significance %6.3lf%% and power %6.1lf%%\n",prob,mde_power);
        set_mde_level( prob, mde_power );
    }


    if( got_vector_data() && ! output_xyz_vector_residuals )
    {
        if( gps_vertical_fixed() )
        {
            double topolat, topolon;
            const DmsFormat latitudeFormat( 3, 5, 0, std::nullopt, std::nullopt, std::nullopt, "N", "S" );
            const DmsFormat longitudeFormat( 3, 5, 0, std::nullopt, std::nullopt, std::nullopt, "E", "W" );
            get_network_topocentre( net, &topolat, &topolon );
            fprintf(out,"\nVector residual east, north, up directions are calculated at\n   ");
            fputs( dms_string( topolat* RTOD, latitudeFormat ).c_str(), out );
            fputs( "    ", out );
            fputs( dms_string( topolon* RTOD, longitudeFormat ).c_str(), out );
            fputs( "\n", out );
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
            const int64_t loc = get_sorted_obs_loc();
            if( loc < 0 ) break;
            init_get_bindata( loc );
        }

        if( get_bindata( SURVDATA, b ) != OK ) break;

        nbin++;
        update_progress_meter( nbin );

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
    fprintf(out,"\nFile %s: line %d\n",survey_data_file_name(file).c_str(),(int)lineno);
    nwait = file_location_frequency;
}


static int obsset = -1;

static void write_observation_csv_common_start( output_csv &csv, survdata *sd, trgtdata *tgt, const std::string_view component )
{
    station *from = stnptr(sd->from);
    station *to = stnptr(tgt->to);
    if( ! from ) { from = to; to = nullptr; }
    if( obsset < 0 ) obsset=tgt->obsid;
    std::string type( datatype[tgt->type].code );
    if( ! component.empty() && type.size()+component.size()+2 < 16 ) { type += '-'; type += component; }
    csv.writeInt( tgt->obsid );
    if( have_obs_ids ) csv.writeInt( tgt->id );
    csv.writeString(from->Code);
    if( to ) csv.writeString(to->Code);
    else csv.writeNullField();
    csv.writeDate(sd->date);
    csv.writeDouble(sd->fromhgt,3);
    csv.writeDouble(tgt->tohgt,3);
    csv.writeString(type);
    csv.writeInt(obsset);
    if( to ) csv.writeDouble(calc_distance( from, 0.0, to, 0.0, nullptr, nullptr ),3);
    else csv.writeNullField();
    csv.writeString(tgt->unused ? "rej" : "use" );
    csv.writeDouble(tgt->errfct,3);
}

static void write_observation_csv_common_end( output_csv &csv, survdata *sd, trgtdata *tgt )
{
    station *from = stnptr(sd->from);
    station *to = stnptr(tgt->to);
    if( ! from ) { from = to; to = nullptr; }

    for( int i = 0; i < obs_classes.count(); i++ )
    {
        const auto name = get_obs_classification_name(sd,tgt,i+1);
        if( name ) csv.writeString(*name);
        else csv.writeNullField();
    }
    csv.writeString(survey_data_file_name(sd->file));
    csv.writeInt(tgt->lineno);

    if( output_csv_shape )
    {
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
        std::ostringstream wkt;
        wkt << std::fixed << std::setprecision(ndp);
        if( to )
        {
            wkt << "LINESTRING(" << ef << " " << nf << ", " << et << " " << nt << ")";
        }
        else
        {
            wkt << "POINT(" << ef << " " << nf << ")";
        }
        csv.writeString(wkt.str());
    }

    csv.endRecord();
}

//&output_csv_veccomp
//&output_csv_vecsum
//&output_csv_vecinline
//&output_csv_vecenu
//&output_csv_correlations

void write_obsdata_csv( output_csv &csv, survdata *sd, obsdata *o, double semult )
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

    write_observation_csv_common_start( csv, sd, t, {} );
    csv.writeDouble( o->value*mult, ndp );
    csv.writeNullFields( nskip1 );
    csv.writeDouble( o->error*mult*semult, ndp );
    csv.writeNullFields( nskip1+nskip2 );
    csv.writeDouble( o->residual*mult, ndp );
    csv.writeNullFields( nskip1 );
    csv.writeDouble( o->reserr*mult*semult, ndp );
    csv.writeNullFields( nskip1+nskip2 );
    sres = o->sres;
    if( sres >= 0.0 && semult > 0.0 ) sres /= semult;
    csv.writeDouble( sres, 3 );
    csv.writeNullFields( nskip1 );
    if( o->error > 0 && ! t->unused )
    {
        csv.writeDouble( o->reserr/o->error, 3 );
    }
    else
    {
        csv.writeNullField();
    }
    csv.writeNullFields( nskip1 );
    write_observation_csv_common_end( csv, sd, t );
}

void write_pntdata_csv( output_csv &csv, survdata *sd, pntdata *p, double semult )
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

    write_observation_csv_common_start( csv, sd, t, {} );
    csv.writeDouble( p->value*mult, ndp );
    csv.writeNullFields( nskip1 );
    csv.writeDouble( p->error*mult*semult, ndp );
    csv.writeNullFields( nskip1+nskip2 );
    csv.writeDouble( p->residual*mult, ndp );
    csv.writeNullFields( nskip1 );
    csv.writeDouble( p->reserr*mult*semult, ndp );
    csv.writeNullFields( nskip1+nskip2 );
    sres = p->sres;
    if( sres >= 0.0 && semult > 0.0 ) sres /= semult;
    csv.writeDouble( sres, 3 );
    csv.writeNullFields( nskip1 );
    if( p->error > 0 && ! t->unused )
    {
        csv.writeDouble( p->reserr/p->error, 3 );
    }
    else
    {
        csv.writeNullField();
    }
    csv.writeNullFields( nskip1 );
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

void write_vecdata_csv_components( output_csv &csv, survdata *sd, int iobs, double semult )
{
    double sres;
    double vec[3],veccvr[6],res[3],rescvr[6];
    vecdata *vd = &(sd->obs.vdata[iobs]);
    trgtdata *t = &(vd->tgt);
    int ndp = obs_precision[t->type];
    constexpr std::array<std::string_view,3> xyzcomp = {"X","Y","Z"};
    constexpr std::array<std::string_view,3> topocomp = {"X-E","Y-N","Z-U"};
    const std::array<std::string_view,3> *comp = output_csv_vecenu ? &topocomp : &xyzcomp;
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
            write_observation_csv_common_start( csv, sd, t, (*comp)[dim] );
            csv.writeDouble( vec[dim], ndp );
            csv.writeDouble( veccvr[cvridx[dim]]*semult, ndp+2 );
            csv.writeDouble( res[dim], ndp );
            csv.writeDouble( rescvr[cvridx[dim]]*semult, ndp+2 );
            sres = rescvr[cvridx[dim]];
            if( sres <= 0.0 ) sres = 1.0;
            sres = res[dim] / sres;
            if( sres >= 0.0 && semult > 0.0 ) sres /= semult;
            csv.writeDouble( sres, 3 );
            if( veccvr[cvridx[dim]] > 0 && ! t->unused )
            {
                csv.writeDouble( rescvr[cvridx[dim]]/veccvr[cvridx[dim]], 3 );
            }
            else
            {
                csv.writeNullField();
            }
            write_observation_csv_common_end( csv, sd, t );
        }
    }
    if( output_csv_vecsum )
    {
        double length = 0.0;
        if( ! datatype[t->type].ispoint )
            length = sqrt(vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2]);
        write_observation_csv_common_start( csv, sd, t, {} );
        csv.writeDouble( length, ndp );
        csv.writeNullField();
        length = sqrt( res[0]*res[0]+res[1]*res[1]+res[2]*res[2] );
        csv.writeDouble( length, ndp );
        csv.writeNullField();

        sres = vd->vsres;
        if( sres >= 0.0 && semult > 0.0 ) sres /= semult;
        csv.writeDouble( sres, 3 );
        csv.writeNullField();
        write_observation_csv_common_end( csv, sd, t );
    }
}

void write_vecdata_csv_inline( output_csv &csv, survdata *sd, int iobs, double semult )
{
    double sres;
    double vec[3],veccvr[6],res[3],rescvr[6];
    vecdata *vd = &(sd->obs.vdata[iobs]);
    trgtdata *t = &(vd->tgt);
    int ndp = obs_precision[t->type];
    int cvridx[3] = {0,2,5};
    int topo = output_csv_vecenu ? VD_TOPOCENTRIC : 0;

    calc_vecdata_vector(sd,VD_REF_STN,iobs,VD_OBSVEC,vec, 0);
    calc_vecdata_vector(sd,VD_REF_STN,iobs,VD_OBSVEC | topo,0,veccvr);
    calc_vecdata_vector(sd,VD_REF_STN,iobs,VD_RESVEC | topo,res,rescvr);
    convert_cvr_to_secorr( veccvr );
    convert_cvr_to_secorr( rescvr );

    write_observation_csv_common_start( csv, sd, t, {} );
    if( output_csv_vecsum )
    {
        double length = sqrt(vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2]);
        csv.writeDouble(length, ndp);
    }
    csv.writeDouble( vec[0], ndp );
    csv.writeDouble( vec[1], ndp );
    csv.writeDouble( vec[2], ndp );
    if( output_csv_vecsum ) csv.writeNullField();

    csv.writeDouble( veccvr[0]*semult, ndp+2 );
    csv.writeDouble( veccvr[2]*semult, ndp+2 );
    csv.writeDouble( veccvr[5]*semult, ndp+2 );
    if( output_csv_correlations )
    {
        csv.writeDouble( veccvr[1], 4 );
        csv.writeDouble( veccvr[3], 4 );
        csv.writeDouble( veccvr[4], 4 );
    }
    if( output_csv_vecsum )
    {
        double length = sqrt(res[0]*res[0]+res[1]*res[1]+res[2]*res[2]);
        csv.writeDouble(length,ndp);
    }
    csv.writeDouble( res[0], ndp );
    csv.writeDouble( res[1], ndp );
    csv.writeDouble( res[2], ndp );
    if( output_csv_vecsum ) csv.writeNullField();
    csv.writeDouble( rescvr[0]*semult, ndp+2 );
    csv.writeDouble( rescvr[2]*semult, ndp+2 );
    csv.writeDouble( rescvr[5]*semult, ndp+2 );
    if( output_csv_correlations )
    {
        csv.writeDouble( rescvr[1], 4 );
        csv.writeDouble( rescvr[3], 4 );
        csv.writeDouble( rescvr[4], 4 );
    }
    if( output_csv_vecsum )
    {
        sres = vd->vsres;
        if( sres >= 0.0 && semult > 0.0 ) sres /= semult;
        csv.writeDouble( sres, 3 );
    }
    for( int dim = 0; dim < 3; dim++ )
    {

        sres = rescvr[cvridx[dim]];
        if( sres <= 0.0 ) sres = 1.0;
        sres = res[dim] / sres;
        if( sres >= 0.0 && semult > 0.0 ) sres /= semult;
        csv.writeDouble( sres, 3 );
    }
    if( output_csv_vecsum ) csv.writeNullField();
    for( int dim = 0; dim < 3; dim++ )
    {
        if( veccvr[cvridx[dim]] > 0 && ! t->unused )
        {
            csv.writeDouble( rescvr[cvridx[dim]]/veccvr[cvridx[dim]], 3 );
        }
        else
        {
            csv.writeNullField();
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

    /* Allocate space for the least squares results */

    if( ! got_vector_data() ) output_csv_veccomp = false;
    if( ! output_csv_veccomp )
    {
        output_csv_vecinline = false;
        output_csv_vecsum = true;
    }

    b = create_bindata();

    const std::unique_ptr<output_csv> csv = open_snap_output_csv( "obs" );
    if( ! csv ) return;

    csv->writeHeader("obsid");
    if( have_obs_ids ) csv->writeHeader("srcid");
    csv->writeHeader("fromstn");
    csv->writeHeader("tostn");
    csv->writeHeader("date");
    csv->writeHeader("fromhgt");
    csv->writeHeader("tohgt");
    csv->writeHeader("obstype");
    csv->writeHeader("obsset");
    csv->writeHeader("length");
    csv->writeHeader("status");
    csv->writeHeader("errfct");
    if( output_csv_vecinline )
    {
        if( output_csv_vecsum ) csv->writeHeader("value");
        csv->writeHeader("value1");
        csv->writeHeader("value2");
        csv->writeHeader("value3");
        if( output_csv_vecsum ) csv->writeHeader("error");
        csv->writeHeader("error1");
        csv->writeHeader("error2");
        csv->writeHeader("error3");
        if( output_csv_correlations )
        {
            csv->writeHeader("corr12");
            csv->writeHeader("corr13");
            csv->writeHeader("corr23");
        }
        if( output_csv_vecsum ) csv->writeHeader("residual");
        csv->writeHeader("residual1");
        csv->writeHeader("residual2");
        csv->writeHeader("residual3");
        if( output_csv_vecsum ) csv->writeHeader("reserror");
        csv->writeHeader("reserror1");
        csv->writeHeader("reserror2");
        csv->writeHeader("reserror3");
        if( output_csv_correlations )
        {
            csv->writeHeader("rescorr12");
            csv->writeHeader("rescorr13");
            csv->writeHeader("rescorr23");
        }
        if( output_csv_vecsum ) csv->writeHeader("stdres");
        csv->writeHeader("stdres1");
        csv->writeHeader("stdres2");
        csv->writeHeader("stdres3");
        if( output_csv_vecsum ) csv->writeHeader("redundancy");
        csv->writeHeader("redundancy1");
        csv->writeHeader("redundancy2");
        csv->writeHeader("redundancy3");
    }
    else
    {
        csv->writeHeader("value");
        csv->writeHeader("error");
        csv->writeHeader("residual");
        csv->writeHeader("reserror");
        csv->writeHeader("stdres");
        csv->writeHeader("redundancy");
    }
    /* csv->writeHeader("flags"); */

    for( int i = 0; i < obs_classes.count(); i++ )
    {
        csv->writeHeader( "c_" + obs_classes.name(i+1).substr(0,30) );
    }

    csv->writeHeader("sourcefile");
    csv->writeHeader("sourcelineno");
    if( output_csv_shape ) csv->writeHeader("shape");
    csv->endRecord();

    semult = apriori ? 1.0 : seu;

    /* The residuals will be listed either in the input order, or if
       sort_obs is not zero, to the order defined by get_sorted_obs_loc */

    nbin = 0;
    init_progress_meter( nbindata );

    init_get_bindata( 0L );
    for(;;)
    {
        if( get_bindata( SURVDATA, b ) != OK ) break;

        nbin++;
        update_progress_meter( nbin );

        sd = (survdata *) b->data;
        /* Set obsset to -1 so that it gets reset on first call to write_observation_csv_common_start */
        obsset = -1;
        for( int iobs = 0; iobs < sd->nobs; iobs++ )
        {
            switch( sd->format )
            {
            case SD_OBSDATA: write_obsdata_csv( *csv, sd, &sd->obs.odata[iobs], semult ); break;

            case SD_VECDATA:
                if( output_csv_vecinline ) write_vecdata_csv_inline( *csv, sd, iobs, semult );
                else write_vecdata_csv_components( *csv, sd, iobs, semult );
                break;

            case SD_PNTDATA: write_pntdata_csv( *csv, sd, &sd->obs.pdata[iobs], semult ); break;

            default: program_error("Invalid survdata format","write_observation_csv");
            }
        }
    }

    end_progress_meter();

    delete_bindata( b );
}
