#include "snapconfig.h"
/* netlist2.c: Station list functions accessed via network */

/*
   $Log: networks.c,v $
   Revision 1.2  1998/05/21 04:00:28  ccrook
   Added geodetic coordinate system to network object and facilitated getting and setting
   coordinates in the network coordinate system.

   Revision 1.1  1995/12/22 17:37:17  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <boost/algorithm/string/predicate.hpp>
#include "util/snapctype.h"

#include "network/network.h"
#include "util/dstring.h"
#include "util/fieldscanner.hpp"
#include "util/fileutil.h"
#include "util/filelist.h"
#include "util/errdef.h"
#include "util/polygon.h"
#include "util/pi.h"
#include "util/wildcard.h"


#define COMMENT_CHAR '!'

// The order matters: skip_criterion_frame compares operators.
enum class criterion_operator
{
    line,
    except,
    and_,
    or_
};

#define CRIT_IGNORE_MISSING 1
#define CRIT_WARN_MISSING 2
#define CRIT_STATUS_FAIL_MISSING 3

#define CRIT_ID_UNKNOWN -1
#define CRIT_ID_MISSING -2

#define CRIT_STATUS_UNKNOWN 0
#define CRIT_STATUS_PASS 1
#define CRIT_STATUS_FAIL 2

static const char *source_prefix="station list in ";
static const char *default_source="station list";

struct code_criterion
{
    std::string code;
    // Cache of the station id this code resolved to, or CRIT_ID_UNKNOWN or
    // CRIT_ID_MISSING. Station ids can change if stations are removed.
    mutable int id = CRIT_ID_UNKNOWN;
    int missing_error = OK;
};

struct code_match_criterion
{
    std::string code;
};

struct code_range_criterion
{
    std::string fromcode;
    std::string tocode;
};

struct polygon_criterion
{
    void *polygon = nullptr;
    coordsys *cs = nullptr;
    coord_conversion *conv = nullptr;
    bool isgeodetic = false;
    bool inside = false;
};

struct classification_criterion
{
    int class_id = 0;
    int value_id = 0;
};

// Matches every station.
struct all_criterion
{
};

// Marks the start of a frame. The criteria after it at a greater stacklevel
// are evaluated together, and their result is combined using this entry's
// crit_operator (inverted for except). It matches no station itself.
struct frame_start
{
};

using criterion_type = std::variant<all_criterion, frame_start, code_criterion,
                                    code_match_criterion, code_range_criterion,
                                    polygon_criterion, classification_criterion>;

struct criterion
{
    criterion( criterion_type type, criterion_operator crit_operator, int stacklevel )
        : type( std::move( type ) ), crit_operator( crit_operator ), stacklevel( stacklevel ) {}

    const criterion_type type;
    const criterion_operator crit_operator;
    const int stacklevel;
    // Index into station_criteria::sources, or nullopt if the criterion
    // did not come from a station list file.
    std::optional<size_t> source;
};

struct station_criteria
{
    std::vector<criterion> criteria;
    // The result of matching each station, indexed by station id, as one of
    // CRIT_STATUS_*. No value means caching has not been set up.
    std::optional<std::vector<unsigned char>> cache;
    bool all_code_criteria = true;
    // The station list file names criteria were read from, without
    // duplicates. criterion::source is an index into this.
    std::vector<std::string> sources;
};

/*-----------------------------------------------------------------------*/

static unsigned char check_criteria_cache( const std::vector<unsigned char> &cache, int id )
{
    if( id < 0 ) return CRIT_STATUS_UNKNOWN;
    const size_t index=id;
    return index < cache.size() ? cache[index] : CRIT_STATUS_UNKNOWN;
}

static void set_criteria_cache( std::vector<unsigned char> &cache, int id, unsigned char match )
{
    if( id < 0 ) return;
    const size_t index=id;
    if( index >= cache.size() )
    {
        const size_t newsize=std::max( { cache.size()*2, index+index/10+1, size_t( 1024 ) } );
        cache.resize( newsize, CRIT_STATUS_UNKNOWN );
    }
    cache[index]=match;
}

/*-----------------------------------------------------------------------*/

static criterion new_all_criterion( criterion_operator crit_operator, int stacklevel )
{
    return criterion( all_criterion(), crit_operator, stacklevel );
}

static criterion new_criteria_frame( criterion_operator crit_operator, int stacklevel )
{
    return criterion( frame_start(), crit_operator, stacklevel );
}

