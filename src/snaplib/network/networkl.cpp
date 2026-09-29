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
#include <forward_list>
#include <optional>
#include <string>
#include <string_view>
#include <boost/algorithm/string/predicate.hpp>
#include "util/snapctype.h"

#include "network/network.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/fieldscanner.hpp"
#include "util/fileutil.h"
#include "util/filelist.h"
#include "util/errdef.h"
#include "util/polygon.h"
#include "util/pi.h"
#include "util/wildcard.h"


#define COMMENT_CHAR '!'

#define CRIT_NONE      0
#define CRIT_ALL       1
#define CRIT_CODE      2
#define CRIT_MATCH     3
#define CRIT_RANGE     4
#define CRIT_POLYGON   5
#define CRIT_CLSF      6
#define CRIT_FRAME    32 

#define CRIT_OP_DEFAULT 0
#define CRIT_OP_LINE    1
#define CRIT_OP_EXCEPT  2
#define CRIT_OP_AND     3
#define CRIT_OP_OR      4

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
    char *code;
    int id;
    int missing_error;
};

struct criteria_cache
{
    unsigned char *cache;
    int maxcache;
};

struct code_match_criterion
{
    char *code;
};

struct code_range_criterion
{
    char *fromcode;
    char *tocode;
};

struct polygon_criterion
{
    void *polygon;
    coordsys *cs;
    coord_conversion *conv;
    bool isgeodetic;
    bool inside;
};

struct classification_criterion
{
    int class_id;
    int value_id;
};

struct criterion
{
    unsigned char type;
    unsigned char crit_operator;
    unsigned char stacklevel;
    const char *source;
    union
    {
        code_criterion code;
        code_match_criterion code_match;
        code_range_criterion code_range;
        polygon_criterion polygon;
        classification_criterion classification;
    } c;
    struct criterion *next;
};

struct station_criteria
{
    criterion *first = nullptr;
    criterion *last = nullptr;
    criteria_cache *cache = nullptr;
    bool all_code_criteria = true;
    // Each entry is one "station list in <file>" source, in the order
    // encountered. forward_list rather than vector: cur_source below, and
    // criterion::source elsewhere, hold views/pointers straight into an
    // entry's string, and must stay valid across later insertions - a
    // guarantee forward_list gives unconditionally (push_front never
    // relocates existing elements) that vector cannot.
    std::forward_list<std::string> sources;
    // Always a view of a whole entry in sources (never a substring) -
    // criterion::source elsewhere relies on that to treat .data() as a
    // null-terminated C string.
    std::optional<std::string_view> cur_source;
    int cur_missing_error = INVALID_DATA;
};

/*-----------------------------------------------------------------------*/

static criteria_cache *new_criteria_cache( int maxcache )
{
    criteria_cache *cache=(criteria_cache *) check_malloc( sizeof(criteria_cache));
    cache->cache=0;
    cache->maxcache=maxcache;
    return cache;
}

static unsigned char check_criteria_cache( criteria_cache *cache, int id )
{
    if( id < 0 || ! cache->cache || id > cache->maxcache ) return CRIT_STATUS_UNKNOWN;
    return cache->cache[id];
}

static void set_criteria_cache( criteria_cache *cache, int id, unsigned char match )
{
    if( id < 0 ) return;
    if( id > cache->maxcache || ! cache->cache )
    {
        unsigned char *newcache;
        int maxcache=cache->maxcache;
        int newmax=cache->maxcache*2;
        int idmax=id+id/10;
        if( idmax > newmax ) newmax=idmax;
        if( newmax < 1023 ) newmax=1023;
        newcache=(unsigned char *)check_malloc( newmax+1 );
        if( cache->cache )
        {
            memcpy( newcache, cache->cache, maxcache );
            memset( newcache+maxcache+1, CRIT_STATUS_UNKNOWN, newmax-maxcache );
            check_free( cache->cache );
        }
        else
        {
            memset( newcache, CRIT_STATUS_UNKNOWN, newmax );
        }
        cache->cache=newcache;
        cache->maxcache=newmax;
    }
    cache->cache[id]=match;
}

