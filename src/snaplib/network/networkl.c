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
#include <ctype.h>

#include "network/network.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/fileutil.h"
#include "util/errdef.h"
#include "util/polygon.h"
#include "util/pi.h"


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

typedef struct code_criterion_s
{
    char *code;
    int id;
    int missing_error;
} code_criterion;

typedef struct criteria_cache_s
{
    unsigned char *cache;
    int maxcache;
} criteria_cache;

typedef struct code_match_criterion_s
{
    char *code;
    int matchlen;
} code_match_criterion;

typedef struct code_range_criterion_s
{
    char *fromcode;
    char *tocode;
} code_range_criterion;

typedef struct polygon_criterion_s
{
    void *polygon;
    coordsys *cs;
    coord_conversion *conv;
    bool isgeodetic;
    bool inside;
} polygon_criterion;

typedef struct classification_criterion_s
{
    int class_id;
    int value_id;
} classification_criterion;

typedef struct criterion_s
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
    struct criterion_s *next;
} criterion;

typedef struct station_criteria_source_s
{
    const char *source;
    struct station_criteria_source_s *next;
} station_criteria_source;

typedef struct station_criteria_s
{
    criterion *first;
    criterion *last;
    criteria_cache *cache;
    bool all_code_criteria;
    station_criteria_source *sources;
    station_criteria_source *cur_source;
    int cur_missing_error;
} station_criteria;

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