/*-----------------------------------------------------------------------*/

static criterion new_code_criterion( const std::string &code, int missing_error, criterion_operator crit_operator, int stacklevel )
{
    return criterion( code_criterion{ code, CRIT_ID_UNKNOWN, missing_error }, crit_operator, stacklevel );
}

static bool code_criterion_match( const criterion &c, station *stn )
{
    const code_criterion &code=std::get<code_criterion>( c.type );
    if( stn->id == code.id ) return true;
    if( code.id == CRIT_ID_MISSING ) return false;
    if( boost::algorithm::iequals( std::string_view( stn->Code ), code.code ) )
    {
        code.id = stn->id;
        return true;
    }
    return false;
}

/*-----------------------------------------------------------------------*/

static criterion new_code_match_criterion( const std::string &code, criterion_operator crit_operator, int stacklevel )
{
    return criterion( code_match_criterion{ code }, crit_operator, stacklevel );
}

static bool code_match_criterion_match( const criterion &c, station *stn )
{
    return wildcard_match( std::get<code_match_criterion>( c.type ).code, stn->Code );
}

/*-----------------------------------------------------------------------*/

static criterion new_code_range_criterion( const std::string &fromcode, const std::string &tocode, criterion_operator crit_operator, int stacklevel )
{
    return criterion( code_range_criterion{ fromcode, tocode }, crit_operator, stacklevel );
}

static bool code_range_criterion_match( const criterion &c, station *stn )
{
    const code_range_criterion &range=std::get<code_range_criterion>( c.type );
    return stncodecmp( stn->Code, range.fromcode ) >= 0 &&
           stncodecmp( stn->Code, range.tocode ) <= 0;
}

/*-----------------------------------------------------------------------*/

static criterion new_polygon_criterion( void *polygon, coordsys *cs, coord_conversion *conv, bool isgeodetic, bool inside, criterion_operator crit_operator, int stacklevel )
{
    return criterion( polygon_criterion{ polygon, cs, conv, isgeodetic, inside }, crit_operator, stacklevel );
}

static bool polygon_criterion_match( const criterion &c, station *stn )
{
    const polygon_criterion &poly=std::get<polygon_criterion>( c.type );
    bool isinside;
    double lon=stn->ELon;
    double lat=stn->ELat;
    if( poly.conv )
    {
        double llh[3];
        llh[CRD_LAT]=lat;
        llh[CRD_LON]=lon;
        llh[CRD_HGT]=stn->OHgt+stn->GUnd;
        convert_coords( poly.conv, llh, NULL, llh, NULL );
        if( poly.isgeodetic )
        {
            lon=llh[CRD_LON]*RTOD;
            lat=llh[CRD_LAT]*RTOD;
        }
        else
        {
            lon=llh[CRD_EAST];
            lat=llh[CRD_NORTH];
        }
    }
    else
    {
        lat *= RTOD;
        lon *= RTOD;
    }
    isinside=polygon_contains_point( poly.polygon, lon, lat ) ? 1 : 0;
    bool ok=isinside == poly.inside;
    return ok;
}

// The polygon criterion owns its polygon, coordinate conversion and
// coordinate system. Nothing else in a criterion needs freeing.
static void delete_polygon_criterion( const polygon_criterion &poly )
{
    if( poly.polygon ) delete_polygon( poly.polygon );
    delete poly.conv;
    if( poly.cs ) delete poly.cs;
}

/*-----------------------------------------------------------------------*/

static criterion new_classification_criterion( int class_id, int value_id, criterion_operator crit_operator, int stacklevel )
{
    return criterion( classification_criterion{ class_id, value_id }, crit_operator, stacklevel );
}

static bool classification_criterion_match( const criterion &c, station *stn )
{
    const classification_criterion &clsf=std::get<classification_criterion>( c.type );
    return stn->get_class( clsf.class_id ) == clsf.value_id;
}

/*-----------------------------------------------------------------------*/

static bool criterion_match( const criterion &c, station *stn )
{
    if( std::holds_alternative<all_criterion>( c.type ) ) return true;
    if( std::holds_alternative<code_criterion>( c.type ) ) return code_criterion_match( c, stn );
    if( std::holds_alternative<code_match_criterion>( c.type ) ) return code_match_criterion_match( c, stn );
    if( std::holds_alternative<code_range_criterion>( c.type ) ) return code_range_criterion_match( c, stn );
    if( std::holds_alternative<polygon_criterion>( c.type ) ) return polygon_criterion_match( c, stn );
    if( std::holds_alternative<classification_criterion>( c.type ) ) return classification_criterion_match( c, stn );
    return false;
}

