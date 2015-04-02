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

#include "network/network.h"
#include "util/linklist.h"
#include "util/chkalloc.h"
#include "util/errdef.h"

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
    sl->list = create_list( 0 );
    sl->indexed = 0;
    sl->index = NULL;
    sl->use_sorted = 0;
    sl->nextstn = 0;
    return sl;
}

void delete_station_list( station_list *sl )
{
    station *st;
    reset_list_pointer( sl->list );
    while( NULL != (st = (station *) next_list_item( sl->list )) )
        delete_station( st );
    free_list( sl->list, NO_ACTION );
    if( sl->index ) check_free( sl->index );
    check_free( sl );
}

void sl_add_station( station_list *sl, station *st )
{

    add_to_list( sl->list, st );
    sl->count++;                /* Increment count and invalidate the indexes */
    sl->indexed = 0;
}

void sl_remove_station( station_list *sl, station *st )
{
    st = (station *) del_list_item( sl->list, st );
    if( st )
    {
        delete_station( st );
        sl->count--;
        sl->indexed = 0;
    }
}


int stncodecmp( const char *s1, const char *s2 )
{
    long l1, l2;
    l1 = atol( s1 );
    l2 = atol( s2 );
    if( l1 < l2 ) return -1;
    if( l1 > l2 ) return 1;
    return _stricmp( s1, s2 );
}

static  int stncodecmps( const void *code, const void *st )
{
    char *s1, *s2;
    s1 = (char *) code;
    s2 = (*(station**)st)->Code;
    return stncodecmp( s1, s2 );
}

static  int stncmp( const void *st1, const void *st2 )
{
    return stncodecmp( (*(station **)st1)->Code, (*(station **)st2)->Code );
}

static void index_stations( station_list *sl )
{
    int i;
    if( sl->indexed ) return;

    if( sl->index ) check_free( sl->index );
    sl->index = (station **) check_malloc( (1+sl->count) * sizeof(station *) );

    reset_list_pointer( sl->list );
    sl->index[0] = NULL;
    for( i = 0; i++ < sl->count; )
    {
        sl->index[i] = (station *) next_list_item( sl->list );;
    }

    qsort( sl->index+1, sl->count, sizeof( station * ), stncmp );

    for( i = 0; i++ < sl->count; )
    {
        sl->index[i]->id = i;
    }

    sl->indexed = 1;
}

int sl_find_station( station_list *sl, const char *code )
{
    station **match;
    if( !sl->indexed ) index_stations( sl );
    match = (station **) bsearch( code, sl->index+1, sl->count, sizeof(station *), stncodecmps );
    return match ? match - sl->index : 0;
}

int sl_station_id( station_list *sl, station *st )
{
    if( !sl->indexed ) index_stations( sl );
    return st->id;
}

station *sl_station_ptr( station_list *sl, int istn )
{
    if( istn < 1 || istn > sl->count ) return NULL;
    if( !sl->indexed ) index_stations( sl );
    return sl->index[istn];
}


void sl_process_stations( station_list *sl, void *data, void (*function)( station *st, void *data ) )
{
    int i;
    if( !sl->indexed ) index_stations( sl );
    for( i=0; i++ < sl->count; )
    {
        if(sl->index[i]) (*function)( sl->index[i], data );
    }
}

void sl_reset_station_list( station_list *sl, int sorted )
{
    if( sorted )
    {
        if( !sl->indexed ) index_stations( sl );
        sl->use_sorted = 1;
        sl->nextstn = 1;
    }
    else
    {
        sl->use_sorted = 0;
        reset_list_pointer( sl->list );
    }
}

station *sl_next_station( station_list *sl )
{
    station *st;
    st = NULL;
    if( sl->use_sorted )
    {
        if( sl->indexed && sl->nextstn <= sl->count )
        {
            st = sl->index[sl->nextstn];
            sl->nextstn++;
        }
    }
    else
    {
        st = (station *) next_list_item( sl->list );
    }
    return st;
}


int sl_number_of_stations( station_list *sl )
{
    return sl->count;
}

