#include "snapconfig.h"
/*


This code manages "general" parameters - meaning any parameters in the
adjustment other than the station related parameters (ie coordinates).

The routines provide the following capabilities:

   Maintaining a list of named parameters
   Selecting parameters to be used in adjustments, and automatically
      rejecting any parameters which are not referenced in any observations
   Specifying that two parameters are to be constrained as identical.
   Specifying and selecting parameters using a (very limited) wildcard which
      matches any parameter with a specified prefix.

To do this it initially builds two lists

   1) A list of parameters
   2) A list of specifications.  The specifications, defined in the
      prm_action structure, include defining parameter values and defining
      identical parameters, either explicitely or as wild cards.
After the lists are complete (when parameters are first used), the
specifications are applied to the list of parameters.

The procedure requires the following sequence of calls

   define_param     create a parameter  |  wildcard_param_value
                                        |  wildcard_param_match
   define_param_value  } in any order   |
   define_param_match  }                |
   flag_param_used     }                |

       ------------------------------------------------

              init_param_rowno

	      ....


*/


/*
   $Log: genparam.c,v $
   Revision 1.2  1999/05/18 14:38:30  ccrook
   Fixing minor bug.

   Revision 1.1  1995/12/22 17:43:38  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <cstddef>
#include <algorithm>
#include <array>
#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <boost/numeric/conversion/cast.hpp>

#include "util/dstring.h"
#include "snap/genparam.h"
#include "util/linklist.h"
#include "util/binfile.h"
#include "util/errdef.h"
#include "util/fieldscanner.hpp"
#include "util/wildcard.h"

struct prm_action
{
    unsigned char action;
    param *prm = nullptr;         ///< the parameter acted on, unless PA_WILDCARD is set
    std::string wildcard;         ///< pattern for the parameters acted on, if PA_WILDCARD is set
    double value = 0.0;           ///< the value assigned, for PA_SET and PA_ADJ
    param *matchprm = nullptr;    ///< the parameter matched to, for PA_MATCH

    /// Sets or adjusts one parameter to a value.
    prm_action( unsigned char action, param *prm, double value )
        : action( action ), prm( prm ), value( value ) {}

    /// Sets or adjusts every parameter matching a name pattern to a value.
    prm_action( unsigned char action, std::string_view pattern, double value )
        : action( action ), wildcard( pattern ), value( value ) {}

    /// Makes one parameter identical to another.
    prm_action( unsigned char action, param *prm, param *matchprm )
        : action( action ), prm( prm ), matchprm( matchprm ) {}

    /// Makes every parameter matching a name pattern identical to another parameter.
    prm_action( unsigned char action, std::string_view pattern, param *matchprm )
        : action( action ), wildcard( pattern ), matchprm( matchprm ) {}
};

#define PA_SET       1
#define PA_ADJ       2
#define PA_MATCH     3
#define PA_WILDCARD  8

/// The parameters in the order they were defined. The parameter id is the index plus one.
static std::vector<std::unique_ptr<param>> prmlist;

/// The parameter ids in case insensitive name order.
static std::vector<int> srtlist;

static std::vector<prm_action> action_list;

static constexpr std::array<std::string_view,4> coefprefix =
{
    "Refr coef ",
    "Scale error ",
    "Bearing error ",
    "Systematic: "
};

static int prefixlen[] =
{
    10,
    12,
    14,
    12
};
#define COEFLEN 20

static double default_refcoef = DEFAULT_REFCOEF;

void set_default_refcoef( double value )
{
    default_refcoef = value;
}

/*==============================================================*/
/* Refraction coefficient specific bits.                        */

int param_count( void )
{
    return boost::numeric_cast<int>( prmlist.size() );
}

static unsigned int hash( std::string_view name )
{
    unsigned int h = 0;
    for( const char ch : name ) h = (h<<3) + h + ch;
    return h;
}

static param *reset_param( param *p, double value, int adjust )
{
    p->value = value;
    p->covar = 0.0;
    if( adjust )
    {
        p->flags |= PRM_ADJUST;
    }
    else
    {
        p->flags &= ~PRM_ADJUST;
    }
    return p;
}

static void sort_param( int n )
{
    int np;

    np = srtlist[n];
    const std::string_view name = param_name( np );
    while( n > 0 )
    {
        if( compare_ignoring_case( param_name(srtlist[n-1]), name ) <= 0 ) break;
        srtlist[n] = srtlist[n-1];
        n--;
    }
    srtlist[n] = np;
}