static void delete_criterion( const criterion &c )
{
    if( const polygon_criterion *poly=std::get_if<polygon_criterion>( &c.type ) ) delete_polygon_criterion( *poly );
}

/*-----------------------------------------------------------------------*/

void *new_station_criteria()
{
    return new station_criteria();
}

void setup_station_criteria_cache( void *psc, int maxstn )
{
    station_criteria *sc=(station_criteria *) psc;
    if( sc->cache ) return;
    // Station ids run from 1 to maxstn
    if( maxstn > 0 ) sc->cache=std::vector<unsigned char>( maxstn+1, CRIT_STATUS_UNKNOWN );
}

// Returns the index of file in sc->sources, adding it if it is not there.
static size_t station_criteria_source_index( station_criteria *sc, const std::string &file )
{
    const auto found=std::find( sc->sources.begin(), sc->sources.end(), file );
    if( found != sc->sources.end() ) return found-sc->sources.begin();
    sc->sources.push_back( file );
    return sc->sources.size()-1;
}

static bool station_criteria_source_used( const station_criteria *sc, int maxstack, const std::string &file )
{
    for( const criterion &c : sc->criteria )
    {
        if( c.stacklevel < maxstack && c.source && sc->sources[*c.source] == file ) return true;
    }
    return false;
}


static void delete_all_station_criteria( station_criteria *sc )
{
    for( const criterion &c : sc->criteria ) delete_criterion( c );
    sc->criteria.clear();
    sc->all_code_criteria=true;
}

static void add_station_criterion( station_criteria *sc, criterion c, std::optional<size_t> source )
{
    /* Optimisation for simple list of codes */
    if( ! (std::holds_alternative<code_criterion>( c.type ) || std::holds_alternative<frame_start>( c.type )) ||
        (c.crit_operator != criterion_operator::line && c.crit_operator != criterion_operator::or_) )
                sc->all_code_criteria=false;

    c.source=source;
    sc->criteria.push_back( std::move( c ) );
}

// The frame functions take pos, the index of the criterion to start at, and
// leave it at the first criterion after the frame.
static void skip_criterion_frame( const std::vector<criterion> &criteria, size_t &pos, int stacklevel, criterion_operator op )
{
    while( pos < criteria.size() &&
            (criteria[pos].stacklevel > stacklevel ||
            (criteria[pos].stacklevel == stacklevel && criteria[pos].crit_operator >= op)) )
    {
        pos++;
    }
}

static bool criterion_frame_match( const std::vector<criterion> &criteria, size_t &pos, station *stn )
{
    // pos must be strictly less than criteria.size() to index criteria[pos].
    // It equals size() when there is nothing left, including an empty list.
    if( pos >= criteria.size() ) return false;
    bool invert=criteria[pos].crit_operator == criterion_operator::except;
    bool match=invert;
    int stacklevel=criteria[pos].stacklevel;
    while( pos < criteria.size() && criteria[pos].stacklevel >= stacklevel )
    {
        const criterion &crt=criteria[pos];
        criterion_operator op=crt.crit_operator;
        /* Can we skip this frame */
        if( (op == criterion_operator::line && match) ||
                (op == criterion_operator::except && ! match) ||
                (op == criterion_operator::and_ && ! match) ||
                (op == criterion_operator::or_ && match) )
        {
            skip_criterion_frame( criteria, pos, stacklevel, op );
            continue;
        }
        if( std::holds_alternative<frame_start>( crt.type ) )
        {
            // A frame with no criteria after it, such as an empty station
            // list file, has no effect and is stepped over
            pos++;
            if( pos < criteria.size() && criteria[pos].stacklevel > crt.stacklevel )
            {
                match=criterion_frame_match( criteria, pos, stn );
                if( op == criterion_operator::except ) match = ! match;
            }
        }
        else
        {
            match=criterion_match( crt, stn );
            pos++;
        }
    }
    return match;
}

