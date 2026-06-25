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
#include "util/snapctype.h"
#include <assert.h>

#include "network/network.h"
#include "util/linklist.h"
#include "util/chkalloc.h"
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
    station_list *sl;

    sl = (station_list *) check_malloc( sizeof(station_list) );
    sl->count = 0;
    sl->lastid=0;
    sl->indexsize=STNLIST_INIT_INDEX_SIZE;
    sl->index=(station **) check_malloc(sizeof(station *) * sl->indexsize );
    sl->index[0]=0;
    sl->nsorted=0;
    sl->maxsortid=0;
    sl->codeindex=0;
    sl->usesorted=0;
    sl->nextstn=0;
    return sl;
}

void delete_station_list( station_list *sl )
{
    int i;
    for( i=1; i < sl->lastid; i++ )
    {
        if( sl->index[i] ) delete_station(sl->index[i] );
    }
    if( sl->index ) check_free( sl->index );
    if( sl->codeindex ) check_free( sl->codeindex );
    check_free( sl );
}

void sl_add_station( station_list *sl, station *st )
{

    sl->count++;
    sl->lastid++;
    if( sl->lastid >= sl->indexsize )
    {
        sl->indexsize *= 2;
        sl->index=(station **) check_realloc((void *)(sl->index), sizeof(station *) * sl->indexsize );
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


int stncodecmp( const char *s1, const char *s2 )
{
    long l1, l2;
    while( *s1 && *s2 && TOLOWER(*s1) == TOLOWER(*s2) && ! ISDIGIT(*s1) )
    {
        s1++;
        s2++;
    }
    if( ISDIGIT(*s1 ) && ISDIGIT(*s2) )
    {
        l1 = atol( s1 );
        l2 = atol( s2 );
        if( l1 < l2 ) return -1;
        if( l1 > l2 ) return 1;
    }
    return _stricmp( s1, s2 );
}

static  int stncodecmps( const void *code, const void *st )
{
    char *s1, *s2;
    s1 = (char *) code;
    s2 = (*(station**)st)->Code;
    return stncodecmp( s1, s2 );
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
    if( sl->codeindex ) check_free( sl->codeindex );
    sl->codeindex = (station **) check_malloc( (1+count) * sizeof(station *) );
    sl->codeindex[0] = 0;

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

static int sl_lookup_codeindex( station_list *sl, const char *code )
{
    station **match;
    if( sl->nsorted < 1 ) return 0;
    match = (station **) bsearch( code, sl->codeindex+1, sl->nsorted, sizeof(station *), stncodecmps );
    if( ! match ) return 0;
    int id=match-sl->codeindex;
    while( id > 1 )
    {
        if( stncodecmp(sl->codeindex[id-1]->Code, code) != 0 ) break;
        id--;
    }
    return match ? match - sl->codeindex : 0;
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
    const char *code=0;
    int nremove=0;

    index_stations(sl);
    for( int i=1; i <= sl->count; i++ )
    {
        station *st=sl->codeindex[i];
        if( code == 0 || stncodecmp(st->Code,code) != 0 )
        {
            code=st->Code;
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

int sl_find_station( station_list *sl, const char *code )
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

int sl_find_station_sorted_id( station_list *sl, const char *code )
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

