#include "snapconfig.h"
/*
   $Log: network.c,v $
   Revision 1.2  1998/05/21 04:00:26  ccrook
   Added geodetic coordinate system to network object and facilitated getting and setting
   coordinates in the network coordinate system.

   Revision 1.1  1995/12/22 17:32:16  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>

#include <boost/algorithm/string/predicate.hpp>

#include "network/network.h"

/*=============================================================*/
/* Basic routine to read a station data file                   */

static stationfunc default_initstation=0;
static stationfunc default_uninitstation=0;

network::network() :
    name( std::nullopt ),
    stnlist( nullptr ),
    crdsys( nullptr ),
    geosys( nullptr ),
    topolat( 0 ),
    topolon( 0 ),
    got_topocentre( 0 ),
    options( 0 ),
    orderclsid( 0 ),
    initstation( default_initstation ),
    uninitstation( default_uninitstation )
{
}

network *new_network( void )
{
    return new network();
}

void set_network_initstn_func( network *nw, stationfunc initfunc, stationfunc uninitfunc )
{
    if( nw )
    {
        nw->initstation=initfunc;
        nw->uninitstation=uninitfunc;
    }
    else
    {
        default_initstation=initfunc;
        default_uninitstation=uninitfunc;
    }
}

void set_network_name( network *nw, const std::string &n )
{
    nw->name = std::nullopt;
    size_t start = n.find_first_not_of(" \n");
    if( start == std::string::npos ) return;
    std::string name = n.substr(start);
    size_t nl = name.find('\n');
    if( nl != std::string::npos ) name.resize(nl);
    nw->name = std::move(name);
}

static void uninit_station( station *st, void *pnw )
{
    network *nw=(network *)pnw;
    nw->uninitstation( st );
}

void network::clear()
{
    if( uninitstation )
    {
        process_stations( this, this, uninit_station );
    }
    name = std::nullopt;
    crdsysdef.clear();
    if( stnlist ) { delete_station_list( stnlist ); stnlist = nullptr; }
    if( crdsys ) { delete crdsys; crdsys = nullptr; }
    if( geosys ) { delete geosys; geosys = nullptr; }
    ccnet = coord_conversion();
    ccgeo = coord_conversion();
    topolat = 0;
    topolon = 0;
    got_topocentre = 0;
    options = 0;
    orderclsid = 0;
    stnclasses.clear();
}

network::~network()
{
    clear();
}

void delete_network( network *nw )
{
    delete nw;
}

int network::classification_count() const
{
    return stnclasses.count();
}

int network::class_id( const std::string &classname, int create )
{
    int id = stnclasses.id( classname, 0 );
    if( create && id == 0 )
    {
        id = stnclasses.id( classname, 1 );
        stnclasses.set_default_value( id, "-" );
        if( boost::algorithm::iequals(classname,STATION_ORDER_CLASS_NAME) ) orderclsid = id;
    }
    return id;
}

std::string network::class_name( int class_id ) const
{
    return stnclasses.name( class_id );
}

int network::class_count( int class_id ) const
{
    return stnclasses.value_count( class_id );
}

int network::class_value_id( int class_id, const std::string &value, int create )
{
    return stnclasses.value_id( class_id, value, create );
}

std::string network::class_value( int class_id, int value_id ) const
{
    return stnclasses.value_name( class_id, value_id );
}

int network::add_orders()
{
    return class_id( STATION_ORDER_CLASS_NAME, 1 );
}

int network::order_count() const
{
    return orderclsid ? class_count(orderclsid) : 0;
}

int network::order_id( const std::string &order, int addorder )
{
    return orderclsid ? class_value_id( orderclsid, order, addorder ) : 0;
}

std::string network::order( int orderid ) const
{
    return class_value(orderclsid,orderid);
}

int network::station_order( station *stn ) const
{
    return orderclsid ? stn->get_class( orderclsid ) : 0;
}

int network_has_explicit_geoid_info( network *nw )
{
    return (nw->options & (NW_GEOID_HEIGHTS | NW_DEFLECTIONS))
            && (nw->options & NW_EXPLICIT_GEOID);
}

int network_has_geoid_info( network *nw )
{
    return coordsys_heights_orthometric( nw->crdsys ) 
        || network_has_explicit_geoid_info( nw );
}

void set_network_explicit_geoid_info( network *nw, char geoid_opts )
{
    if( ! geoid_opts ) geoid_opts = NW_GEOID_HEIGHTS | NW_DEFLECTIONS;
    nw->options |= geoid_opts | NW_EXPLICIT_GEOID;
}

void clear_network_explicit_geoid_info( network *nw )
{
    nw->options &= ~NW_EXPLICIT_GEOID;
}

int network_height_coord_is_ellipsoidal( network *nw )
{
    return nw->options & NW_ELLIPSOIDAL_HEIGHTS ? 1 : 0;
}

void set_network_height_coord_ellipsoidal( network *nw )
{
    nw->options |= NW_ELLIPSOIDAL_HEIGHTS;
}

void set_network_height_coord_orthometric( network *nw )
{
    nw->options &= ~ NW_ELLIPSOIDAL_HEIGHTS;
}
