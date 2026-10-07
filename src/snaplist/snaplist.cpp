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
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include "util/fieldscanner.hpp"
#include "util/snapctype.h"

#include "util/errdef.h"
#include "util/chkalloc.h"
#include "util/fileutil.h"

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
#include "util/textformat.hpp"
#include "util/getversion.h"

using boost::numeric_cast;

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
enum {TYPE_TEXT, TYPE_DOUBLE, TYPE_ANGLE };

constexpr std::size_t maximumHeaderRows = 3;

constexpr std::size_t maximumClasses = 20;

/// The current value of a column, which is text for TYPE_TEXT and a number otherwise. A
/// classification is not held with the column, so its value is the index of the
/// classification in the table, and the TableWriter listing the table holds the text.
using ColumnValue = std::variant<const std::string *, const double *, std::size_t>;

/// A value that can be listed in a table column: what it is called, how it is laid
/// out unless the table says otherwise, and where its current value is.
struct ColumnSource
{
    std::string_view name;
    int width;
    int ndp;
    int just;
    int type;
    ColumnValue value;
};

/// A column of a table as defined in the configuration file
struct TableColumn
{
    explicit TableColumn( const ColumnSource &source )
        : type( source.type ), value( source.value ),
          width( source.width ), ndp( source.ndp ), just( source.just )
    {
    }

    int type;
    ColumnValue value;
    int width;
    int ndp;
    int just;
    int quote = QT_NONE;
    std::optional<DmsFormat> format;      /* For TYPE_ANGLE */
    std::string prefix;
    std::string suffix;
    std::vector<std::string> header;      /* One entry per heading row, up to maximumHeaderRows */
    std::optional<int> extralen;          /* Width of quotes, prefix and suffix, set when first printed */
};

static std::string fromStn;
static std::string toStn;
static std::string fromStnName;
static std::string toStnName;
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

static FILE *out;
static BINARY_FILE *b;

static const std::vector<ColumnSource> obs_valid_columns =
{
    { "from",0,0,JST_LEFT,TYPE_TEXT,&fromStn },
    { "to",0,0,JST_LEFT,TYPE_TEXT,&toStn },
    { "from_name",0,0,JST_LEFT,TYPE_TEXT,&fromStnName },
    { "to_name",0,0,JST_LEFT,TYPE_TEXT,&toStnName },
    { "obs_ell_dist",0,2,JST_RIGHT,TYPE_DOUBLE,&obs_ell_dist },
    { "calc_ell_dist",0,2,JST_RIGHT,TYPE_DOUBLE,&calc_ell_dist },
    { "ell_dist_err",0,2,JST_RIGHT,TYPE_DOUBLE,&ell_dist_err },
    { "ppm_ell_dist_err",0,0,JST_RIGHT,TYPE_DOUBLE,&ppm_ell_dist_err },
    { "rf_ell_dist_err",0,0,JST_RIGHT,TYPE_DOUBLE,&rf_ell_dist_err },
    { "obs_prj_brng",0,0,JST_RIGHT,TYPE_ANGLE,&obs_prj_brng },
    { "calc_prj_brng",0,0,JST_RIGHT,TYPE_ANGLE,&calc_prj_brng },
    { "prj_brng_err",0,0,JST_RIGHT,TYPE_DOUBLE,&prj_brng_err },
    { "ppm_prj_brng_err",0,0,JST_RIGHT,TYPE_DOUBLE,&ppm_prj_brng_err },
    { "rf_prj_brng_err",0,0,JST_RIGHT,TYPE_DOUBLE,&rf_prj_brng_err },
    { "hor_vec_err",0,2,JST_RIGHT,TYPE_DOUBLE,&hor_vec_err },
    { "ppm_hor_vec_err",0,0,JST_RIGHT,TYPE_DOUBLE,&ppm_hor_vec_err },
    { "rf_hor_vec_err",0,0,JST_RIGHT,TYPE_DOUBLE,&rf_hor_vec_err },
};

static std::string stn_code;
static std::string stn_name;
static std::string stn_order;
static double stn_northing;
static double stn_easting;
static double stn_height;
static double stn_h_max_error;
static double stn_h_min_error;
static double stn_h_max_brng;
static double stn_de;
static double stn_dn;
static double stn_dh;