static void delete_criteria_cache( criteria_cache *cache )
{
    if( ! cache ) return;
    if( cache->cache ) check_free( cache->cache );
    check_free( cache );
}

/*-----------------------------------------------------------------------*/

static criterion *new_criterion()
{
    criterion *c=(criterion *)check_malloc(sizeof(criterion));
    c->type=CRIT_NONE;
    c->crit_operator=CRIT_OP_OR;
    c->stacklevel=0;
    c->next=nullptr;
    c->source=default_source;
    return c;
}

static criterion *new_all_criterion()
{
    criterion *c=new_criterion();
    c->type=CRIT_ALL;
    return c;
}

static criterion *new_criteria_frame( int crit_operator, int stacklevel )
{
    criterion *c=new_criterion();
    c->type=CRIT_FRAME;
    c->crit_operator=crit_operator;
    c->stacklevel=stacklevel;
    return c;
}

/*-----------------------------------------------------------------------*/

static criterion *new_code_criterion( const std::string &code, int missing_error )
{
    criterion *c=new_criterion();
    c->type=CRIT_CODE;
    c->c.code.code=copy_string(code.c_str());
    c->c.code.id=CRIT_ID_UNKNOWN;
    c->c.code.missing_error=missing_error;
    return c;
}

static bool code_criterion_match( criterion *c, station *stn )
{
    if( stn->id == c->c.code.id ) return true;
    if( c->c.code.id == CRIT_ID_MISSING ) return false;
    if( _stricmp(stn->Code,c->c.code.code) == 0 )
    {
        c->c.code.id = stn->id;
        return true;
    }
    return false;
}

static void delete_code_criterion( criterion *c )
{
    check_free( c->c.code.code );
    c->c.code.code=nullptr;
}

/*-----------------------------------------------------------------------*/

static criterion *new_code_match_criterion( const std::string &code )
{
    criterion *c=(criterion *) new_criterion();
    c->type=CRIT_MATCH;
    c->c.code_match.code=copy_string(code.c_str());
    return c;
}

static bool is_code_match( const char *m, const char *c )
{
    return wildcard_match(m,c);
}

static bool code_match_criterion_match( criterion *c, station *stn )
{
    return is_code_match( c->c.code_match.code, stn->Code );
}

static void delete_code_match_criterion( criterion *c )
{
    check_free( c->c.code_match.code );
    c->c.code_match.code=nullptr;
}

/*-----------------------------------------------------------------------*/

static criterion *new_code_range_criterion( const std::string &fromcode, const std::string &tocode )
{
    criterion *c=(criterion *) new_criterion();
    c->type=CRIT_RANGE;
    c->c.code_range.fromcode=copy_string(fromcode.c_str());
    c->c.code_range.tocode=copy_string(tocode.c_str());
    return c;
}

static bool code_range_criterion_match( criterion *c, station *stn )
{
    if( stncodecmp(stn->Code,c->c.code_range.fromcode) >= 0 &&
        stncodecmp(stn->Code,c->c.code_range.tocode) <= 0 ) return true;
    return false;
}

static void delete_code_range_criterion( criterion *c )
{
    check_free( c->c.code_range.fromcode );
    check_free( c->c.code_range.tocode );
}

/*-----------------------------------------------------------------------*/

static criterion *new_polygon_criterion( void *polygon, coordsys *cs, coord_conversion *conv, bool isgeodetic, bool inside )
{
    criterion *c=(criterion *) new_criterion();
    c->type = CRIT_POLYGON;
    c->c.polygon.polygon=polygon;
    c->c.polygon.cs=cs;
    c->c.polygon.conv=conv;
    c->c.polygon.inside=inside;
    c->c.polygon.isgeodetic=isgeodetic;
    return c;
}

