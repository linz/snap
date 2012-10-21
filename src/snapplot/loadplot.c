#include "snapconfig.h"
/* Routine to load all data into the plotting program */

/*
   $Log: loadplot.c,v $
   Revision 1.3  1996/07/12 20:32:06  CHRIS
   Modified to support hidden stations.

   Revision 1.2  1996/01/09 18:07:53  CHRIS
   Fixes an error in the routines snap_id and snap_name - refraction
   coeffients and other parameters were being treated as classifications.

   Revision 1.1  1996/01/03 22:21:46  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "snap/stnadj.h"
#include "snap/snapglob.h"
#include "snapdata/loaddata.h"
#include "snap/survfilr.h"
#include "plotconn.h"
#include "loadplot.h"
#include "plotbin.h"
#include "snap/bindata.h"
#include "util/classify.h"
#include "snap/bearing.h"
#include "util/chkalloc.h"
#include "snapplot_util.h"


static char rcsid[]="$Id: loadplot.c,v 1.3 1996/07/12 20:32:06 CHRIS Exp $";

/* Callback function used by loaddata to get id's of various objects */

typedef struct missing_stn_s
{
    struct missing_stn_s *next;
    char *code;
    int id;
} missing_stn;

static missing_stn *missing = NULL;
static int missing_id = 0;

static int missing_station_id( const char *code )
{
    missing_stn *ms, *prev, *newstn;
    if( !code ) return 0;
    for( ms = missing, prev = NULL; ms; prev = ms, ms = ms->next )
    {
        int cmp;
        cmp = stncodecmp( ms->code, code );
        if( cmp == 0 ) return ms->id;
        if( cmp > 0 ) break;
    }
    newstn = (missing_stn *) check_malloc( sizeof(missing_stn) + strlen(code) + 1 );
    newstn->next = ms;
    if( prev ) prev->next = newstn; else missing = newstn;
    newstn->id = --missing_id;
    newstn->code = ((char *)(void *)newstn)+sizeof(missing_stn);
    strcpy( newstn->code, code );
    return newstn->id;
}

static char *missing_station_name( int id )
{
    missing_stn *ms;
    for( ms = missing; ms; ms = ms->next )
    {
        if( ms->id == id ) return ms->code;
    }
    return NULL;
}

static void delete_missing_station_list( void )
{
    missing_stn *ms;
    while( missing )
    {
        ms = missing->next;
        check_free( missing );
        missing = ms;
    }
    missing_id = 0;
}

static void list_missing_stations( void )
{
    missing_stn *ms;
    int nline = 0;
    if( !missing ) return;
    print_log("\nThe following station codes are used in the data files\n");
    print_log("but are not listed in the coordinate file\n");
    for( ms = missing; ms; ms = ms->next )
    {
        if( nline == 6 ) { print_log("\n"); nline = 0; }
        print_log("  %-10s",ms->code);
        nline++;
    }
    /* TODO: Fix up this ...                       */
    /* print_log("\n\nPress enter to continue: "); */
    for(;;) { int c = getchar(); if( c== '\n' || c == EOF ) break; }
}


static long snap_id( int type, int group_id, const char *code )
{
    long id;
    id = 0;
    switch (type)
    {
    case ID_STATION:    id = find_station( net, code );
        if( id == 0 ) id = missing_station_id( code );
        break;
    case ID_CLASSTYPE:  id = classification_id( &obs_classes, code, 1 ); break;
    case ID_CLASSNAME:  id = class_value_id( &obs_classes, group_id, code, 1 ); break;
    case ID_PROJCTN:    id = get_bproj( code ); break;
    case ID_COEF:
    case ID_SYSERR:
    case ID_NOTE:       id = 0; break;
    }
    return id;
}

static const char *snap_name( int type, int group_id, long id )
{
    const char *name;
    name = NULL;
    switch (type)
    {
    case ID_STATION:    if( id < 0 ) name = missing_station_name( id );
        else name = station_code( (int) id );
        break;
    case ID_CLASSTYPE: name = classification_name( &obs_classes, (int) id ); break;
    case ID_CLASSNAME: name = class_value_name( &obs_classes, group_id, (int) id ); break;
    case ID_PROJCTN: name = bproj_name( id ); break;
    case ID_COEF:
    case ID_SYSERR:
    case ID_NOTE:      name = NULL; break;
    }
    return name;
}

static double snap_calc_value( int type, long id1, long id2 )
{
    if( type == CALC_DISTANCE )
    {
        double dist;
        station *st1 = stnptr(id1);
        station *st2 = stnptr(id2);
        if( !st1 || ! st2 ) return 0.0;
        dist = calc_distance( st1, 0.0, st2, 0.0, NULL, NULL );
        return dist;
    }
    else if ( type == CALC_HDIST )
    {
        double dist;
        station *st1 = stnptr(id1);
        station *st2 = stnptr(id2);
        if( !st1 || ! st2 ) return 0.0;
        dist = calc_horizontal_distance( st1, st2, NULL, NULL );
        return dist;
    }
    return 0.0;
}


static int flag_missing_stations( survdata *sd )
{
    trgtdata *t;
    int i;
    int nmissing;
    nmissing = 0;
    for( i = 0; i < sd->nobs; i++ )
    {
        t = get_trgtdata( sd, i );
        if( sd->from < 0 || t->to < 0 )
        {
            t->unused |= IGNORE_OBS_BIT;
            nmissing++;
        }
    }
    return nmissing;
}

static void add_survdata( survdata *sd )
{
    int nmissing;
    nmissing = flag_missing_stations( sd );
    if( nmissing )
    {
        save_survdata_subset( sd, -1, -1 );
    }
    else
    {
        save_survdata( sd );
    }
}


/*===============================================================*/
/* Routines to set, get, and calculate a topocentre              */



static void init_load_plot( void )
{
    init_load_data( add_survdata, snap_id, snap_name, snap_calc_value );
}

static void term_load_plot( void )
{
    term_load_data();
}

void load_connections( )
{
    init_load_plot();
    set_binary_data( 0 );
    read_data_files( cmd_dir, NULL );
    term_load_plot();
    load_observations_from_binary();
    list_missing_stations();
    delete_missing_station_list();
}