static const std::vector<ColumnSource> stn_valid_columns =
{
    { "code",0,0,JST_LEFT,TYPE_TEXT,&stn_code },
    { "name",0,0,JST_LEFT,TYPE_TEXT,&stn_name },
    { "order",0,0,JST_LEFT,TYPE_TEXT,&stn_order },
    { "northing",0,4,JST_RIGHT,TYPE_DOUBLE,&stn_northing },
    { "easting",0,4,JST_RIGHT,TYPE_DOUBLE,&stn_easting },
    { "latitude",0,9,JST_RIGHT,TYPE_DOUBLE,&stn_northing },
    { "longitude",0,9,JST_RIGHT,TYPE_DOUBLE,&stn_easting },
    { "height",0,4,JST_RIGHT,TYPE_DOUBLE,&stn_height },
    { "h_max_error",0,4,JST_RIGHT,TYPE_DOUBLE,&stn_h_max_error },
    { "h_min_error",0,4,JST_RIGHT,TYPE_DOUBLE,&stn_h_min_error },
    { "h_max_brng",0,3,JST_RIGHT,TYPE_DOUBLE,&stn_h_max_brng },
    { "change_east",0,4,JST_RIGHT,TYPE_DOUBLE,&stn_de },
    { "change_north",0,4,JST_RIGHT,TYPE_DOUBLE,&stn_dn },
    { "change_up",0,4,JST_RIGHT,TYPE_DOUBLE,&stn_dh },
};

/// A table as defined by a table command in the configuration file
struct TableDefinition
{
    std::optional<char> quote = '"';       /* Written around text, none if there is no character */
    std::optional<char> delimiter = ',';   /* Written between columns */
    std::optional<char> escape = '"';      /* Written before a character that cannot otherwise be written */
    bool stationData = false;              /* Lists stations rather than observations */
    const std::vector<ColumnSource> *validColumns = nullptr;  /* The values that can be listed */
    std::vector<TableColumn> columns;
    std::vector<int> classIds;             /* The classification of each class index */

    /// \return the value with the name, or nullptr if there isn't one
    const ColumnSource *validColumn( std::string_view name ) const;

    /// Adds a classification as a value that can be listed. \return nullopt if there are too many.
    std::optional<ColumnSource> classColumn( std::string_view className );
};

/// Everything the configuration file has defined so far
struct ListingConfig
{
    /// The text written between the degrees, minutes and seconds of angles, for all tables
    std::string degreeSeparator = " ";
    std::string minuteSeparator = " ";
    std::string secondSeparator;

    TableDefinition table;

    DmsFormat angleFormat( int decimalPlaces ) const;
};

/// Writes a table as delimited text, once it has been defined. The header is written
/// first, and then each row, after the values for the row have been set.
class TableWriter
{
public:
    explicit TableWriter( const TableDefinition &definition );

    bool stationData() const { return _stationData; }
    bool listsObservations() const;
    bool listsStations() const;

    std::size_t classCount() const { return _classId.size(); }
    int classId( const std::size_t index ) const { return _classId[index]; }
    void setClassValue( const std::size_t index, std::string value ) { _classValue[index] = std::move( value ); }

    void printHeader( FILE *out );
    void printRow( FILE *out );

private:
    /// What to write for a character that cannot be written as it is
    struct Replacements
    {
        std::string quotedQuote;           /* A quote in quoted text */
        std::string quotedEscape;          /* An escape in quoted text */
        std::string unquotedQuote;         /* A quote in unquoted text */
        std::string unquotedEscape;
        std::string unquotedDelimiter;
        std::string unquotedNewline;
    };

    static Replacements _replacementsFor( std::optional<char> quote, std::optional<char> escape, char delimiter );
    void _printField( TableColumn &column, std::string_view text, int quoting, FILE *out ) const;

    const bool _stationData;
    const std::vector<ColumnSource> *const _validColumns;
    const char _delimiter;
    const std::optional<char> _quote;
    const std::optional<char> _escape;
    const Replacements _replacements;
    std::vector<TableColumn> _columns;     /* Not const, as the width of quotes, prefix and suffix is set when first printed */
    const std::size_t _headerRows;
    const std::vector<int> _classId;
    std::vector<std::string> _classValue;
};


static void write_text( FILE *out, const std::string_view text )
{
    fwrite( text.data(), 1, text.size(), out );
}

static std::string character_text( const std::optional<char> character )
{
    return character ? std::string( 1, *character ) : std::string();
}

