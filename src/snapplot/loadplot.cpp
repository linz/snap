#include "snapconfig.h"
/* Routine to load all data into the plotting program */

/*
   $Log: loadplot.c,v $
   Revision 1.3  1996/07/12 20:32:06  CHRIS
   Modified to support hidden stations.

   Revision 1.2  1996/01/09 18:07:53  CHRIS
   Fixes an error in the routines snap_id and snap_name - refraction
   coefficients and other parameters were being treated as classifications.

   Revision 1.1  1996/01/03 22:21:46  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <forward_list>
#include <optional>
#include <string>
#include <string_view>
#include <boost/numeric/conversion/cast.hpp>
using boost::numeric_cast;

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
#include "snapplot_util.h"


/* Callback function used by loaddata to get id's of various objects */

struct missing_stn
{
    missing_stn( std::string_view code, int id ) : code(code), id(id)
    {}

    const std::string code;
    const int id;
};

/// Stations used in the data files but absent from the coordinate file,
/// kept sorted by stncodecmp. Nodes never move, so a code's c_str() stays
/// valid until the list is cleared.
static std::forward_list<missing_stn> missing;
static int missing_id = 0;

/// Returns the id for a missing station code, adding it to the list if it
/// is not there already.
static int missing_station_id( std::string_view code )
{
    auto previous = missing.before_begin();
    for( auto station = missing.begin(); station != missing.end(); previous = station, ++station )
    {
        const int cmp = stncodecmp( station->code, code );
        if( cmp == 0 ) return station->id;
        if( cmp > 0 ) break;
    }
    return missing.emplace_after( previous, code, --missing_id )->id;
}

/// Returns the code of the missing station with the given id, or nullopt
/// if there is none.
static std::optional<std::string_view> missing_station_name( int id )
{
    for( const missing_stn &station : missing )
    {
        if( station.id == id ) return station.code;
    }
    return std::nullopt;
}

/// Empties the missing station list and restarts id numbering.
static void delete_missing_station_list( void )
{
    missing.clear();
    missing_id = 0;
}

static void list_missing_stations( void )
{
    int nline = 0;
    if( missing.empty() ) return;
    print_log("\nThe following station codes are used in the data files\n");
    print_log("but are not listed in the coordinate file\n");
    for( const missing_stn &station : missing )
    {
        if( nline == 6 ) { print_log("\n"); nline = 0; }
        print_log("  %-10s",station.code.c_str());
        nline++;
    }
    /* TODO: Fix up this ...                       */
    /* print_log("\n\nPress enter to continue: "); */
    for(;;) { int c = getchar(); if( c== '\n' || c == EOF ) break; }
}


static int64_t snap_id( int type, int group_id, std::string_view code )
{
    int64_t id;
    id = 0;
    switch (type)
    {
    case ID_STATION:    id = find_station( net, code );
        if( id == 0 ) id = missing_station_id( code );
        break;
    case ID_CLASSTYPE:  id = obs_classes.id( code, 1 ); break;
    case ID_CLASSNAME:  id = obs_classes.value_id( group_id, code, 1 ); break;
    case ID_PROJCTN:    id = get_bproj( code ); break;
    case ID_COEF:
    case ID_SYSERR:
    case ID_NOTE:       id = 0; break;
    }
    return id;
}

static std::string snap_name( int type, int group_id, long id )
{
    switch (type)
    {
    case ID_STATION:    if( id < 0 )
        {
            return std::string( missing_station_name( numeric_cast<int>( id ) ).value_or( std::string_view() ) );
        }
        return std::string( station_code( numeric_cast<int>( id ) ) );
    case ID_CLASSTYPE: return obs_classes.name( numeric_cast<int>( id ) );
    case ID_CLASSNAME: return obs_classes.value_name( group_id, numeric_cast<int>( id ) );
    case ID_PROJCTN: return std::string( bproj_name( numeric_cast<int>( id ) ) );
    case ID_COEF:
    case ID_SYSERR:
    case ID_NOTE:      break;
    }
    return std::string();
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
    read_data_files( NULL );
    term_load_plot();
    load_observations_from_binary();
    list_missing_stations();
    delete_missing_station_list();
}
