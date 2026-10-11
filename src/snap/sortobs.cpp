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
#include <algorithm>
#include <vector>

#define SORTOBS_C
#include "sortobs.h"

#undef SORTOBS_C

struct obsdef
{
    int from;  /* From station */
    int to;    /* To station, or 0 if there are several */
    int type;  /* Observation type */
    int64_t loc;  /* File location */
};

static std::vector<obsdef> obsdeflst;
static std::vector<int64_t> sorted_locs;
static size_t nextobs = 0;

void save_observation( int from, int to, int type, int64_t loc )
{
    obsdef &o = obsdeflst.emplace_back();
    if( (to && to < from && sort_obs & SORT_BY_LINE) || ! from )
    {
        o.from = to;
        o.to = from;
    }
    else
    {
        o.from = from;
        o.to = to;
    }
    o.type = type;
    o.loc = loc;
}


static bool obsdef_precedes( const obsdef &o1, const obsdef &o2 )
{
    const int diftype = o1.type - o2.type;
    int difline = o1.from - o2.from;
    if( !difline ) difline = o1.to - o2.to;

    if( sort_obs & SORT_BY_TYPE )
    {
        return (diftype ? diftype : difline) < 0;
    }
    else
    {
        return (difline ? difline : diftype) < 0;
    }
}



void sort_observation_list( void )
{
    if( obsdeflst.empty() ) return;

    /* Sort the observation definitions, keeping the saved order of equal ones */

    std::stable_sort( obsdeflst.begin(), obsdeflst.end(), obsdef_precedes );

    /* Copy the locations into the observation list */

    sorted_locs.clear();
    for( const obsdef &o : obsdeflst )
    {
        sorted_locs.push_back( o.loc );
    }

    /* Free up the observation definition list */

    obsdeflst.clear();
}


void init_get_sorted_obs_loc( void )
{
    nextobs = 0;
}


int64_t get_sorted_obs_loc( void )
{
    if( sorted_locs.empty() ) sort_observation_list();
    if( nextobs >= sorted_locs.size() ) return -1;
    return sorted_locs[nextobs++];
}
