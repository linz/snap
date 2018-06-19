#include "snapconfig.h"
/* Code for managing a list of reference frame transformations */

/*
   $Log: bearing.c,v $
   Revision 1.1  2004/04/20 00:58:36  ccrook
   These files were ommitted when they were first built

   Revision 1.1  1995/12/22 17:46:54  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "snap/bearing.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
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

#define BPLIST_INC 10

static brngProjection **bplist = NULL;
static int nbplist = 0;
static int nbproj = 0;
static const char *null_bpname = "null";

static int use_datum_trans=1;

static int find_bproj( const char *name )
{
    int nbp;

    for( nbp = 0; nbp < nbproj; nbp++ )
    {
        if( _stricmp( bplist[nbp]->name, name ) == 0 ) return nbp+1;
    }
    return 0;
}


static brngProjection *new_bproj( void )
{
    brngProjection *bp;

    bp = (brngProjection *) check_malloc( sizeof( brngProjection ) );

    if( nbproj >= nbplist )
    {
        nbplist = nbproj + BPLIST_INC;
        bplist = (brngProjection **) check_realloc( bplist,
                 nbplist * sizeof( brngProjection *) );
    }

    bplist[nbproj] = bp;
    nbproj++;

    bp->name = NULL;
    bp->prjsys = NULL;

    return bp;
}

void clear_bproj_list( void )
{
    int i;
    for( i = 0; i < nbproj; i++ )
    {
        if( bplist[i] )
        {
            if( bplist[i]->name ) check_free( bplist[i]->name );
            if( bplist[i]->prjsys ) delete_coordsys( bplist[i]->prjsys );
            check_free( bplist[i] );
        }
    }
    nbproj = 0;
}

static int create_bproj( const char *name )
{
    brngProjection *bp;
    coord_conversion cc;
    coordsys *prjsys;

    /* See if coordinate system is valid and can be converted to the
       network coordinate system */

    if( ! net ) return 0;

    prjsys = load_coordsys( name );
    if( ! prjsys ) return 0;

    if( ! is_projection( prjsys ) ||
            (use_datum_trans &&
             define_coord_conversion( &cc, net->geosys, prjsys ) != OK) )
    {
        delete_coordsys( prjsys );
        return 0;
    }

    bp= new_bproj();

    bp->name = copy_string( name );
    _strupr( bp->name );
    bp->dtmtrans=use_datum_trans;
    bp->prjsys = prjsys;
    define_coord_conversion( &(bp->prjconv), net->geosys, prjsys );

    return nbproj;
}


int get_bproj( const char *name )
{
    int bp;

    bp = find_bproj( name );
    if( !bp ) bp = create_bproj( name );
    return bp;
}

int bproj_count( void )
{
    return nbproj;
}

brngProjection *bproj_from_id( int id )
{
    return id ? bplist[id-1] : NULL;
}

const char *bproj_name( int id )
{
    brngProjection *bp = bproj_from_id( id );
    return bp ? bp->name : null_bpname;
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