DmsFormat ListingConfig::angleFormat( const int decimalPlaces ) const
{
    return DmsFormat( 0, decimalPlaces, 0, std::string_view( degreeSeparator ),
                      std::string_view( minuteSeparator ), std::string_view( secondSeparator ) );
}

static std::size_t header_row_count( const std::vector<TableColumn> &columns )
{
    std::size_t rows = 0;
    for( const TableColumn &column : columns ) rows = std::max( rows, column.header.size() );
    return rows;
}

TableWriter::Replacements TableWriter::_replacementsFor(
    const std::optional<char> quote,
    const std::optional<char> escape,
    const char delimiter )
{
    Replacements replacements;
    const std::string replace = delimiter == ' ' ? "_" : " ";
    const std::string escapeText = character_text( escape );

    if( escape && escape != quote )
    {
        replacements.unquotedEscape = escapeText + escapeText;
        replacements.unquotedDelimiter = escapeText + delimiter;
        replacements.unquotedNewline = escapeText + "\n";
        replacements.unquotedQuote = escapeText + character_text( quote );
    }
    else
    {
        replacements.unquotedEscape = replace;
        replacements.unquotedDelimiter = replace;
        replacements.unquotedNewline = replace;
        replacements.unquotedQuote = replace;
    }

    if( quote )
    {
        if( escape )
        {
            replacements.quotedQuote = escapeText + character_text( quote );
            replacements.quotedEscape = escapeText + escapeText;
        }
        else
        {
            replacements.quotedQuote = replace;
        }
    }
    return replacements;
}

TableWriter::TableWriter( const TableDefinition &definition )
    : _stationData( definition.stationData ),
      _validColumns( definition.validColumns ),
      _delimiter( definition.delimiter.value_or( ',' ) ),
      _quote( definition.quote == _delimiter ? std::nullopt : definition.quote ),
      _escape( definition.escape == _delimiter ? std::nullopt : definition.escape ),
      _replacements( _replacementsFor( _quote, _escape, _delimiter ) ),
      _columns( definition.columns ),
      _headerRows( header_row_count( definition.columns ) ),
      _classId( definition.classIds ),
      _classValue( definition.classIds.size() )
{
}

bool TableWriter::listsObservations() const
{
    return _validColumns == &obs_valid_columns;
}

bool TableWriter::listsStations() const
{
    return _validColumns == &stn_valid_columns;
}

const ColumnSource *TableDefinition::validColumn( const std::string_view name ) const
{
    if( ! validColumns ) return nullptr;
    for( const ColumnSource &source : *validColumns )
    {
        if( boost::algorithm::iequals( source.name, name ) ) return &source;
    }
    return nullptr;
}

std::optional<ColumnSource> TableDefinition::classColumn( const std::string_view className )
{
    if( classIds.size() >= maximumClasses ) return std::nullopt;
    const std::string name( className );
    classIds.push_back( stationData ? net->class_id( name, 0 ) : obs_classes.id( name, 0 ) );
    return ColumnSource{ std::string_view(), 0, 0, JST_LEFT, TYPE_TEXT, classIds.size() - 1 };
}

void TableWriter::_printField( TableColumn &column, const std::string_view text, const int quoting, FILE *out ) const
{
    if( ! column.extralen )
    {
        int extralen = 0;
        if( quoting == QT_QUOTE && _quote ) extralen += 2;
        extralen += numeric_cast<int>( column.prefix.size() + column.suffix.size() );
        column.extralen = extralen;
    }
    const int spare = column.width - numeric_cast<int>( text.size() ) - *column.extralen;
    int left = 0;
    int right = 0;

    if( spare > 0 ) switch( column.just )
        {
        case JST_CENTRE:  right = spare/2; left = spare - right; break;
        case JST_RIGHT:   left = spare; break;
        default:          right = spare; break;
        }
    while( left-- ) fputc( ' ', out );
    write_text( out, column.prefix );
    if( quoting == QT_LITERAL )
    {
        write_text( out, text );
    }
    else if( quoting == QT_QUOTE && _quote )
    {
        fputc( *_quote, out );
        for( const char character : text )
        {
            if( character == _quote ) write_text( out, _replacements.quotedQuote );
            else if( character == _escape ) write_text( out, _replacements.quotedEscape );
            else fputc( character, out );
        }
        fputc( *_quote, out );
    }
    else
    {
        for( const char character : text )
        {
            if( character == _quote ) write_text( out, _replacements.unquotedQuote );
            else if( character == _escape ) write_text( out, _replacements.unquotedEscape );
            else if( character == _delimiter ) write_text( out, _replacements.unquotedDelimiter );
            else if( character == '\n' ) write_text( out, _replacements.unquotedNewline );
            else fputc( character, out );
        }
    }
    write_text( out, column.suffix );
    while( right-- ) fputc( ' ', out );
}