int define_param( std::string_view name, double value, int adjust )
{
    int p = find_param( name );
    if( !p )
    {
        auto prm = std::make_unique<param>();
        prm->name = name;
        prm->hash = hash(name);
        prm->identical = 0;
        prm->flags = 0;

        prmlist.push_back( std::move( prm ) );
        p = param_count();
        srtlist.push_back( p );
        sort_param( p-1 );
    }
    reset_param( prmlist[p-1].get(), value, adjust );
    return p;
}

void flag_param_used( int p )
{
    if( p ) prmlist[p-1]->flags |= PRM_USED;
}

void flag_param_listed( int p )
{
    if( p ) prmlist[p-1]->flags |= PRM_LISTED;
}

static int get_param_id( const param *p )
{
    for( int i = 0; i < param_count(); i++ )
    {
        if( prmlist[i].get() == p ) return i+1;
    }
    return 0;
}

param * param_from_id( int pid )
{
    if( pid <= 0 || pid > param_count() ) return nullptr;
    return prmlist[pid - 1].get();
}

static param *identical_param_ptr( param *p )
{
    if( !p->identical ) return p;
    return prmlist[p->identical-1].get();
}

int sorted_param_id( int n )
{
    if( n <= 0 || n > param_count() ) return 0;
    return srtlist[n-1];
}

static void merge_params( param *p1, param *p2 )
{
    if( prmlist.empty() ) return;

    p1 = identical_param_ptr( p1 );
    p2 = identical_param_ptr( p2 );

    if( p1 == p2 ) return;

    p1->value = p2->value;

    p2->flags |= p1->flags & PRM_USED;
    p1->flags = p2->flags;

    if( _stricmp(p1->name.c_str(), p2->name.c_str()) > 0  )
    {
        param *ptmp;
        ptmp = p1;
        p1 = p2;
        p2 = ptmp;
    }

    p2->identical = get_param_id( p1 );
    const int p2id = get_param_id( p2 );
    for( const std::unique_ptr<param> &prm : prmlist )
    {
        if( prm->identical == p2id )
            prm->identical = p2->identical;
    }

}

int find_param( std::string_view name )
{
    const unsigned int hashedname = hash(name);
    const auto found = std::find_if( prmlist.begin(), prmlist.end(),
        [&]( const std::unique_ptr<param> &prm )
        {
            return hashedname == prm->hash && prm->name == name;
        } );
    return found == prmlist.end() ? 0 : boost::numeric_cast<int>( found - prmlist.begin() ) + 1;
}

double param_value( int p )
{
    return prmlist[p-1]->value;
}

void update_param_value( int pid, double value, double var )
{
    param *p;
    p = param_from_id( pid );
    if( p )
    {
        p->value = value;
        p->covar = var;
    }
}

std::string_view param_name( int p )
{
    return p ? std::string_view( prmlist[p-1]->name ) : std::string_view( "" );
}


static void do_action( const prm_action &pa )
{
    switch( pa.action )
    {
    case PA_SET:   reset_param( pa.prm, pa.value, 0 ); break;
    case PA_ADJ:   reset_param( pa.prm, pa.value, 1 ); break;
    case PA_MATCH: merge_params( pa.prm, pa.matchprm ); break;
    }
}

static void do_prm_actions( void )
{
    for( const prm_action &pa : action_list )
    {
        if( pa.action & PA_WILDCARD )
        {
            prm_action apa = pa;
            apa.action &= ~PA_WILDCARD;
            for( const std::unique_ptr<param> &p : prmlist )
            {
                if( ! wildcard_match(pa.wildcard,p->name) ) continue;
                apa.prm = p.get();
                do_action( apa );
            }
        }
        else
        {
            do_action( pa );
        }
    }
}

static void merge_common_params( void )
{
    for( const int pid : srtlist )
    {
        param *p = param_from_id(pid);
        param *ip = identical_param_ptr(p);
        if( ip != identical_param_ptr(ip) )
        {
            merge_params( p, identical_param_ptr(ip) );
        }
    }
}

std::optional<std::string> find_param_row( const int row )
{
    const auto found = std::find_if( prmlist.begin(), prmlist.end(),
        [row]( const std::unique_ptr<param> &p ) { return p->rowno == row; } );
    if( found == prmlist.end() ) return std::nullopt;
    return (*found)->name;
}


int param_rowno( int pid )
{
    param *p;
    if( !pid ) return 0;
    p = identical_param_ptr(prmlist[pid-1].get());
    if( !p ) return 0;
    return p->rowno;
}

int identical_param( int pid )
{
    if( !pid ) return 0;
    const param *p = prmlist[pid-1].get();
    return p ? p->identical : 0;
}

void define_param_value( int pid, double value, int adjust )
{
    action_list.emplace_back( adjust ? PA_ADJ : PA_SET, prmlist[pid-1].get(), value );
}