bool station_criteria_match( void *psc, station *stn )
{
    station_criteria *sc=(station_criteria *) psc;
    unsigned int status=CRIT_STATUS_UNKNOWN;
    if( sc->cache ) status=check_criteria_cache(*sc->cache, stn->id);
    if( status == CRIT_STATUS_UNKNOWN )
    {
        /* Match if any criteria match, except may be undone by reverse criteria */
        size_t pos=0;
        status=criterion_frame_match( sc->criteria, pos, stn ) ? CRIT_STATUS_PASS : CRIT_STATUS_FAIL;
        if( sc->cache ) set_criteria_cache(*sc->cache, stn->id, status );
    }
    return status == CRIT_STATUS_PASS;
}

void delete_station_criteria( void *psc )
{
    station_criteria *sc=(station_criteria *) psc;
    delete_all_station_criteria( sc );
    delete sc;
}

void apply_station_criteria_to_network( void *psc, network *nw, 
    void *data, void (*function)( station *stn, void *data ))
{
    station_criteria *sc=(station_criteria *) psc;
    /* Optimisation for simple criteria - no need to process entire list */
    if( sc->all_code_criteria )
    {
        for( const criterion &c : sc->criteria )
        {
            const code_criterion *code=std::get_if<code_criterion>( &c.type );
            if( ! code ) continue;
            const int id=find_station(nw, code->code );
            if( id )
            {
                station *stn=station_ptr(nw,id);
                (*function)(stn,data);
            }
        }
        return;
    }
    /* Otherwise check each station for match */
    for( int istn = number_of_stations(nw); istn; istn-- ) {
        station *stn = station_ptr(nw,istn);
        if( station_criteria_match(psc,stn))
        {
            (*function)(stn,data);
        }
    }
}

int check_station_criteria_codes( void *psc, network *nw )
{
    station_criteria *sc = (station_criteria *) psc;
    int sts=OK;
    for( const criterion &c : sc->criteria )
    {
        const code_criterion *code=std::get_if<code_criterion>( &c.type );
        if( ! code ) continue;
        if( code->missing_error == OK ) continue;
        if( code->id == CRIT_ID_UNKNOWN )
        {
            int id=find_station(nw, code->code );
            if( id )
            {
                code->id=id;
                continue;
            }
            code->id=CRIT_ID_MISSING;
        }
        if( code->id != CRIT_ID_MISSING ) continue;
        const std::string source=c.source ? source_prefix+sc->sources[*c.source] : default_source;
        const std::string errmess="Invalid station "+code->code.substr(0,20)+" in "+source.substr(0,80);
        handle_error(code->missing_error,errmess,NO_MESSAGE);
        if( sts != INVALID_DATA ) sts=code->missing_error;
    }
    return sts;
}

/*-----------------------------------------------------------------------*/


// source is the index in sc->sources of the station list file select was read
// from, if any. missing_error is the default for stations that do not exist.
static int compile_station_criteria1( station_criteria *sc, network *nw, std::string_view select, const std::string &basefile, int stacklevel, std::optional<size_t> source, int missing_error );

static int compile_station_list_file_criteria( station_criteria *sc, network *nw, const std::string &file, const std::string &basefile, int stacklevel, int missing_error )
{
    std::optional<std::string> spec;
    FILE *list_file;
    char buf[2048];
    int sts = OK;

    spec = find_file( file, DFLTSTLIST_EXT, std::optional<std::string>( basefile ), FF_TRYLOCAL, "" );
    list_file = NULL;
    if( spec ) list_file = fopen( spec->c_str(), "r" );

    if( !list_file )
    {
        const std::string errmess="Cannot open station list file "+file.substr(0,MAX_FILENAME_LEN)+"\n";
        handle_error( INVALID_DATA, errmess, NO_MESSAGE  );
        return INVALID_DATA;
    }
    record_filename( *spec, "station_list_file" );

    skip_utf8_bom(list_file);

    const size_t source=station_criteria_source_index( sc, file );
    while( sts==OK && fgets(buf,2048,list_file) )
    {
        char *b = buf;
        while( *b && ISSPACE(*b) ) b++;
        if( ! *b || *b == COMMENT_CHAR ) continue;
        sts=compile_station_criteria1(sc, nw,b,file,stacklevel,source,missing_error);
    }
    return sts;
}

