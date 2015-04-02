#include "snapconfig.h"
/* Code to handle sorted observations.  These are managed by creating a
   linked list of observations.  When this is complete an index array of
   pointers is created and sorted, and then replaced by the file location
   of the observation */

/*
   $Log: sortobs.c,v $
   Revision 1.1  1996/01/03 22:10:57  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>

#define SORTOBS_C
#include "util/linklist.h"
#include "util/chkalloc.h"
#include "sortobs.h"

#undef SORTOBS_C

typedef struct
{
    int from;  /* From station */
    int to;    /* To station, or 0 if there are several */
    int type;  /* Observation type */
    long loc;  /* File location */
} obsdef;

typedef union
{
    obsdef *ptr;
    long loc;
} obsloc;

static void *obsdeflst = NULL;
static obsloc *obslst = NULL;
static int nobslst = 0;
static int nextobs = 0;

void save_observation( int from, int to, int type, long loc )
{
    obsdef *o;
    if( !obsdeflst ) obsdeflst = create_list( sizeof(obsdef) );
    o = (obsdef *) add_to_list( obsdeflst, NEW_ITEM );
    if( (to && to < from && sort_obs & SORT_BY_LINE) || ! from )
    {
        o->from = to;
        o->to = from;
    }
    else
    {
        o->from = from;
        o->to = to;
    }
    o->type = type;
    o->loc = loc;
    nobslst++;
}


static int cmp_obsdef( const void *p1, const void *p2 )
{
    obsdef *o1, *o2;
    int diftype, difline;
    o1 = ((obsloc *) p1)->ptr;
    o2 = ((obsloc *) p2)->ptr;

    diftype = o1->type - o2->type;
    difline = o1->from - o2->from;
    if( !difline ) difline = o1->to - o2->to;

    if( sort_obs & SORT_BY_TYPE )
    {
        return diftype ? diftype : difline;
    }
    else
    {
        return difline ? difline : diftype;
    }
}



void sort_observation_list( void )
{
    int i;

    if( nobslst <= 0 ) return;
    if( obslst ) return;

    /* Create an index array */

    obslst = (obsloc *) check_malloc( sizeof(obslst) * nobslst );

    /* Copy the locations of the definitions into the list */

    reset_list_pointer( obsdeflst );
    for( i = 0; i<nobslst; i++ )
    {
        obslst[i].ptr = (obsdef *) next_list_item( obsdeflst );
    }

    /* Sort the list */

    qsort( obslst, nobslst, sizeof(obsloc), cmp_obsdef );

    /* Copy the locations into the observation list */

    for( i=0; i<nobslst; i++ )
    {
        obslst[i].loc = obslst[i].ptr->loc;
    }

    /* Free up the observation definition list */

    free_list( obsdeflst, NO_ACTION );

}


void init_get_sorted_obs_loc( void )
{
    nextobs = 0;
}


long get_sorted_obs_loc( void )
{
    if( nextobs >= nobslst ) return -1;
    if( !obslst ) sort_observation_list();
    return obslst[nextobs++].loc;
}
