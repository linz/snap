#include "snapconfig.h"
/* netlist.c: Maintain an indexed list of stations */

/*
   $Log: netlist.c,v $
   Revision 1.1  1995/12/22 17:29:51  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <cctype>
#include <string_view>
#include <boost/numeric/conversion/cast.hpp>
using boost::numeric_cast;
#include <assert.h>

#include "network/network.h"
#include "util/fieldscanner.hpp"
#include "util/errdef.h"

#define STNLIST_INIT_INDEX_SIZE 1024

/* Searching by code will normally create the code index 
 * but will just to a linear search if less than 
 * STNLIST_MAX_LINEAR_SEARCH stations are not in the 
 * code index.
 */

#define STNLIST_MAX_LINEAR_SEARCH 256

/*=======================================================*/
/* Maintain a list of stations ...                       */
/* The stations are entered as a linked list.  Once entry*/
/* is completed an array index is set up for the sorted  */
/* list of stations.                                     */

station_list *new_station_list( void )
{
    station_list *sl = new station_list;
    sl->count = 0;
    sl->lastid=0;
    sl->indexsize=STNLIST_INIT_INDEX_SIZE;
    sl->index=new station *[sl->indexsize];
    sl->index[0]=nullptr;
    sl->nsorted=0;
    sl->maxsortid=0;
    sl->codeindex=nullptr;
    sl->usesorted=0;
    sl->nextstn=0;
    return sl;
}

void delete_station_list( station_list *sl )
{
    for( int i=1; i <= sl->lastid; i++ )
    {
        if( sl->index[i] ) delete_station(sl->index[i] );
    }
    delete [] sl->index;
    delete [] sl->codeindex;
    delete sl;
}

void sl_add_station( station_list *sl, station *st )
{

    sl->count++;
    sl->lastid++;
    if( sl->lastid >= sl->indexsize )
    {
        station **newindex = new station *[sl->indexsize * 2];
        std::copy( sl->index, sl->index + sl->indexsize, newindex );
        delete [] sl->index;
        sl->index = newindex;
        sl->indexsize *= 2;
    }
    sl->index[sl->lastid]=st;
    if( st ) st->id=sl->lastid;
}

/* Function used by reload_station_list to ensure station ids are preserved */
/* Assumes stations are being added in order of id */

void sl_add_station_at_id( station_list *sl, station *st )
{
    int id=st->id;
    if( id <= sl->lastid ) return;
    while( sl->lastid < id-1 ) sl_add_station(sl,0);
    sl_add_station(sl,st);
}

void sl_remove_station( station_list *sl, station *st )
{
    int id = st->id;
    if( id > 0 && id <= sl->lastid && sl->index[id] == st )
    {
        sl->index[id]=0;
        st->id=0;
        sl->count--;
        if( id <= sl->maxsortid ) sl->maxsortid=0;
    }
}


/// Compares two station codes, returning negative if s1 sorts before s2,
/// zero if they are equal and positive if s1 sorts after s2. Letters are
/// compared ignoring case, and codes whose first run of digits starts at the
/// same place after equal text are ordered by the value of that run, so
/// "AB9" sorts before "AB10".
///
/// The runs are compared as digit strings, not converted to a number. The
/// original used atol, which is undefined for a run too long for a long, and
/// parse_leading<long> reports overflow as no value, which would sort such a
/// run as zero and so give an inconsistent order. A comparison that cannot
/// fail matters because this is the ordering for qsort, std::lower_bound and
/// the station code maps, none of which can handle an error from it.
int stncodecmp(
    std::string_view s1,   ///< the first station code
    std::string_view s2 )  ///< the second station code
{
    const size_t digits1 = std::find_if( s1.begin(), s1.end(), is_digit ) - s1.begin();
    const size_t digits2 = std::find_if( s2.begin(), s2.end(), is_digit ) - s2.begin();
    if( digits1 == digits2 && digits1 < s1.size() && digits1 < s2.size()
        && compare_ignoring_case( s1.substr(0,digits1), s2.substr(0,digits1) ) == 0 )
    {
        // The digits of a run without its leading zeros. A longer run is a
        // larger number, and runs of equal length compare as text, so the
        // comparison needs no integer type and cannot overflow.
        const auto significantDigits = []( const std::string_view text )
        {
            const size_t end = std::find_if_not( text.begin(), text.end(), is_digit ) - text.begin();
            const size_t zeros = std::find_if_not( text.begin(), text.begin() + end,
                []( const char ch ) { return ch == '0'; } ) - text.begin();
            return text.substr( zeros, end - zeros );
        };
        const std::string_view number1 = significantDigits( s1.substr(digits1) );
        const std::string_view number2 = significantDigits( s2.substr(digits2) );
        if( number1.size() != number2.size() ) return number1.size() < number2.size() ? -1 : 1;
        const int cmp = number1.compare( number2 );
        if( cmp != 0 ) return cmp < 0 ? -1 : 1;
    }
    return compare_ignoring_case( s1, s2 );
}