static int compile_station_criteria1( station_criteria *sc, network *nw, std::string_view select, const std::string &basefile, int stacklevel, std::optional<size_t> source, int missing_error )
{
    char errmess[200];
    int sts=OK;
    // The operator read for the next criterion, if any. A list starts with
    // line, so it cannot begin with and, or, or except.
    std::optional<criterion_operator> curop=criterion_operator::line;
    const int baselevel=stacklevel;

    errmess[0] = 0;
    const std::string src=source ? source_prefix+sc->sources[*source] : default_source;

    FieldScanner scanner( select );
    std::optional<std::string_view> field;
    while( (field=scanner.next()) )
    {

        /* Missing station options */

        if( boost::algorithm::iequals( *field, "ignore_missing" ) )
        {
            missing_error=OK;
            continue;
        }

        if( boost::algorithm::iequals( *field, "warn_missing" ) )
        {
            missing_error=INFO_ERROR;
            continue;
        }

        if( boost::algorithm::iequals( *field, "fail_missing" ) )
        {
            missing_error=INVALID_DATA;
            continue;
        }

        /* Operators */

        {
            std::optional<criterion_operator> op;
            if( boost::algorithm::iequals( *field, "except" ) ) op=criterion_operator::except;
            else if( boost::algorithm::iequals( *field, "and" ) ) op=criterion_operator::and_;
            else if( boost::algorithm::iequals( *field, "or" ) ) op=criterion_operator::or_;
            if( op )
            {
                if( curop )
                {
                    std::string fieldText(*field);
                    sprintf(errmess,"\"%s\" out of place in %.100s",fieldText.c_str(),src.c_str());
                    break;
                }
                /* Except increments stack level by one so can evaluate except clause on stack
                 * before inverting status */
                if( *op == criterion_operator::except )
                {
                    add_station_criterion( sc, new_criteria_frame( *op, baselevel ), source );
                    op=criterion_operator::line;
                    stacklevel=baselevel+1;
                }
                curop=op;
                continue;
            }
        }

        /* Items are alternatives unless another operator is given */
        const criterion_operator op = curop.value_or( criterion_operator::or_ );

        /* If this reference a file of station definitions, then process the file.
           Not allowed if this is already in a station list file. */

        if( field->size() > 1 && (*field)[0] == '@' )
        {
            const std::string file( field->substr(1) );
            /* Add a placeholder for the current operation */
            add_station_criterion( sc, new_criteria_frame( op, stacklevel ), source );
            if( station_criteria_source_used( sc, stacklevel, file ))
            {
                sprintf(errmess,"Station list file %.100s uses itself",file.c_str());
                break;
            }
            /* Embedded station list increments stack level by 2 to distinguish from
             * except stack level */
            sts=compile_station_list_file_criteria( sc, nw,file,basefile,stacklevel+1,missing_error);
            if( sts != OK )
            {
                sprintf(errmess,"Error processing station list file %.100s",file.c_str());
                break;
            }
            curop.reset();
            continue;
        }

        std::optional<criterion> c;
        if( boost::algorithm::iequals( *field, "all" ) )
        {
            c.emplace( new_all_criterion( op, stacklevel ) );
        }
        else if( boost::algorithm::iequals( *field, "inside" ) || boost::algorithm::iequals( *field, "outside" ) )
        {
            bool isgeo=true;
            coordsys *cs;
            coord_conversion *conv=nullptr;

            const bool inside=boost::algorithm::iequals(*field,"inside");
            const auto crdsysField=scanner.next();
            const auto pgnfileField=scanner.next();
            if( ! pgnfileField )
            {
                std::string fieldText(*field);
                sprintf(errmess,"Invalid \"%s\" option in %s requires coord sys code and wkt file name",fieldText.c_str(),src.c_str());
                break;
            }
            const std::string crdsys( *crdsysField );
            const std::string pgnfile( *pgnfileField );
            cs=load_coordsys( crdsys );
            if( ! cs )
            {
                sprintf(errmess,"Invalid coordinate system %-20s in \"%s\" option in %s",crdsys.c_str(),std::string(*field).c_str(),src.c_str());
                break;
            }

            std::optional<std::string> spec = find_file( pgnfile, DFLT_WKT_EXT, std::optional<std::string>( basefile ), FF_TRYLOCAL, "" );
            if( ! spec )
            {
                sprintf(errmess,"Cannot find WKT polygon file %.50s in %s",
                        pgnfile.c_str(),src.c_str());
                break;
            }

            if( identical_coordinate_systems( cs, nw->geosys ) )
            {
                delete cs;
                cs=nullptr;
            }
            else
            {
                isgeo=is_geodetic(cs);
                conv=new coord_conversion( nw->geosys, cs, DEFAULT_CRDSYS_EPOCH );
                if( ! conv->valid )
                {
                    sprintf(errmess,"Cannot use WKT coordinate system %.20s in %s option in %s",
                            crdsys.c_str(),std::string(*field).c_str(),src.c_str());
                    delete conv;
                    delete cs;
                    break;
                }
            }

            void *const pgn=read_polygon_wkt( *spec, isgeo);
            if( ! pgn )
            {
                sprintf(errmess,"Cannot read WKT polygon file %.50s in %s",
                        pgnfile.c_str(),src.c_str());
                delete conv;
                if( cs ) delete cs;
                break;
            }
            record_filename(*spec,"wkt_polygon_definition");
            c.emplace( new_polygon_criterion( pgn, cs, conv, isgeo, inside, op, stacklevel ) );
        }


        /* If this is a classification criteria ("class=value1/value2/..."):
           split on the first '=' after the first character (so a field
           starting with '=' is never treated as a classification), then
           split the value on '/'. A trailing or doubled '/' produces an
           empty value segment, matched against no defined class value -
           reproducing the original char*-based splitting's behavior
           exactly, not just its common case. */

        else if( (*field)[0] != '\\' && field->find('=',1) != std::string_view::npos )
        {
            const auto eqPos=field->find('=',1);
            const std::string className( field->substr(0,eqPos) );
            const int class_id = nw->class_id( className, 0 );
            const std::string_view values = field->substr(eqPos+1);
            // The first value uses the operator read, the rest are ored with it
            criterion_operator valueOp=op;
            for( size_t pos=0; pos < values.size(); )
            {
                const auto slashPos=values.find('/',pos);
                const auto valueEnd = slashPos==std::string_view::npos ? values.size() : slashPos;
                const std::string value( values.substr(pos,valueEnd-pos) );
                const int value_id = class_id ? nw->class_value_id( class_id, value, 0 ) : CLASS_VALUE_NOT_DEFINED;
                if( value_id != CLASS_VALUE_NOT_DEFINED )
                {
                    add_station_criterion( sc, new_classification_criterion( class_id, value_id, valueOp, stacklevel ), source );
                    valueOp=criterion_operator::or_;
                }
                pos = slashPos==std::string_view::npos ? values.size() : slashPos+1;
            }
            curop.reset();
            continue;
        }

        /* Is it matched as a range? */

        else if( (*field)[0] != '\\' && field->find('-',1) != std::string_view::npos )
        {
            const auto dashPos=field->find('-',1);
            std::string fromCode( field->substr(0,dashPos) );
            std::string toCode( field->substr(dashPos+1) );
            c.emplace( new_code_range_criterion( fromCode, toCode, op, stacklevel ) );
        }
        else if( (*field)[0] != '\\' && has_wildcard( *field ) )
        {
            std::string fieldStr(*field);
            c.emplace( new_code_match_criterion( fieldStr, op, stacklevel ) );
        }
        else
        {
            /* Allow \ escape on station names matching keywords */

            std::string_view codeField=*field;
            if( codeField.front() == '\\' ) codeField.remove_prefix(1);
            if( codeField.empty() ) continue;
            std::string codeStr(codeField);
            c.emplace( new_code_criterion(codeStr, missing_error, op, stacklevel) );
        }

        if( c )
        {
            add_station_criterion( sc, std::move( *c ), source );
            curop.reset();
        }
    }
    if( ! errmess[0] && curop )
    {
         sprintf(errmess,"Station list cannot end with and, or, or except in %.100s",src.c_str());
    }
    if( errmess[0] )
    {
        sts=INVALID_DATA;
        handle_error(INVALID_DATA,errmess,NO_MESSAGE);
    }
    return sts;
}

int compile_station_criteria( void *psc, network *nw, const std::string &select, const std::string &basefile )
{
    int sts;
    station_criteria *sc=(station_criteria *) psc;
    sts=compile_station_criteria1( sc, nw, select, basefile, 0, std::nullopt, INVALID_DATA );
    return sts;
}


int process_selected_stations( network *nw, const std::string &select, const std::string &basefile,
                                void *data, void (*function)( station *st, void *data ))
{
    int sts;
    int chksts=OK;
    void *psc=new_station_criteria();
    sts=compile_station_criteria( psc, nw, select, basefile );
    if( sts == OK ) chksts=check_station_criteria_codes( psc, nw );
    if( sts == OK ) apply_station_criteria_to_network( psc, nw, data, function );
    delete_station_criteria( psc );
    return sts==OK ? chksts : sts;
}