void wildcard_param_value( std::string_view name, double value, int adjust )
{
    action_list.emplace_back( (adjust ? PA_ADJ : PA_SET) | PA_WILDCARD, name, value );
}

void define_param_match( int pid1, int pid2 )
{
    action_list.emplace_back( PA_MATCH, prmlist[pid1-1].get(), prmlist[pid2-1].get() );
}

void wildcard_param_match( std::string_view name, int pid )
{
    action_list.emplace_back( PA_MATCH | PA_WILDCARD, name, prmlist[pid-1].get() );
}


int init_param_rowno( int nextprm )
{
    if( prmlist.empty() ) return nextprm;

    /* Sort needed for merging parameters to word correctly */
    /* Apply parameter constraints, etc */

    do_prm_actions();
    merge_common_params();

    for( const int pid : srtlist )
    {
        param *p = param_from_id(pid);
        p->rowno = 0;
        if( p->identical ) continue;
        if( !(p->flags & PRM_ADJUST ) ) continue;
        if( !(p->flags & PRM_USED) )
        {
            char errmsg[80];
            sprintf(errmsg,"Parameter %.40s cannot be calculated",p->name.c_str());
            handle_error( WARNING_ERROR, errmsg, NO_MESSAGE );
            continue;
        }
        p->rowno = nextprm++;
    }
    return nextprm;
}

void clear_param_list( void )
{
    action_list.clear();
    prmlist.clear();
    srtlist.clear();
}


// Single source of truth for the fixed-width on-disk param layout, excluding
// `name` (handled separately via dump_string/reload_string, since it's a
// variable-length std::string). `name` is the struct's first field, so it
// sits entirely before this table's first entry rather than in the middle -
// unlike rftrndmp.cpp's table, there's no interior gap here to skip when
// checking contiguity below.
//
// hash and flags get an explicitly unsigned kind (UInt32/UInt8) rather than
// Int32/Int8: converting an unsigned value that doesn't fit the corresponding
// signed type was implementation-defined before C++20. Unsigned-to-unsigned
// conversion has always been fully defined (modulo 2^N), at zero extra cost.
// hash needs the full unsigned range by design - it's a hash code. flags only
// uses 3 of 8 bits today (PRM_ADJUST/PRM_USED/PRM_LISTED, genparam.h), but
// there's no structural guarantee that stays true, so UInt8 removes the
// dependency on that fact rather than relying on it.
//
// NOTE: this table must stay in sync with param's declared fields (see the
// matching note at genparam.h next to the struct) - adding/removing a field in
// one place without the other silently desyncs the on-disk format from the
// struct. param_disk_fields_contiguous() below verifies this at compile time.
//
// Uses DiskField (util/binfile.h) and has external linkage via the `extern`
// declarations in genparam.h - see the comment there. Every entry's count is
// 1 (param has no array fields), unlike station's/rfTransformation's tables.
constexpr DiskField PARAM_DISK_FIELDS[] = {
    { FieldKind::UInt32,  offsetof(param, hash), 1 },
    { FieldKind::Float64, offsetof(param, value), 1 },
    { FieldKind::Float64, offsetof(param, covar), 1 },
    { FieldKind::Int32,   offsetof(param, rowno), 1 },
    { FieldKind::UInt8,   offsetof(param, flags), 1 },
    { FieldKind::Int32,   offsetof(param, identical), 1 },
};
constexpr size_t PARAM_DISK_FIELD_COUNT = sizeof(PARAM_DISK_FIELDS) / sizeof(PARAM_DISK_FIELDS[0]);

// Verifies PARAM_DISK_FIELDS has no gap relative to param's actual memory layout -
// the same rounded-up-to-next-alignment check as survdata_disk_fields_contiguous()
// in bindata.cpp. Every consecutive pair in this table is checked (there's no
// interior exclusion to skip, unlike rftrndmp.cpp's table), and the last tracked
// field (identical) must, by the same rule, reach exactly the end of the struct.
static constexpr bool param_disk_fields_contiguous()
{
    for( size_t i = 0; i + 1 < PARAM_DISK_FIELD_COUNT; ++i )
    {
        const size_t end = PARAM_DISK_FIELDS[i].offset + field_in_memory_size(PARAM_DISK_FIELDS[i].kind);
        const size_t expected_next = round_up(end, field_in_memory_alignment(PARAM_DISK_FIELDS[i+1].kind));
        if( expected_next != PARAM_DISK_FIELDS[i+1].offset ) return false;
    }
    const DiskField &last = PARAM_DISK_FIELDS[PARAM_DISK_FIELD_COUNT-1];
    const size_t last_end = last.offset + field_in_memory_size(last.kind);
    return round_up(last_end, alignof(param)) == sizeof(param);
}
// Runs entirely at compile time, same as survdata_disk_fields_contiguous() in
// bindata.cpp - costs nothing in the compiled binary either way.
static_assert(param_disk_fields_contiguous(),
    "PARAM_DISK_FIELDS has a gap relative to param's actual layout - a field was "
    "likely added, removed, or reordered in genparam.h without updating this table");

