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
#include "util/chkalloc.h"
#include "util/dstring.h"

/*=============================================================*/
/* Basic routine to read a station data file                   */

static stationfunc default_initstation=0;
static stationfunc default_uninitstation=0;

network *new_network( void )
{
    network *nw;

    nw = (network *) check_malloc( sizeof( network ) );
    init_network( nw );
    return nw;
}

void init_network( network *nw )
{
    nw->name = NULL;
    nw->crdsysdef = NULL;
    nw->stnlist = NULL;
    nw->crdsys = NULL;
    nw->geosys = NULL;
    nw->topolat = 0;
    nw->topolon = 0;
    nw->got_topocentre = 0;
    nw->options = 0;
    nw->orderclsid = 0;
    nw->initstation = default_initstation;
    nw->uninitstation = default_uninitstation;
    init_classifications( &(nw->stnclasses));
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

void set_network_name( network *nw, const char *n )
{
    char *pn;
    if( nw->name ) {check_free( nw->name ); nw->name = NULL; }
    while( *n == ' ' || *n == '\n' ) n++;
    if( !*n || *n=='\n') return;
    nw->name = copy_string( n );
    for( pn = nw->name; *pn; pn++ ) {if( *pn == '\n' ) *pn = 0;}
}

static void uninit_station( station *st, void *pnw )
{
    network *nw=(network *)pnw;
    nw->uninitstation( st );
}

void clear_network( network *nw )
{
    if( nw->uninitstation )
    {
        process_stations( nw, nw, uninit_station );
    }
    if( nw->name ) { check_free( nw->name ); nw->name = 0; }
    if( nw->crdsysdef ) { check_free( nw->crdsysdef ); nw->crdsysdef = 0; }
    if( nw->stnlist ) { delete_station_list( nw->stnlist ); nw->stnlist = 0; }
    if( nw->crdsys ) { delete_coordsys( nw->crdsys ); nw->crdsys = 0; }
    if( nw->geosys ) { delete_coordsys( nw->geosys ); nw->geosys = 0; }
    delete_classifications( &(nw->stnclasses));
}

void delete_network( network *nw )
{
    clear_network( nw );
    check_free( nw );
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
