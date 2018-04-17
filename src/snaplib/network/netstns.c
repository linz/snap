#include "snapconfig.h"
/*
   $Log: netstns.c,v $
   Revision 1.1  1995/12/22 17:31:03  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network/network.h"
#include "network/stnoffset.h"
#include "util/errdef.h"
#include "util/dstring.h"
#include "util/chkalloc.h"


/* Create a station ... */

station *new_station( void )
{
    station *st;
    st = (station *) check_malloc( sizeof( station ) );
    st->Code[0] = 0;
    st->nclass = 0;
    st->classval = NULL;
    st->Name = NULL;
    st->ts = NULL;
    st->hook = NULL;
    return st;
}

void delete_station( station *st )
{
    if( !st ) return;
    if( st->Name ) check_free( st->Name );
    if( st->classval ) check_free( st->classval );
    if (st->ts ) delete_station_offset( st );
    check_free( st );
}

void init_station_classes( station *st, int nclass )
{
    int oldnclass=st->nclass;
    int *oldval=st->classval;
    st->nclass = 0;
    st->classval=0;
    if( nclass > 0 )
    {
        int i;
        st->classval = (int *) check_malloc( sizeof(int) * nclass );
        for( i = 0; i < nclass; i++ ) 
        {
            st->classval[i] = i < oldnclass ? oldval[i] : 0;
        }
        st->nclass = nclass;
    }
    if( oldval ) check_free(oldval);
}

void set_station_class( station *s, int class_id, int value )
{
    if( class_id > 0 && class_id <= s->nclass )
    {
        s->classval[class_id-1] = value;
    }
    else
    {
        handle_error( INCONSISTENT_DATA, "Invalid class id in set_station_class",NO_MESSAGE);
    }
}

int get_station_class( station *s, int class_id )
{
    if( class_id > 0 && class_id <= s->nclass ) return s->classval[class_id-1];
    handle_error( INCONSISTENT_DATA, "Invalid class id in get_station_class",NO_MESSAGE);
    return 0;
}

static void setup_station_data( station *st, ellipsoid *el )
{
    double llh[3];
    init_toprot( st->ELat, st->ELon, &st->rTopo );
    init_gravrot( st->ELat, st->ELon, st->GXi, st->GEta, &st->rGrav );
    llh[CRD_LAT] = st->ELat; llh[CRD_LON] = st->ELon; llh[CRD_HGT] = st->OHgt + st->GUnd;
    llh_to_xyz(el, llh, st->XYZ, &st->dEdLn, &st->dNdLt );
}

void init_station( station *st, const char *code, const char *Name,
                   double Lat, double Lon, double Hgt,
                   double Xi, double Eta, double Und,
                   ellipsoid *el )
{

    strncpy( st->Code, code, STNCODELEN );
    st->Code[STNCODELEN] = 0;
    st->Name = copy_string( Name );
    st->ELat = Lat;
    st->ELon = Lon;
    st->OHgt = Hgt;
    st->GXi = Xi;
    st->GEta = Eta;
    st->GUnd = Und;
    st->hook = 0;
    setup_station_data( st, el );
}

void modify_station_coords( station *st,
                            double Lat, double Lon, double Hgt,
                            ellipsoid *el )
{
    st->ELat = Lat;
    st->ELon = Lon;
    st->OHgt = Hgt;
    setup_station_data( st, el );
}

void modify_station_xyz( station *st, double xyz[3], ellipsoid *el )
{
    double llh[3];
    xyz_to_llh(el, xyz, llh );
    modify_station_coords( st, llh[CRD_LAT], llh[CRD_LON], llh[CRD_HGT]-st->GUnd, el );
}

void stnmultifunc( station *st, void *data )
{
    stnmultifunc_data *smd=(stnmultifunc_data *) data;
    if( smd->func1 ) (*(smd->func1))(st,smd->data1);
    if( smd->func2 ) (*(smd->func1))(st,smd->data2);
}