static int stncmp( const void *st1, const void *st2 )
{
    int cmp=stncodecmp( (*(station **)st1)->Code, (*(station **)st2)->Code );
    if( cmp == 0 )
    {
        cmp=(*(station **)st2)->id-(*(station **)st1)->id;
    }
    return cmp;
}

static void index_stations( station_list *sl )
{
    int i, ic, count; 

    if( sl->maxsortid == sl->lastid ) return;

    count=sl->count;
    delete [] sl->codeindex;
    sl->codeindex = new station *[1+count];
    sl->codeindex[0] = nullptr;

    ic=0;
    for( i = 1; i <= sl->lastid; i++ )
    {
        if( sl->index[i] )
        {
            ic++;
            if( ic <= count ) sl->codeindex[ic]=sl->index[i];
        }
    }
    assert( ic == count );
    qsort( sl->codeindex+1, count, sizeof( station * ), stncmp );

    sl->maxsortid=sl->lastid;
    sl->nsorted=count;
}

/// Returns the position in the sorted code index of the first station with the
/// given code, or 0 if there is none.
static int sl_lookup_codeindex( station_list *sl, std::string_view code )
{
    if( sl->nsorted < 1 ) return 0;
    station **first = sl->codeindex+1;
    station **last = first+sl->nsorted;
    station **match = std::lower_bound( first, last, code,
        []( const station *st, std::string_view target ) { return stncodecmp( st->Code, target ) < 0; } );
    if( match == last || stncodecmp( (*match)->Code, code ) != 0 ) return 0;
    return numeric_cast<int>( match - sl->codeindex );
}

int sl_reindex_stations( station_list *sl )
{
    /* Pack station array removing deleted stations.  Also resets station ids */

    int nremove=0;
    for( int i=1; i <= sl->lastid; i++ )
    {
        station *st=sl->index[i];
        if( st == 0 )
        {
            nremove++;
        }
        else if( nremove > 0 )
        {
            sl->index[i-nremove]=st;
            st->id=i-nremove;
        }
    }
    if( nremove )
    {
        sl->lastid -= nremove;
        assert(sl->lastid == sl->count);
        sl->count=sl->lastid;
        sl->nsorted=0;
        sl->maxsortid=0;
    }
    return nremove;
}

int sl_remove_duplicate_stations( station_list *sl, int reindex, void *data, stnfunc function )
{
    std::optional<std::string_view> code;
    int nremove=0;

    index_stations(sl);
    for( int i=1; i <= sl->count; i++ )
    {
        station *st=sl->codeindex[i];
        if( ! code || stncodecmp(st->Code,*code) != 0 )
        {
            code=std::string_view( st->Code );
        }
        else
        {
            sl_remove_station(sl, st);
            nremove++;
            if( function ) (*function)(st,data);
        }
    }
    if( nremove && reindex )
    {
        sl_reindex_stations( sl );
    }
    return nremove;
}

int sl_find_station( station_list *sl, std::string_view code )
{
    int i;
    if( sl->count < 0 ) return 0;
    if( sl->lastid > sl->maxsortid )
    {
        if( sl->lastid - sl->maxsortid > STNLIST_MAX_LINEAR_SEARCH )
        {
            index_stations(sl);
        }
        else
        {
            for( i = sl->maxsortid+1; i <= sl->lastid; i++ )
            {
                station *st=sl->index[i];
                if( st && stncodecmp(st->Code,code)==0 ) return i;
            }
        }
    }
    i=sl_lookup_codeindex( sl, code );
    return i ? sl->codeindex[i]->id: 0;
}

int sl_find_station_sorted_id( station_list *sl, std::string_view code )
{
    index_stations(sl);
    return sl_lookup_codeindex( sl, code );
}

int sl_station_id( station_list *, station *st )
{
    return st->id;
}

station *sl_station_ptr( station_list *sl, int istn )
{
    if( istn < 1 || istn > sl->lastid ) return NULL;
    return sl->index[istn];
}

station *sl_station_sorted_ptr( station_list *sl, int istn )
{
    if( istn < 1 || istn > sl->nsorted ) return NULL;
    return sl->codeindex[istn];
}


void sl_process_stations( station_list *sl, void *data, void (*function)( station *st, void *data ) )
{
    int i;
    index_stations( sl );
    for( i=0; i++ < sl->nsorted; )
    {
        if(sl->index[i]) (*function)( sl->index[i], data );
    }
}

void sl_reset_station_list( station_list *sl, int sorted )
{
    if( sorted ) index_stations( sl );
    sl->usesorted = sorted;
    sl->nextstn = 1;
}

station *sl_next_station( station_list *sl )
{
    station *st=0;

    if( sl->usesorted )
    {
        if( sl->nextstn <= sl->nsorted ) 
        {
            st=sl->codeindex[sl->nextstn];
            sl->nextstn++;
        }
    }
    else
    {
        while( sl->nextstn <= sl->lastid && ! st )
        {
            st=sl->index[sl->nextstn];
            sl->nextstn++;
        }
    }
    return st;
}


int sl_number_of_stations( station_list *sl )
{
    return sl->count;
}

