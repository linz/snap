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
    init_classifications( &stnclasses );
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
    if( stnlist ) { delete_station_list( stnlist ); stnlist = 0; }
    if( crdsys ) { delete crdsys; crdsys = 0; }
    if( geosys ) { delete geosys; geosys = 0; }
    delete_classifications( &stnclasses );
}

network::~network()
{
    clear();
}

void delete_network( network *nw )
{
    delete nw;
}

int network_classification_count( network *nw )
{
    return classification_count( &(nw->stnclasses) );
}

int network_class_id( network *nw, const char *classname, int create )
{
    int id = classification_id( &(nw->stnclasses), classname, 0 );
    if( create && id == 0 )
    {
        id = classification_id( &(nw->stnclasses), classname, 1 );
        set_default_class_value( &(nw->stnclasses), id, "-" );
        if( _stricmp(classname,STATION_ORDER_CLASS_NAME) == 0 ) nw->orderclsid = id;
    }
    return id;
}
const char *network_class_name( network *nw, int class_id )
{
    return classification_name( &(nw->stnclasses), class_id);
}
int network_class_count( network *nw, int class_id )
{
    return class_value_count( &(nw->stnclasses), class_id );
}

int network_class_value_id( network *nw, int class_id, const char *value, int create )
{
    return class_value_id( &(nw->stnclasses), class_id, value, create );
}

const char *network_class_value( network *nw, int class_id, int value_id )
{
    return class_value_name( &(nw->stnclasses), class_id, value_id );
}

int add_network_orders( network *nw )
{
    return network_class_id( nw, STATION_ORDER_CLASS_NAME, 1 );
}

int network_order_count( network *nw )
{
    return nw->orderclsid ? network_class_count(nw,nw->orderclsid) : 0;
}

int network_order_id( network *nw, const char *order, int addorder )
{
    return nw->orderclsid ? network_class_value_id( nw, nw->orderclsid, order, addorder ) : 0;
}

const char *network_order( network *nw, int orderid )
{
    return network_class_value(nw,nw->orderclsid,orderid);
}

int network_station_order( network *nw, station *stn )
{
    return nw->orderclsid ? get_station_class( stn, nw->orderclsid ) : 0;
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