// Writes PARAM_DISK_FIELDS in table order through the fixed-width disk-cast
// templates from binfile.h. name is handled separately via dump_string (it's
// a variable-length std::string, out of scope for this fixed-width table) -
// together, this covers every field of param.
static void write_param_fixed_width( const param &p, FILE *f )
{
    for_each_disk_field( p, PARAM_DISK_FIELDS, PARAM_DISK_FIELD_COUNT,
        [f]( FieldKind kind, auto value ) { write_disk_field( f, kind, value ); } );
}

// Mirrors write_param_fixed_width: same table, same iteration order, reading
// into each field in place instead of writing.
static void read_param_fixed_width( FILE *f, param &p )
{
    for_each_disk_field_mutable( p, PARAM_DISK_FIELDS, PARAM_DISK_FIELD_COUNT,
        [f]( FieldKind kind, auto &value ) { read_disk_field( f, kind, value ); } );
}


void dump_parameters( BINARY_FILE *b )
{
    create_section( b, "MISCPARAMS" );
    const int nprm = param_count();
    fwrite( &nprm, sizeof(nprm), 1, b->f );
    for( const std::unique_ptr<param> &p : prmlist )
    {
        write_param_fixed_width( *p, b->f );
        dump_string( p->name, b->f );
    }
    fwrite( srtlist.data(), sizeof(int), srtlist.size(), b->f );
    end_section( b );
}

int reload_parameters( BINARY_FILE *b )
{
    clear_param_list();
    if( find_section( b, "MISCPARAMS" ) != OK ) return MISSING_DATA;
    int nprm = 0;
    if( fread( &nprm, sizeof(nprm), 1, b->f ) != 1 ) return INVALID_DATA;
    if( nprm < 0 ) return INVALID_DATA;

    std::vector<std::unique_ptr<param>> params;
    for( int np = 0; np < nprm; np++ )
    {
        auto p = std::make_unique<param>();
        read_param_fixed_width( b->f, *p );
        p->name = reload_string( b->f );
        if( p->name.empty() ) return INVALID_DATA;
        params.push_back( std::move( p ) );
    }

    std::vector<int> sorted( params.size() );
    if( fread( sorted.data(), sizeof(int), sorted.size(), b->f ) != sorted.size() ) return INVALID_DATA;
    prmlist = std::move( params );
    srtlist = std::move( sorted );

    return check_end_section( b );
}


/// Builds the full parameter name from the prefix for the parameter type and the
/// upper case coefficient name, truncated to fit COEFLEN.
static std::string make_prmname( int type, std::string_view name )
{
    std::string prmname( coefprefix[type] );
    const size_t prefixchars = prmname.size();
    prmname.append( name.substr( 0, COEFLEN-1 ) );
    std::transform( prmname.begin()+prefixchars, prmname.end(), prmname.begin()+prefixchars,
                    []( unsigned char ch ) { return std::toupper( ch ); } );
    return prmname;
}

int get_param( int type, std::string_view name, int create )
{
    const std::string prmname = make_prmname( type, name );

    int prm = find_param( prmname );
    if( create && !prm )
    {
        const double dflt = type == PRM_REFCOEF && compare_ignoring_case(name,"zero") != 0 ? default_refcoef : 0;
        prm = define_param( prmname, dflt, 0 );
    }

    return prm;
}

std::string_view param_type_name( int type, int pid )
{
    const std::string_view name = param_name(pid);
    return name.substr( std::min<size_t>( prefixlen[type], name.size() ) );
}

void configure_param( int type, std::string_view name, double value, int adjust )
{
    if( name.empty() ) return;

    if( has_wildcard(name))
    {
        wildcard_param_value( make_prmname( type, name ), value, adjust );
    }
    else
    {
        const int prm = get_param( type, name, 1 );
        define_param_value( prm, value, adjust );
    }
}

void configure_param_match( int type, std::string_view coef1, std::string_view coef2 )
{
    const int p2 = get_param( type, coef2, 1 );

    if( coef1.empty() ) return;

    if( coef1.back() == '*' )
    {
        wildcard_param_match( make_prmname( type, coef1 ), p2 );
    }
    else
    {
        const int p1 = get_param( type, coef1, 1 );
        define_param_match( p1, p2 );
    }
}