void TableWriter::printHeader( FILE *out )
{
    for( std::size_t row = 0; row < _headerRows; row++ )
    {
        bool first = true;
        for( TableColumn &column : _columns )
        {
            const std::string_view text = row < column.header.size() ? std::string_view( column.header[row] ) : std::string_view();
            if( first ) first = false; else fputc( _delimiter, out );
            _printField( column, text, QT_QUOTE, out );
        }
        fputc( '\n', out );
    }
}

void TableWriter::printRow( FILE *out )
{
    if( ! out ) return;
    bool first = true;
    for( TableColumn &column : _columns )
    {
        std::string computed;
        std::string_view text;
        if( const auto *textValue = std::get_if<const std::string *>( &column.value ) )
        {
            text = **textValue;
        }
        else if( const auto *classIndex = std::get_if<std::size_t>( &column.value ) )
        {
            text = _classValue[*classIndex];
        }
        else
        {
            const double number = *std::get<const double *>( column.value );
            if( column.type == TYPE_DOUBLE )
            {
                computed = format_fixed( number, column.ndp );
            }
            else if( column.type == TYPE_ANGLE && column.format )
            {
                double angle = number * RTOD;
                while( angle > 360 ) angle -= 360;
                while( angle < 0 ) angle += 360;
                computed = dms_string( angle, *column.format );
            }
            text = computed;
        }
        if( first ) first = false; else fputc( _delimiter, out );
        _printField( column, text, column.quote, out );
    }
    fputc( '\n', out );
}

void list_vecdata_residuals( FILE *out, survdata  *v, TableWriter &table )
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

        dummy1 = *from;
        dummy2 = *to;
        dummy1.modify_xyz( d1xyz, *el );
        dummy2.modify_xyz( d2xyz, *el );

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

        for( std::size_t i = 0; i < table.classCount(); i++ )
        {
            std::string value;
            const int idclass = table.classId( i );
            if( idclass > 0 )
            {
                auto name = get_obs_classification_name( v,  &(t->tgt), idclass );
                if( name ) value = std::move(*name);
            }
            table.setClassValue( i, std::move(value) );
        }

        table.printRow( out );
    }
}

static int list_observations( FILE *out, BINARY_FILE *bf, TableWriter &table )
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
        list_vecdata_residuals( out, sd, table );
    }

    delete_bindata(b);
    return OK;
}

static int list_stations( FILE *out, TableWriter &table )
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
        stn_order = net->order( net->station_order( st ) );
        if( stn_order.empty() ) stn_order = "-";

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

        for( std::size_t i = 0; i < table.classCount(); i++ )
        {
            std::string value;
            const int idclass = table.classId( i );
            if( idclass > 0 )
            {
                const int idvalue = st->get_class( idclass );
                if(idvalue > 0 )
                {
                    value = net->class_value( idclass, idvalue );
                }
            }
            table.setClassValue( i, std::move(value) );
        }
        table.printRow( out );
    }
    return OK;
}

static void print_table( TableWriter &table )
{
    if( ! table.stationData() && !is_projection( net->crdsys ) )
    {
        printf( "Cannot print snaplist data for coordinate systems without projections\n");
        return;
    }

    printf("\nPrinting table...\n");
    table.printHeader( out );

    if( table.listsObservations() )
    {
        list_observations( out, b, table );
    }
    else if ( table.listsStations() )
    {
        list_stations( out, table );
    }
}

/*======================================================================*/

static ListingConfig config;

