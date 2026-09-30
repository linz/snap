#include "snapconfig.h"
/* Code for managing a list of reference frame transformations */

/*
   $Log: bearing.c,v $
   Revision 1.1  2004/04/20 00:58:36  ccrook
   These files were omitted when they were first built

   Revision 1.1  1995/12/22 17:46:54  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <cctype>
#include <memory>
#include <string_view>
#include <vector>
#include <boost/numeric/conversion/cast.hpp>
using boost::numeric_cast;

#include "snap/bearing.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/fieldscanner.hpp"
#include "network/network.h"
/* #include "errdef.h" */
#include "snap/stnadj.h"
#include "util/pi.h"


#define DIST_TOL 1.0e-5
#define DIST_TOL_2 (DIST_TOL*DIST_TOL)

/* Definition of a bearing projection */

/*  The bearing projection is a bearing coordinate system that can be
    converted to the adjustment base coordinate system.  This is used
    to calculate projection bearings.

    Each bearing coordinate system is identified by a coordinate system
    code, and has a defined coordinate conversion object for converting the
    base latitude and longitude to projection bearings.

*/

static std::vector<std::unique_ptr<brngProjection>> bplist;
static constexpr std::string_view null_bpname = "null";

static int use_datum_trans=1;

static int find_bproj( std::string_view name )
{
    for( size_t nbp = 0; nbp < bplist.size(); nbp++ )
    {
        if( compare_ignoring_case( bplist[nbp]->name, name ) == 0 ) return numeric_cast<int>( nbp+1 );
    }
    return 0;
}


void clear_bproj_list( void )
{
    bplist.clear();
}

static int create_bproj( std::string_view name )
{
    coord_conversion cc;

    /* See if coordinate system is valid and can be converted to the
       network coordinate system */

    if( ! net ) return 0;

    coordsys *prjsys = load_coordsys( name );
    if( ! prjsys ) return 0;

    if( ! is_projection( prjsys ) ||
            (use_datum_trans &&
             define_coord_conversion( &cc, net->geosys, prjsys ) != OK) )
    {
        delete prjsys;
        return 0;
    }

    auto bp = std::make_unique<brngProjection>();

    bp->name = name;
    std::transform( bp->name.begin(), bp->name.end(), bp->name.begin(),
                    []( unsigned char ch ) { return std::toupper( ch ); } );
    bp->dtmtrans=use_datum_trans;
    bp->prjsys = prjsys;
    define_coord_conversion( &(bp->prjconv), net->geosys, prjsys );
    bplist.push_back( std::move(bp) );

    return bproj_count();
}


int get_bproj( std::string_view name )
{
    int bp;

    bp = find_bproj( name );
    if( !bp ) bp = create_bproj( name );
    return bp;
}

int bproj_count( void )
{
    return numeric_cast<int>( bplist.size() );
}

brngProjection *bproj_from_id( int id )
{
    return id ? bplist[id-1].get() : NULL;
}

std::string_view bproj_name( int id )
{
    brngProjection *bp = bproj_from_id( id );
    return bp ? std::string_view( bp->name ) : null_bpname;
}


int calc_prj_azimuth2( int bproj_id,
                       station *st1, double hgt1, station *st2, double hgt2,
                       double *angle, vector3 dst1, vector3 dst2 )
{

    double e1, n1, e2, n2, de, dn;
    double llh[3], enh[3];
    brngProjection *bproj;

    bproj = bproj_from_id( bproj_id );

    /* Use the geodetic azimuth to calculate dependency upon parameters
       approximate, but good enough */

    if( ! bproj || dst1 || dst2 )
    {
        (*angle) = calc_azimuth(st1, hgt1, st2, hgt2, 0, dst1, dst2 );
    }

    if( ! bproj ) return INVALID_DATA;

    llh[CRD_LAT] = st1->ELat;
    llh[CRD_LON] = st1->ELon;
    llh[CRD_HGT] = 0.0;

    if( bproj->dtmtrans )
    {
        if( convert_coords( &(bproj->prjconv), llh, NULL, enh, NULL ) != OK )
            return INVALID_DATA;
        e1 = enh[CRD_EAST];
        n1 = enh[CRD_NORTH];

        llh[CRD_LAT] = st2->ELat;
        llh[CRD_LON] = st2->ELon;

        if( convert_coords( &(bproj->prjconv), llh, NULL, enh, NULL ) != OK )
            return INVALID_DATA;
        e2 = enh[CRD_EAST];
        n2 = enh[CRD_NORTH];
    }
    else
    {
        projection *prj = bproj->prjsys->prj;
        if( geog_to_proj(prj,st1->ELon,st1->ELat,&e1,&n1) != OK ) return INVALID_DATA;
        if( geog_to_proj(prj,st2->ELon,st2->ELat,&e2,&n2) != OK ) return INVALID_DATA;
    }


    de = e2-e1;
    dn = n2-n1;
    if( de*de + dn*dn < DIST_TOL_2 )
    {
        (*angle) = 0.0;
    }
    else
    {
        (*angle) = atan2( e2-e1, n2-n1 );
    }

    return OK;
}

void set_bproj_use_datum_transformation( int usedatum )
{
    use_datum_trans=usedatum;
}