static criterion *new_code_criterion( char *code, int missing_error )
{
    criterion *c=new_criterion();
    c->type=CRIT_CODE;
    c->c.code.code=copy_string(code);
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

static criterion *new_code_match_criterion( char *code )
{
    criterion *c=(criterion *) new_criterion();
    c->type=CRIT_MATCH;
    c->c.code_match.code=copy_string(code);
    c->c.code_match.matchlen=strlen(code);
    return c;
}

static bool code_match_criterion_match( criterion *c, station *stn )
{
    return _strnicmp( stn->Code, c->c.code_match.code, c->c.code_match.matchlen) == 0 ? true : false;
}

static void delete_code_match_criterion( criterion *c )
{
    check_free( c->c.code_match.code );
    c->c.code_match.code=nullptr;
}

/*-----------------------------------------------------------------------*/

static criterion *new_code_range_criterion( char *fromcode, char *tocode )
{
    criterion *c=(criterion *) new_criterion();
    c->type=CRIT_RANGE;
    c->c.code_range.fromcode=copy_string(fromcode);
    c->c.code_range.tocode=copy_string(tocode);
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
    if( c->c.polygon.cs ) delete_coordsys( c->c.polygon.cs );
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

static void delete_classification_criterion( criterion *c )
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
    station_criteria *sc=(station_criteria *) check_malloc( sizeof(station_criteria) );
    sc->first = nullptr;
    sc->last = nullptr;
    sc->cache = nullptr;
    sc->sources = nullptr;
    sc->cur_source = nullptr;
    sc->cur_missing_error=INVALID_DATA;
    sc->all_code_criteria=true;
    return (void *) sc;
}

void setup_station_criteria_cache( void *psc, int maxstn )
{
    station_criteria *sc=(station_criteria *) psc;
    if( sc->cache ) return;
    if( maxstn > 0 ) sc->cache=new_criteria_cache(maxstn);
}

static void set_station_criteria_source( station_criteria *sc, char *file )
{
    station_criteria_source *src=sc->sources;
    int prefix_len=strlen(source_prefix);
    char *srcfile;
    while( src )
    {
        if( strcmp(src->source + prefix_len, file) == 0 ) 
        {
            sc->cur_source=src;
            return;
        }
        src=src->next;
    }
    src=(station_criteria_source *) check_malloc( sizeof(station_criteria_source) 
         + prefix_len + strlen(file) + 1 );
    srcfile=((char *)src) + sizeof(station_criteria_source);

    strcpy( srcfile, source_prefix );
    strcpy( srcfile+prefix_len, file );
    src->source=srcfile;
    src->next=sc->sources;
    sc->sources=src;
    sc->cur_source=src;
}

static bool station_criteria_source_used( station_criteria *sc, int maxstack, char *file )
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

    if( sc->cur_source ) c->source=sc->cur_source->source;
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
    while( sc->sources )
    {
        station_criteria_source *src=sc->sources;
        sc->sources=src->next;
        check_free( src );
    }
    check_free( sc );
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


static int compile_station_criteria1( station_criteria *sc, network *nw, char *select, char *basefile, unsigned char stacklevel );

static int compile_station_list_file_criteria( station_criteria *sc, network *nw, char *file, char *basefile, unsigned char stacklevel )
{
    const char *spec;
    FILE *list_file;
    char buf[2048];
    int sts = OK;

    spec = find_file( file,DFLTSTLIST_EXT,basefile,1,0);
    list_file = NULL;
    if( spec ) list_file = fopen( spec, "r" );

    if( !list_file )
    {
        char errmess[80];
        sprintf(errmess,"Cannot open station list file %.40s\n",file);
        handle_error( INVALID_DATA, errmess, NULL  );
        return INVALID_DATA;
    }

    skip_utf8_bom(list_file);

    set_station_criteria_source( sc, file );
    while( sts==OK && fgets(buf,2048,list_file) )
    {
        char *b = buf;
        while( *b && isspace(*b) ) b++;
        if( ! *b || *b == COMMENT_CHAR ) continue;
        sts=compile_station_criteria1(sc, nw,b,file,stacklevel);
    }
    return sts;
}

static int compile_station_criteria1( station_criteria *sc, network *nw, char *select, char *basefile, unsigned char stacklevel )
{
    char *field;
    char *s = select;
    char *delim;
    const char *src;
    char errmess[200];
    int missing_error=sc->cur_missing_error;
    int sts;
    unsigned char curop=CRIT_OP_LINE;
    int baselevel=stacklevel;

    criterion *c;

    errmess[0] = 0;
    src=sc->cur_source ? sc->cur_source->source : default_source;

    sts=OK;
    while( (field=next_field(&s)) )
    {

        /* Missing station options */

        if( _stricmp( field, "ignore_missing" ) == 0 )
        {
            missing_error=OK;
            continue;
        }

        if( _stricmp( field, "warn_missing" ) == 0 )
        {
            missing_error=INFO_ERROR;
            continue;
        }

        if( _stricmp( field, "fail_missing" ) == 0 )
        {
            missing_error=INVALID_DATA;
            continue;
        }

        /* Operators */

        {
            unsigned char op=CRIT_OP_DEFAULT;
            if( _stricmp( field, "except" ) == 0 ) op=CRIT_OP_EXCEPT;
            else if( _stricmp( field, "and" ) == 0 ) op=CRIT_OP_AND;
            else if( _stricmp( field, "or" ) == 0 ) op=CRIT_OP_OR;
            if( op != CRIT_OP_DEFAULT )
            {
                if( curop != CRIT_OP_DEFAULT )
                {
                    sprintf(errmess,"\"%s\" out of place in %.100s",field,src);
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

        if( field[0] == '@' && field[1] )
        {
            char *file=field+1;
            station_criteria_source *save_src=sc->cur_source;
            /* Add a placeholder for the current operation */
            add_station_criterion( sc, new_criteria_frame( curop, stacklevel ));
            sc->cur_missing_error=missing_error;
            if( station_criteria_source_used( sc, stacklevel, file ))
            {
                sprintf(errmess,"Station list file %.100s uses itself",file);
                break;
            }
            /* Embedded station list increments stack level by 2 to distinguish from
             * except stack level */
            sts=compile_station_list_file_criteria( sc, nw,file,basefile,stacklevel+1);
            sc->cur_source=save_src;
            if( sts != OK ) 
            {
                sprintf(errmess,"Error processing station list file %.100s",file);
                break;
            }
            curop=CRIT_OP_DEFAULT;
            continue;
        }

        c=nullptr;
        if( _stricmp( field, "all" ) == 0 )
        {
            c=new_all_criterion();
        }
        else if( _stricmp( field, "inside" ) == 0 || _stricmp( field, "outside" ) == 0 )
        {
            char *crdsys;
            char *pgnfile;
            const char *spec;
            void *pgn=0;
            bool isgeo=true;
            coordsys *cs;
            coord_conversion *conv=nullptr;

            bool inside=_stricmp(field,"inside") == 0 ? true : false;
            crdsys=next_field(&s);
            pgnfile=next_field(&s);
            if( ! pgnfile )
            {
                sprintf(errmess,"Invalid \"%s\" option in %s requires coord sys code and wkt file name",field,src);
                break;
            }
            cs=load_coordsys( crdsys );
            if( ! cs )
            {
                sprintf(errmess,"Invalid coordinate system %-20s in \"%s\" option in %s",crdsys,field,src);
                break;
            }

            spec = find_file( pgnfile,DFLT_WKT_EXT,basefile,1,0);
            if( ! spec )
            {
                sprintf(errmess,"Cannot find WKT polygon file %.50s in %s",
                        pgnfile,src);
                break;
            }
            
            if( identical_coordinate_systems( cs, nw->geosys ) )
            {
                delete_coordsys( cs );
                cs=nullptr;
            }
            else 
            {
                isgeo=is_geodetic(cs);
                conv=(coord_conversion *)check_malloc( sizeof (coord_conversion) );
                if( define_coord_conversion_epoch( conv, nw->geosys, cs, DEFAULT_CRDSYS_EPOCH ) != OK )
                {
                    sprintf(errmess,"Cannot use WKT coordinate system %.20s in %s option in %s",
                            crdsys,field,src);
                    check_free( conv );
                    delete_coordsys( cs );
                    break;
                }
            }

            pgn=read_polygon_wkt( spec, isgeo);
            if( ! pgn )
            {
                sprintf(errmess,"Cannot read WKT polygon file %.50s in %s",
                        pgnfile,src);
                if( conv ) check_free( conv );
                if( cs ) delete_coordsys( cs );
                break;
            }
            c=new_polygon_criterion( pgn, cs, conv, isgeo, inside );
        }


        /* If this is a classification criteria */

        else if( field[0] != '\\' && (delim=strchr(field+1,'=')) )
        {
            int class_id = 0;
            *delim = 0;
            class_id = network_class_id( nw, field, 0 );
            *delim='=';
            delim++;
            while( *delim )
            {
                char *value=delim;
                char delchr;
                int value_id = CLASS_VALUE_NOT_DEFINED;
                while( *delim && *delim != '/' ) delim++;
                delchr=*delim;
                *delim=0;
                if( class_id )
                {
                    value_id = network_class_value_id( nw, class_id, value, 0 );
                }
                *delim=delchr;
                if( *delim ) delim++;
                if( value_id != CLASS_VALUE_NOT_DEFINED )
                {
                    c=new_classification_criterion( class_id, value_id );
                    if( curop == CRIT_OP_DEFAULT ) curop=CRIT_OP_OR;
                    c->crit_operator=curop;
                    c->stacklevel=stacklevel;
                    add_station_criterion( sc, c );
                    curop=CRIT_OP_OR;
                }
            }
            c=nullptr;
            curop=CRIT_OP_DEFAULT;
            continue;
        }

        /* Is it matched as a range? */

        else if( field[0] != '\\' && (delim=strchr(field+1,'-')) )
        {
            *delim = 0;
            c=new_code_range_criterion( field, delim+1 );
            *delim='-';
        }
        else
        {
            /* Allow \ escape on station names matching keywords */

            if( field[0] == '\\' ) field++;
            if( ! field[0] ) continue;

            /* Does it end with a wildcard */

            else if( strlen(field) > 0 && strlen(field) < STNCODELEN-1 && field[strlen(field)-1]=='*' )
            {
                delim=field+strlen(field)-1;
                *delim=0;
                c=new_code_match_criterion(field);
                *delim='*';
            }
            else
            {
                c=new_code_criterion(field, missing_error);
            }
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

int compile_station_criteria( void *psc, network *nw, const char *select, char *basefile )
{
    int sts;
    station_criteria *sc=(station_criteria *) psc;
    char *sel = copy_string(select);
    sts=compile_station_criteria1( sc, nw, sel, basefile, 0 );
    check_free(sel);
    return sts;
}


int process_selected_stations( network *nw, const char *select, char *basefile,
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