static int read_angle_format( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_text( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_table( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_data( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_delimiter( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
static int read_column( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );

static config_item main_commands[] =
{
    {"angle_format",&config,CFG_ABSOLUTE,0,read_angle_format,0,0},
    {"text",NULL,CFG_ABSOLUTE,0,read_text,0,0},
    {"table",&config,CFG_ABSOLUTE,0,read_table,0,0},
    {NULL}
};

enum { CHAR_DELIM, CHAR_QUOTE, CHAR_ESCAPE };

static config_item table_commands[] =
{
    {"data",&config,CFG_ABSOLUTE,0,read_data,CFG_ONEONLY | CFG_REQUIRED,0},
    {"delimiter",&config,CFG_ABSOLUTE,0,read_delimiter,CFG_ONEONLY | CFG_REQUIRED,CHAR_DELIM},
    {"quote",&config,CFG_ABSOLUTE,0,read_delimiter,CFG_ONEONLY,CHAR_QUOTE},
    {"escape",&config,CFG_ABSOLUTE,0,read_delimiter,CFG_ONEONLY,CHAR_ESCAPE},
    {"column",&config,CFG_ABSOLUTE,0,read_column,CFG_REQUIRED,0},
    {"angle_format",&config,CFG_ABSOLUTE,0,read_angle_format,0,0},
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

/// Runs interpret_escaped_string() over the whole of source. The result ends at the first
/// NUL character that the decoding produces, as the C string it replaced did.
static std::string decode_escaped_string( const std::string_view source )
{
    std::string decoded;
    for( std::size_t i=0; i<source.size(); )
    {
        std::optional<char> ch;
        i += interpret_escaped_string( source, i, ch );
        if( ! ch ) continue;
        if( *ch == '\0' ) break;
        decoded += *ch;
    }
    return decoded;
}

// #pragma warning(disable: 4100)

static int read_angle_format( CFG_FILE *, std::string_view string, void *value, int, int )
{
    ListingConfig &config = *static_cast<ListingConfig *>( value );
    // The format's own field separator is whatever character comes first
    // after leading whitespace (e.g. "." or ":"). Fields are separated by
    // one or more of it, so "###a##b##c" and "#a#b#c" are the same format.
    const auto firstNonSpace = std::find_if( string.begin(), string.end(),
        []( const char c ){ return ! ISSPACE(c); } );
    if( firstNonSpace == string.end() ) return MISSING_DATA;
    const char delim = *firstNonSpace;
    FieldScanner scanner( string.substr( std::distance( string.begin(), firstNonSpace ) ) );

    const auto degField = scanner.nextToken( delim );
    const auto minField = scanner.nextToken( delim );
    if( ! minField ) return MISSING_DATA;
    const auto secField = scanner.nextToken( delim );

    config.degreeSeparator = decode_escaped_string( *degField );
    config.minuteSeparator = decode_escaped_string( *minField );
    config.secondSeparator = secField ? decode_escaped_string( *secField ) : std::string();
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

static int read_table( CFG_FILE *cfg, std::string_view, void *value, int, int )
{
    ListingConfig &config = *static_cast<ListingConfig *>( value );
    config.table = TableDefinition();
    const int read_opts = set_config_read_options( cfg, CFG_INIT_ITEMS | CFG_CHECK_MISSING | CFG_POSITIONAL_COMMENT );
    const int sts = read_config_file( cfg, table_commands );
    if( sts == OK )
    {
        TableWriter writer( config.table );
        print_table( writer );
    }
    set_config_read_options( cfg, read_opts );
    return OK;
}

// #pragma warning(disable: 4100)

static int read_data( CFG_FILE *cfg, std::string_view string, void *value, int, int )
{
    TableDefinition &table = static_cast<ListingConfig *>( value )->table;
    const std::vector<ColumnSource> *coltype = nullptr;
    FieldScanner scanner(string);
    auto s = scanner.next();
    if( s && boost::algorithm::iequals( *s, "stations" ) )
    {
        coltype = &stn_valid_columns;
        table.stationData = true;
    }
    else if( s && boost::algorithm::iequals( *s, "gps" ) )
    {
        coltype = &obs_valid_columns;
        table.stationData = false;
    }
    else
    {
        send_config_error( cfg, INVALID_DATA, "Program only handles GPS and station data currently" );
    }
    if( ! table.validColumns ) table.validColumns = coltype;
    if( table.validColumns != coltype )
    {
        send_config_error( cfg, INVALID_DATA, "Inconsistent data types defined" );
    }
    return OK;
}

// #pragma warning(disable: 4100)

static int read_delimiter( CFG_FILE *, std::string_view string, void *value, int, int code )
{
    TableDefinition &table = static_cast<ListingConfig *>( value )->table;
    FieldScanner scanner(string);
    auto s = scanner.next();
    if( !s ) return MISSING_DATA;

    std::optional<char> character;
    if( boost::algorithm::iequals(*s,"tab") )
    {
        character = '\t';
    }
    else if( boost::algorithm::iequals(*s,"comma") )
    {
        character = ',';
    }
    else if( boost::algorithm::iequals(*s,"blank") )
    {
        character = ' ';
    }
    else if( ! boost::algorithm::iequals(*s,"none") )
    {
        // Only the first character is used
        const std::string decoded = decode_escaped_string( *s );
        if( ! decoded.empty() ) character = decoded.front();
    }

    switch( code )
    {
    case CHAR_QUOTE: table.quote = character; break;
    case CHAR_DELIM: table.delimiter = character; break;
    case CHAR_ESCAPE: table.escape = character; break;
    default:
        return INVALID_DATA;
    }
    return OK;
}

/// Splits a column heading into one entry for each line, up to maximumHeaderRows
static std::vector<std::string> split_header( const std::string_view header )
{
    std::vector<std::string> rows;
    if( header.empty() ) return rows;
    std::string_view remaining = header;
    while( rows.size() < maximumHeaderRows )
    {
        const std::size_t newline = remaining.find( '\n' );
        rows.emplace_back( remaining.substr( 0, newline ) );
        if( newline == std::string_view::npos ) break;
        remaining = remaining.substr( newline + 1 );
    }
    return rows;
}

// #pragma warning(disable: 4100)

static int read_column( CFG_FILE *cfg, std::string_view string, void *value, int, int )
{
    ListingConfig &config = *static_cast<ListingConfig *>( value );
    int width = -1;
    int just = -1;
    int ndp = -1;
    int quote = QT_NONE;
    std::string header;
    std::string prefix;
    std::string suffix;

    FieldScanner scanner(string);
    auto data = scanner.next();
    if( !data ) return MISSING_DATA;
    std::optional<ColumnSource> source;
    if( boost::algorithm::istarts_with(*data,"class="))
    {
        source = config.table.classColumn( data->substr(6) );
    }
    else if( const ColumnSource *valid = config.table.validColumn( *data ) )
    {
        source = *valid;
    }
    if( !source )
    {
        send_config_error( cfg, INVALID_DATA, "Invalid column name " + std::string( data->substr( 0, 20 ) ) + " specified" );
        return OK;
    }

    for( auto opt = scanner.next(); opt; opt = scanner.next() )
    {
        if( boost::algorithm::iequals(*opt,"quote") ) { quote = QT_QUOTE; continue; }
        if( boost::algorithm::iequals(*opt,"literal") ) { quote = QT_LITERAL; continue; }
        const auto eqPos = opt->find('=');
        if( eqPos == std::string_view::npos || eqPos == opt->size()-1 )
        {
            send_config_error( cfg, MISSING_DATA,
                               "Missing value for option " + std::string( opt->substr( 0, 20 ) ) + " in column command" );
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
            header = decode_escaped_string( val );
            continue;
        }
        else if( boost::algorithm::iequals(key,"prefix") )
        {
            prefix = decode_escaped_string( val );
            continue;
        }
        else if( boost::algorithm::iequals(key,"suffix") )
        {
            suffix = decode_escaped_string( val );
            continue;
        }
        else
        {
            send_config_error( cfg, INVALID_DATA,
                               "Invalid option " + std::string( key.substr( 0, 20 ) ) + " in column command" );
            return OK;
        }
        {
            send_config_error( cfg, INVALID_DATA,
                               "Invalid value " + std::string( val.substr( 0, 20 ) ) + " for option " + std::string( key.substr( 0, 20 ) ) );
            return OK;
        }

    }

    TableColumn column( *source );
    if( width >= 0 ) column.width = width;
    if( just > 0 ) column.just = just;
    if( ndp >= 0 ) column.ndp = ndp;
    column.quote = quote;
    if( column.type == TYPE_ANGLE )
    {
        column.format.emplace( config.angleFormat( ndp ) );
    }
    column.header = split_header( header );
    column.prefix = std::move( prefix );
    column.suffix = std::move( suffix );
    config.table.columns.push_back( std::move( column ) );

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
    to_xyz = coord_conversion( net->crdsys, xyzcs );
    from_xyz = coord_conversion( xyzcs, net->crdsys );
    el = net->crdsys->rf->el;

    get_network_topocentre( net, &lat, &lon );
    dummy1 = station( "0", "0", lat, lon, 0.0, 0.0, 0.0, 0.0, *el );
    dummy2 = station( "0", "0", lat, lon, 0.0, 0.0, 0.0, 0.0, *el );

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