static bool polygon_criterion_match( criterion *c, station *stn )
{
    bool isinside;
    double lon=stn->ELon;
    double lat=stn->ELat;
    if( c->c.polygon.conv ) 
    {
        double llh[3];
        llh[CRD_LAT]=lat;
        llh[CRD_LON]=lon;
        llh[CRD_HGT]=stn->OHgt+stn->GUnd;
        convert_coords( c->c.polygon.conv, llh, NULL, llh, NULL );
        if( c->c.polygon.isgeodetic )
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
    isinside=polygon_contains_point( c->c.polygon.polygon, lon, lat ) ? 1 : 0;
    bool ok=isinside == c->c.polygon.inside;
    return ok;
}

static void delete_polygon_criterion( criterion *c )
{
    if( c->c.polygon.polygon ) delete_polygon( c->c.polygon.polygon );
    if( c->c.polygon.conv ) check_free( c->c.polygon.conv );
    if( c->c.polygon.cs ) delete c->c.polygon.cs;
    c->c.polygon.polygon = nullptr;
    c->c.polygon.conv = nullptr;
}

/*-----------------------------------------------------------------------*/

static criterion *new_classification_criterion( int class_id, int value_id )
{
    criterion *c=(criterion *) new_criterion();
    c->type=CRIT_CLSF;
    c->c.classification.class_id=class_id;
    c->c.classification.value_id=value_id;
    return c;
}

static bool classification_criterion_match( criterion *c, station *stn )
{
    return get_station_class( stn, c->c.classification.class_id ) == c->c.classification.value_id;
}

static void delete_classification_criterion( criterion * )
{
}

/*-----------------------------------------------------------------------*/

static bool criterion_match( criterion *c, station *stn )
{
    bool ok=false;
    switch( c->type )
    {
        case CRIT_ALL: ok=true; break;
        case CRIT_CODE: ok=code_criterion_match( c, stn ); break;
        case CRIT_MATCH: ok=code_match_criterion_match( c, stn ); break;
        case CRIT_RANGE: ok=code_range_criterion_match( c, stn ); break;
        case CRIT_POLYGON: ok=polygon_criterion_match( c, stn ); break;
        case CRIT_CLSF: ok=classification_criterion_match( c, stn ); break;
    };
    return ok;
}

static void delete_criterion( criterion *c )
{
    switch( c->type )
    {
        case CRIT_ALL: break;
        case CRIT_CODE: delete_code_criterion( c ); break;
        case CRIT_MATCH: delete_code_match_criterion( c ); break;
        case CRIT_RANGE: delete_code_range_criterion( c ); break;
        case CRIT_POLYGON: delete_polygon_criterion( c ); break;
        case CRIT_CLSF: delete_classification_criterion( c ); break;
    };
    check_free(c);
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
    if( maxstn > 0 ) sc->cache=new_criteria_cache(maxstn);
}

static void set_station_criteria_source( station_criteria *sc, const char *file )
{
    // The prefix is identical for every entry, so comparing the whole
    // string is equivalent to the original's "skip the prefix, compare the
    // rest against file" - no pointer arithmetic needed.
    std::string search = std::string(source_prefix) + file;
    for( const std::string &source : sc->sources )
    {
        if( source == search )
        {
            sc->cur_source = source;
            return;
        }
    }
    sc->sources.push_front( std::move(search) );
    sc->cur_source = sc->sources.front();
}

static bool station_criteria_source_used( station_criteria *sc, int maxstack, const char *file )
{
    for( criterion *c=sc->first; c; c=c->next )
    {
        if( c->stacklevel < maxstack 
                && c->source != default_source 
                && strcmp(c->source+strlen(source_prefix),file)==0) return true;
    }
    return false;
}


static void delete_all_station_criteria( station_criteria *sc )
{
    while( sc->first )
    {
        criterion *cur=sc->first;
        sc->first=cur->next;
        delete_criterion( cur );
    }
    sc->first=nullptr;
    sc->last=nullptr;
    sc->all_code_criteria=true;
}

static void add_station_criterion( station_criteria *sc, criterion *c )
{
    /* Optimisation for simple list of codes */
    if( (c->type != CRIT_CODE && c->type != CRIT_FRAME ) ||
        (c->crit_operator != CRIT_OP_LINE && c->crit_operator != CRIT_OP_OR) )
                sc->all_code_criteria=false;

    if( sc->cur_source ) c->source=sc->cur_source->data();
    if( sc->last )
    {
        sc->last->next=c;
        sc->last=c;
    }
    else
    {
        sc->first=c;
        sc->last=c;
    }
}

static void skip_criterion_frame( criterion **c, int stacklevel, int op )
{
    criterion *crt=*c;
    while( crt && 
            (crt->stacklevel > stacklevel ||
            (crt->stacklevel == stacklevel && crt->crit_operator >= op)) )
    {
        crt=crt->next;
    }
    (*c)=crt;
}

static bool criterion_frame_match( criterion **c, station *stn )
{
    criterion *crt=*c;
    if( ! crt ) return false;
    bool invert=crt->crit_operator == CRIT_OP_EXCEPT;
    bool match=invert;
    int stacklevel=crt->stacklevel;
    while( crt && crt->stacklevel >= stacklevel )
    {
        unsigned char op=crt->crit_operator;
        /* Can we skip this frame */
        if( (op == CRIT_OP_LINE && match) ||
                (op == CRIT_OP_EXCEPT && ! match) ||
                (op == CRIT_OP_AND && ! match) ||
                (op == CRIT_OP_OR && match) )
        {
            skip_criterion_frame( &crt, stacklevel, op );
            continue;
        }
        if( crt->type == CRIT_FRAME )
        {
            if( crt->next && crt->next->stacklevel > crt->stacklevel )
            {
                crt=crt->next;
                match=criterion_frame_match( &crt, stn );
                if( op == CRIT_OP_EXCEPT ) match = ! match;
            }
        }
        else
        {
            match=criterion_match( crt, stn );
            crt=crt->next;
        }
    }
    *c=crt;
    return match;
}

bool station_criteria_match( void *psc, station *stn )
{
    station_criteria *sc=(station_criteria *) psc;
    unsigned int status=CRIT_STATUS_UNKNOWN;
    if( sc->cache ) status=check_criteria_cache(sc->cache, stn->id);
    if( status == CRIT_STATUS_UNKNOWN )
    {
        /* Match if any criteria match, except may be undone by reverse criteria */
        criterion *crt=sc->first;
        status=criterion_frame_match( &crt, stn ) ? CRIT_STATUS_PASS : CRIT_STATUS_FAIL;
        if( sc->cache ) set_criteria_cache(sc->cache, stn->id, status );
    }
    return status == CRIT_STATUS_PASS;
}

void delete_station_criteria( void *psc )
{
    station_criteria *sc=(station_criteria *) psc;
    delete_all_station_criteria( sc );
    if( sc->cache ) delete_criteria_cache( sc->cache );
    sc->cache=nullptr;
    delete sc;
}

void apply_station_criteria_to_network( void *psc, network *nw, 
    void *data, void (*function)( station *stn, void *data ))
{
    station_criteria *sc=(station_criteria *) psc;
    /* Optimisation for simple criteria - no need to process entire list */
    if( sc->all_code_criteria )
    {
        int id;
        for( criterion *c=sc->first; c != nullptr; c=c->next )
        {
            if( c->type != CRIT_CODE ) continue;
            id=find_station(nw, c->c.code.code );
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
    char errmess[150];
    int sts=OK;
    for( criterion *c=sc->first; c != nullptr; c=c->next )
    {
        if( c->type != CRIT_CODE ) continue;
        if( c->c.code.missing_error == OK ) continue;
        if( c->c.code.id == CRIT_ID_UNKNOWN )
        {
            int id=find_station(nw, c->c.code.code );
            if( id ) 
            {
                c->c.code.id=id;
                continue;
            }
            c->c.code.id=CRIT_ID_MISSING;
        }
        if( c->c.code.id != CRIT_ID_MISSING ) continue;
        sprintf(errmess,"Invalid station %.20s in %.80s",c->c.code.code,
                c->source ? c->source : "station_list");
        handle_error(c->c.code.missing_error,errmess,NULL);
        if( sts != INVALID_DATA ) sts=c->c.code.missing_error;
    }
    return sts;
}

/*-----------------------------------------------------------------------*/


static int compile_station_criteria1( station_criteria *sc, network *nw, std::string_view select, const char *basefile, unsigned char stacklevel );

static int compile_station_list_file_criteria( station_criteria *sc, network *nw, const char *file, const char *basefile, unsigned char stacklevel )
{
    std::optional<std::string> spec;
    FILE *list_file;
    char buf[2048];
    int sts = OK;

    std::optional<std::string> base = basefile ? std::optional<std::string>(basefile) : std::nullopt;
    spec = find_file( file, DFLTSTLIST_EXT, base, FF_TRYLOCAL, "" );
    list_file = NULL;
    if( spec ) list_file = fopen( spec->c_str(), "r" );

    if( !list_file )
    {
        char errmess[40+MAX_FILENAME_LEN];
        sprintf(errmess,"Cannot open station list file %.*s\n",MAX_FILENAME_LEN,file);
        handle_error( INVALID_DATA, errmess, NULL  );
        return INVALID_DATA;
    }
    record_filename( spec->c_str(), "station_list_file" );

    skip_utf8_bom(list_file);

    set_station_criteria_source( sc, file );
    while( sts==OK && fgets(buf,2048,list_file) )
    {
        char *b = buf;
        while( *b && ISSPACE(*b) ) b++;
        if( ! *b || *b == COMMENT_CHAR ) continue;
        sts=compile_station_criteria1(sc, nw,b,file,stacklevel);
    }
    return sts;
}

static int compile_station_criteria1( station_criteria *sc, network *nw, std::string_view select, const char *basefile, unsigned char stacklevel )
{
    const char *src;
    char errmess[200];
    int missing_error=sc->cur_missing_error;
    int sts=OK;
    unsigned char curop=CRIT_OP_LINE;
    const int baselevel=stacklevel;

    criterion *c;

    errmess[0] = 0;
    src=sc->cur_source ? sc->cur_source->data() : default_source;

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
            unsigned char op=CRIT_OP_DEFAULT;
            if( boost::algorithm::iequals( *field, "except" ) ) op=CRIT_OP_EXCEPT;
            else if( boost::algorithm::iequals( *field, "and" ) ) op=CRIT_OP_AND;
            else if( boost::algorithm::iequals( *field, "or" ) ) op=CRIT_OP_OR;
            if( op != CRIT_OP_DEFAULT )
            {
                if( curop != CRIT_OP_DEFAULT )
                {
                    std::string fieldText(*field);
                    sprintf(errmess,"\"%s\" out of place in %.100s",fieldText.c_str(),src);
                    break;
                }
                /* Except increments stack level by one so can evaluate except clause on stack
                 * before inverting status */
                if( op == CRIT_OP_EXCEPT )
                {
                    add_station_criterion( sc, new_criteria_frame( op, baselevel ));
                    op=CRIT_OP_LINE;
                    stacklevel=baselevel+1;
                }
                curop=op;
                continue;
            }
        }

        /* If this reference a file of station definitions, then process the file.
           Not allowed if this is already in a station list file. */

        if( field->size() > 1 && (*field)[0] == '@' )
        {
            const std::string file( field->substr(1) );
            std::optional<std::string_view> save_src=sc->cur_source;
            /* Add a placeholder for the current operation */
            add_station_criterion( sc, new_criteria_frame( curop, stacklevel ));
            sc->cur_missing_error=missing_error;
            if( station_criteria_source_used( sc, stacklevel, file.c_str() ))
            {
                sprintf(errmess,"Station list file %.100s uses itself",file.c_str());
                break;
            }
            /* Embedded station list increments stack level by 2 to distinguish from
             * except stack level */
            sts=compile_station_list_file_criteria( sc, nw,file.c_str(),basefile,stacklevel+1);
            sc->cur_source=save_src;
            if( sts != OK )
            {
                sprintf(errmess,"Error processing station list file %.100s",file.c_str());
                break;
            }
            curop=CRIT_OP_DEFAULT;
            continue;
        }

        c=nullptr;
        if( boost::algorithm::iequals( *field, "all" ) )
        {
            c=new_all_criterion();
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
                sprintf(errmess,"Invalid \"%s\" option in %s requires coord sys code and wkt file name",fieldText.c_str(),src);
                break;
            }
            const std::string crdsys( *crdsysField );
            const std::string pgnfile( *pgnfileField );
            cs=load_coordsys( crdsys.c_str() );
            if( ! cs )
            {
                sprintf(errmess,"Invalid coordinate system %-20s in \"%s\" option in %s",crdsys.c_str(),std::string(*field).c_str(),src);
                break;
            }

            std::optional<std::string> base = basefile ? std::optional<std::string>(basefile) : std::nullopt;
            std::optional<std::string> spec = find_file( pgnfile, DFLT_WKT_EXT, base, FF_TRYLOCAL, "" );
            if( ! spec )
            {
                sprintf(errmess,"Cannot find WKT polygon file %.50s in %s",
                        pgnfile.c_str(),src);
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
                conv=(coord_conversion *)check_malloc( sizeof (coord_conversion) );
                if( define_coord_conversion_epoch( conv, nw->geosys, cs, DEFAULT_CRDSYS_EPOCH ) != OK )
                {
                    sprintf(errmess,"Cannot use WKT coordinate system %.20s in %s option in %s",
                            crdsys.c_str(),std::string(*field).c_str(),src);
                    check_free( conv );
                    delete cs;
                    break;
                }
            }

            void *const pgn=read_polygon_wkt( spec->c_str(), isgeo);
            if( ! pgn )
            {
                sprintf(errmess,"Cannot read WKT polygon file %.50s in %s",
                        pgnfile.c_str(),src);
                if( conv ) check_free( conv );
                if( cs ) delete cs;
                break;
            }
            record_filename(spec->c_str(),"wkt_polygon_definition");
            c=new_polygon_criterion( pgn, cs, conv, isgeo, inside );
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
            size_t pos=0;
            while( pos < values.size() )
            {
                const auto slashPos=values.find('/',pos);
                const auto valueEnd = slashPos==std::string_view::npos ? values.size() : slashPos;
                const std::string value( values.substr(pos,valueEnd-pos) );
                const int value_id = class_id ? nw->class_value_id( class_id, value, 0 ) : CLASS_VALUE_NOT_DEFINED;
                if( value_id != CLASS_VALUE_NOT_DEFINED )
                {
                    c=new_classification_criterion( class_id, value_id );
                    if( curop == CRIT_OP_DEFAULT ) curop=CRIT_OP_OR;
                    c->crit_operator=curop;
                    c->stacklevel=stacklevel;
                    add_station_criterion( sc, c );
                    curop=CRIT_OP_OR;
                }
                pos = slashPos==std::string_view::npos ? values.size() : slashPos+1;
            }
            c=nullptr;
            curop=CRIT_OP_DEFAULT;
            continue;
        }

        /* Is it matched as a range? */

        else if( (*field)[0] != '\\' && field->find('-',1) != std::string_view::npos )
        {
            const auto dashPos=field->find('-',1);
            std::string fromCode( field->substr(0,dashPos) );
            std::string toCode( field->substr(dashPos+1) );
            c=new_code_range_criterion( fromCode, toCode );
        }
        else if( (*field)[0] != '\\' && has_wildcard( std::string(*field).c_str() ) )
        {
            std::string fieldStr(*field);
            c=new_code_match_criterion(fieldStr);
        }
        else
        {
            /* Allow \ escape on station names matching keywords */

            std::string_view codeField=*field;
            if( codeField.front() == '\\' ) codeField.remove_prefix(1);
            if( codeField.empty() ) continue;
            std::string codeStr(codeField);
            c=new_code_criterion(codeStr, missing_error);
        }

        if( c )
        {
            if( curop == CRIT_OP_DEFAULT ) curop=CRIT_OP_OR;
            c->crit_operator=curop;
            c->stacklevel=stacklevel;
            add_station_criterion( sc, c );
            curop=CRIT_OP_DEFAULT;
        }
    }
    if( ! errmess[0] && curop != CRIT_OP_DEFAULT )
    {
         sprintf(errmess,"Station list cannot end with and, or, or except in %.100s",src);
    }
    if( errmess[0] )
    {
        sts=INVALID_DATA;
        handle_error(INVALID_DATA,errmess,NULL);
    }
    return sts;
}

int compile_station_criteria( void *psc, network *nw, const std::string &select, const std::string &basefile )
{
    int sts;
    station_criteria *sc=(station_criteria *) psc;
    sts=compile_station_criteria1( sc, nw, select, basefile.c_str(), 0 );
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
