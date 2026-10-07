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

#include <boost/numeric/conversion/cast.hpp>

using boost::numeric_cast;

void delete_station( station *st )
{
    if( !st ) return;
    if( st->ts ) delete_station_offset( st );
    delete st;
}

void station::set_class_count( const int count )
{
    classval.resize( count > 0 ? numeric_cast<size_t>( count ) : 0 );
}

void station::set_class( const int class_id, const int value )
{
    if( class_id > 0 && numeric_cast<size_t>( class_id ) <= classval.size() )
    {
        classval[class_id-1] = value;
    }
    else
    {
        handle_error( INCONSISTENT_DATA, "Invalid class id in set_station_class",NO_MESSAGE);
    }
}

int station::get_class( const int class_id ) const
{
    if( class_id > 0 && numeric_cast<size_t>( class_id ) <= classval.size() ) return classval[class_id-1];
    // handle_error( INCONSISTENT_DATA, "Invalid class id in get_station_class",NO_MESSAGE);
    return 0;
}

void station::_derive_geometry( ellipsoid &el )
{
    double llh[3];
    rTopo = rotmat( ELat, ELon );
    rGrav = rotmat::gravimetric( ELat, ELon, GXi, GEta );
    llh[CRD_LAT] = ELat; llh[CRD_LON] = ELon; llh[CRD_HGT] = OHgt + GUnd;
    llh_to_xyz( &el, llh, XYZ, &dEdLn, &dNdLt );
}

station::station( const std::string_view code, const std::string_view name,
                  const double Lat, const double Lon, const double Hgt,
                  const double Xi, const double Eta, const double Und,
                  ellipsoid &el )
    : ELat( Lat ),
      ELon( Lon ),
      OHgt( Hgt ),
      GXi( Xi ),
      GEta( Eta ),
      GUnd( Und ),
      Name( name )
{
    Code.assign( code );
    _derive_geometry( el );
}

void station::modify_coords( const double Lat, const double Lon, const double Hgt, ellipsoid &el )
{
    ELat = Lat;
    ELon = Lon;
    OHgt = Hgt;
    _derive_geometry( el );
}

void station::modify_coords_xeu( const double Lat, const double Lon, const double Hgt,
                                 const double Xi, const double Eta, const double Und,
                                 ellipsoid &el )
{
    ELat = Lat;
    ELon = Lon;
    OHgt = Hgt;
    GXi = Xi;
    GEta = Eta;
    GUnd = Und;
    _derive_geometry( el );
}

void station::modify_xyz( double xyz[3], ellipsoid &el )
{
    double llh[3];
    xyz_to_llh( &el, xyz, llh );
    modify_coords( llh[CRD_LAT], llh[CRD_LON], llh[CRD_HGT]-GUnd, el );
}

void stnmultifunc( station *st, void *data )
{
    stnmultifunc_data *smd=(stnmultifunc_data *) data;
    if( smd->func1 ) (*(smd->func1))(st,smd->data1);
    if( smd->func2 ) (*(smd->func2))(st,smd->data2);
}
