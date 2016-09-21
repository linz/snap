#include "snapconfig.h"
/* autofix.c - compiles data to provide a crude check of which stations can be adjusted and how.
 * For example if only have height data then cannot calculate horizontal position.
 * This is not a rigorous test of which stations can be calculated but handles common cases such as stations
 * constrained only be horizontal or verical data.
 */

#include <stdio.h>

#include "autofix.h"
#include "snap/bindata.h"
#include "snap/stnadj.h"
#include "snapdata/datatype.h"
#include "snapdata/survdata.h"
#include "util/errdef.h"
#include "util/chkalloc.h"

typedef struct
{
    int flags;
    int horstn1;
    int horstn2;
} autofix_data;

#define HAS_DIST 1
#define HAS_BEAR 2
#define HAS_HEIGHT 4
#define HAS_LAT 8
#define HAS_LON 16
#define HAS_3D (HAS_LAT | HAS_LON | HAS_HEIGHT)
#define IS_HOR (HAS_DIST | HAS_BEAR)
#define NO_STN -1

static int obsflags[NOBSTYPE];
static autofix_data *station_autodata=0;
static int max_station_autodata=0;

static void init_obsflags()
{
    int i;
    for( i=0; i < NOBSTYPE; i++ ) obsflags[i]=0;
    obsflags[GB] = HAS_3D;
    obsflags[GX] = HAS_3D;
    obsflags[SD] = HAS_DIST | HAS_HEIGHT;
    obsflags[HD] = HAS_DIST;
    obsflags[ED] = HAS_DIST;
    obsflags[MD] = HAS_DIST;
    obsflags[DR] = HAS_DIST | HAS_HEIGHT;
    obsflags[HA] = HAS_BEAR;
    obsflags[AZ] = HAS_BEAR;
    obsflags[PB] = HAS_BEAR;
    obsflags[ZD] = HAS_HEIGHT | HAS_DIST;
    obsflags[LV] = HAS_HEIGHT;
    obsflags[OH] = HAS_HEIGHT;
    obsflags[EH] = HAS_HEIGHT;
    obsflags[LT] = HAS_LAT;
    obsflags[LN] = HAS_LON;
    obsflags[EP] = HAS_LAT | HAS_LON;
    for( i=0; i < NOBSTYPE; i++ )
    {
        if( obsflags[i] == 0 )
        {
            char errmess[80];
            datatypedef *dtd;
            dtd=datatypedef_from_id(i);
            sprintf(errmess,"Data type %.20s not handled in autofix.c",dtd->name);
            handle_error(WARNING_ERROR,errmess,NULL);
        }
    }
}

void init_station_autodata( int maxstn )
{
    free_station_autofix_data();
    station_autodata=(autofix_data *) check_malloc( maxstn * sizeof(autofix_data));
    max_station_autodata=maxstn;
    for( int i=0; i<maxstn; i++ )
    {
        autofix_data *afx=&(station_autodata[i]);
        afx->flags=0;
        afx->horstn1=NO_STN;
        afx->horstn2=NO_STN;
    }
}

static void add_autofix( int from, int to, int flags )
{
    autofix_data *afx;
    if( from < 0 || from >= max_station_autodata ) return;
    afx=&(station_autodata[from]);
    afx->flags |= flags;
    if( flags & IS_HOR && to != NO_STN )
    {
        if( afx->horstn1 < 0 )
        {
            afx->horstn1=to;
        }
        else if( to != afx->horstn1 )
        {
            afx->horstn2=to;
        }
    }
}

static void add_survdata_fixdata( survdata *sd )
{
    trgtdata *t;
    datatypedef *dtd;
    int from, to, type, flags, ispoint;

    from=sd->from;
    for( int i = 0; i < sd->nobs; i++ )
    {
        t = get_trgtdata( sd, i );
        if( t->unused ) continue;
        type=t->type;
        dtd=datatypedef_from_id( type );
        flags=obsflags[type];
        ispoint=dtd->ispoint;
        to=t->to;
        if( ispoint )
        {
           /* Point obs reference the from station unless also vector */
           if( ! dtd->isvector ) to=from;
           if( ! rejected_station(to) ) add_autofix( to, NO_STN, flags );
        }
        else if( ! rejected_station(from) && ! rejected_station(to) )
        {
            add_autofix( from, to, flags );
            add_autofix( to, from, flags );
        }
    }
}

void compile_station_autofix_data()
{
    int maxstn;
    bindata *bd;
    maxstn=number_of_stations( net ) + 1;
    if( maxstn == max_station_autodata ) return;
    init_obsflags();
    init_station_autodata( maxstn );
    bd=create_bindata();
    init_get_bindata( 0L );
    while( get_bindata( bd ) == OK )
    {
        if( bd->bintype == SURVDATA )
        {
            add_survdata_fixdata( (survdata *) bd->data );
        }
    }
    delete_bindata( bd );
}

int station_autofix_constraints( int istn )
{
    int fixflags=0;
    compile_station_autofix_data();
    if( istn >= 0 || istn < max_station_autodata ) 
    {
        autofix_data *afx=&(station_autodata[istn]);
        int flags = afx->flags;
        /* Horizontal fixes */
        if( !(flags & (HAS_LAT | HAS_LON | HAS_DIST | HAS_BEAR )) )
        {
            fixflags = AUTOFIX_HOR;
        }

        if( ! (flags & HAS_HEIGHT ) )
        {
            fixflags = AUTOFIX_VRT;
        }
    }
    return fixflags;
}

int station_autofix_reject( int istn )
{
    int reject=0;

    compile_station_autofix_data();
    if( istn >= 0 || istn < max_station_autodata ) 
    {
        station *st=stnptr(istn);
        stn_adjustment *sa=stnadj(st);
        autofix_data *afx=&(station_autodata[istn]);
        int flags = afx->flags;
        /* Horizontal fixes */
        if( sa->flag.adj_h && ! sa->flag.float_h && 
             !(((flags & (HAS_LAT | HAS_LON)) == (HAS_LAT | HAS_LON))  ||
              ((flags & (HAS_DIST | HAS_BEAR)) == (HAS_DIST | HAS_BEAR)) ||
              ((flags & (HAS_DIST | HAS_BEAR)) && afx->horstn2 != NO_STN )) )
        {
            reject=1;
        }

        if( sa->flag.adj_v && ! sa->flag.float_v && ! (flags & HAS_HEIGHT ) )
        {
            reject=1;
        }
    }
    return reject;
}

void free_station_autofix_data()
{
    if( station_autodata ) check_free( station_autodata );
    max_station_autodata=0;
}


