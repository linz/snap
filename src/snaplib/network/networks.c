#include "snapconfig.h"
/* netlist2.c: Station list functions accessed via network */

/*
   $Log: networks.c,v $
   Revision 1.2  1998/05/21 04:00:28  ccrook
   Added geodetic coordinate system to network object and facilitated getting and setting
   coordinates in the network coordinate system.

   Revision 1.1  1995/12/22 17:37:17  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "network/network.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/fileutil.h"
#include "util/errdef.h"
#include "util/polygon.h"
#include "util/pi.h"


#define COMMENT_CHAR '!'

static int convert_stn_coords( coord_conversion *ccv, station *st, int testonly )
{
    double llh[3], xeu[3];
    int sts;

    /* Convert the coordinates from the input system to the output
       system */

    llh[CRD_LAT] = st->ELat;
    llh[CRD_LON] = st->ELon;
    llh[CRD_HGT] = st->OHgt + st->GUnd;
    xeu[CRD_LAT] = st->GXi;
    xeu[CRD_LON] = st->GEta;
    xeu[CRD_HGT] = st->GUnd;

    sts=convert_coords( ccv, llh, xeu, llh, xeu );

    if( testonly ) return sts; 

    st->GXi  = xeu[CRD_LAT];
    st->GEta = xeu[CRD_LON];
    st->GUnd = xeu[CRD_HGT];

    /* Update the station components that are defined in terms of the
       ellipsoid */

    modify_station_coords( st, llh[CRD_LAT], llh[CRD_LON], st->OHgt, ccv->to->rf->el );
    return OK;

}


int set_network_coordsys( network *nw, coordsys *cs, double epoch, char *errmsg, int nmsg )
{
    coordsys *geosys, *csold;
    coord_conversion cconv;
    station *st;

    /* If we already have a coordinate system and some stations defined,
       then we need to transform the stations from the input system to
       the new system. The input and output systems must have a common
       reference frame to permit this. Note that we create coordinate
       systems csf and cst which do not include the projection because
       the coordinates are lat/long, not projection coords.  */

    geosys = related_coordsys( cs, CSTP_GEODETIC );
    csold = nw->crdsys;

    if( csold )
    {
        if( nw->stnlist )
        {
            int sts;
            sts=define_coord_conversion_epoch( &cconv, nw->geosys, geosys, epoch );

            /* Trial conversion to check coordinates can be converted */

            reset_station_list( nw, 0 );
            while( sts == OK && NULL != (st = next_station(nw)) )
            {
                sts=convert_stn_coords( &cconv, st, 1 );
                if( sts != OK ) break;
            }

            if( sts != OK )
            {
                if( errmsg && cconv.errmsg[0] )
                {
                    strncpy(errmsg,cconv.errmsg,nmsg);
                    errmsg[nmsg-1]=0;
                }
                delete_coordsys( geosys );
                return INCONSISTENT_DATA;
            }

            /* If all OK, then actually convert coordinates */

            reset_station_list( nw, 0 );
            while( NULL != (st = next_station(nw)) )
            {
                sts=convert_stn_coords( &cconv, st, 0 );
            }
        }

        delete_coordsys( csold );
        delete_coordsys( nw->geosys );
    }

    nw->crdsys = copy_coordsys( cs );
    nw->geosys = geosys;
    define_coord_conversion( &nw->ccgeo, nw->crdsys, nw->geosys );
    define_coord_conversion( &nw->ccnet, nw->geosys, nw->crdsys );

    if( nw->crdsysdef ) check_free(nw->crdsysdef);
    nw->crdsysdef = copy_string( cs->code );

    return OK;
}

int add_station( network *nw, station *st )
{

    /* Must have a coordinate system defined if we want to define stations */

    if( !nw->crdsys ) return INCONSISTENT_DATA;

    if( !nw->stnlist ) nw->stnlist = new_station_list();

    /* Make sure station is on the right ellipsoid */

    modify_station_coords( st, st->ELat, st->ELon, st->OHgt, nw->crdsys->rf->el );

    sl_add_station( nw->stnlist, st );

    if( nw->initstation ) nw->initstation( st );

    return OK;
}


station * new_network_station( network *nw,
                               const char *code, const char *Name,
                               double Lat, double Lon, double Hgt,
                               double Xi, double Eta, double Und )
{

    station *st;

    if( !nw->crdsys ) return NULL;

    st = new_station();

    init_station( st, code, Name, Lat, Lon, Hgt, Xi, Eta, Und, nw->crdsys->rf->el );

    init_station_classes( st, network_classification_count(nw) );

    add_station( nw, st );

    return st;
}

station * duplicate_network_station( network *nw,
        station *st, const char *newcode, const char *newname )
{

    station *stnew;

    if( !nw->crdsys ) return NULL;

    stnew = new_station();

    init_station( stnew, newcode, newname, 
            st->ELat, st->ELon, st->OHgt, 
            st->GXi, st->GEta, st->GUnd, nw->crdsys->rf->el );

    init_station_classes( stnew, network_classification_count(nw) );

    add_station( nw, stnew );
    if( stnew->nclass == st->nclass )
    {
        memcpy( stnew->classval, st->classval, sizeof(int)*st->nclass);
    }

    return st;
}


void modify_network_station_coords( network *nw, station *st, double Lat,
                                    double Lon, double Hgt )
{
    modify_station_coords( st, Lat, Lon, Hgt, nw->crdsys->rf->el );
}


void get_network_coordinates( network *nw, station *st, double crd[3] )
{
    double gcrd[3];
    gcrd[CRD_LAT] = st->ELat;
    gcrd[CRD_LON] = st->ELon;
    gcrd[CRD_HGT] = st->OHgt + st->GUnd;
    convert_coords( & nw->ccnet, gcrd, NULL, crd, NULL );
}


void set_network_coordinates( network *nw, station *st, double crd[3] )
{
    double gcrd[3];
    convert_coords( & nw->ccgeo, crd, NULL, gcrd, NULL );
    modify_network_station_coords( nw, st, gcrd[CRD_LAT], gcrd[CRD_LON],
                                   gcrd[CRD_HGT]-st->GUnd );
}

void remove_station( network *nw, station *st )
{
    if( nw->stnlist ) sl_remove_station( nw->stnlist, st );
}


int find_station( network *nw, const char *code )
{
    if( !nw->stnlist ) return 0;
    return sl_find_station( nw->stnlist, code );
}

int station_id( network *nw, station *st )
{
    if( !nw->stnlist ) return 0;
    return sl_station_id( nw->stnlist, st );
}


station *station_ptr( network *nw, int istn )
{
    if( !nw->stnlist ) return NULL;
    return sl_station_ptr( nw->stnlist, istn );
}

int   find_station_sorted_id( network *nw, const char *code )
{
    if( !nw->stnlist ) return 0;
    return sl_find_station_sorted_id( nw->stnlist, code );
}

station *station_sorted_ptr( network *nw, int sortedid )
{
    if( !nw->stnlist ) return 0;
    return sl_station_sorted_ptr( nw->stnlist, sortedid );
}

void process_stations( network *nw, void *data, void (*function)( station *st, void *data ) )
{
    if( nw->stnlist ) sl_process_stations( nw->stnlist, data, function );
}

void reset_station_list( network *nw, int sorted )
{
    if( nw->stnlist ) sl_reset_station_list( nw->stnlist, sorted );
}

station *next_station( network *nw )
{
    if( !nw->stnlist ) return NULL;
    return sl_next_station( nw->stnlist );
}


int number_of_stations( network *nw )
{
    return nw->stnlist ? sl_number_of_stations(nw->stnlist) : 0;
}
