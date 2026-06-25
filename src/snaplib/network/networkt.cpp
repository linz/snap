#include "snapconfig.h"
/* networkt.c: functions handling the network topocentre */

/*
   $Log: networkt.c,v $
   Revision 1.1  1995/12/22 17:37:50  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>

#include "network/network.h"

static void calc_topocentre( network *nw )
{
    int i;
    station *st;
    double topolat, topolon;
    double vecu[3], tmp[3], r;

    for( i=0; i<3; i++ ) vecu[i] = 0.0;

    /* Add together the vertical vectors of all the stations to get a vector
       in the mean direction */

    topolat = topolon = 0.0;

    /* If there are any stations then calculate the sum of the verticals */

    if( number_of_stations(nw) )
    {

        for( reset_station_list( nw, 0 );
                NULL != (st = next_station(nw));
           )
        {
            rot_vertical( &st->rTopo, tmp );
            for( i = 0; i<3; i++ ) vecu[i] += tmp[i];
        }

        /* Calculate the corresponding latitude and longitude */

        r = _hypot(vecu[0],vecu[1]);
        topolat = atan2( vecu[2], r );
        if( r )
        {
            topolon = atan2( vecu[1], vecu[0] );
        }
    }

    set_network_topocentre( nw, topolat, topolon );
}


void set_network_topocentre( network *nw, double lat, double lon )
{
    nw->topolat = lat;
    nw->topolon = lon;
    nw->got_topocentre = 1;
}

void get_network_topocentre( network *nw, double *lat, double *lon )
{
    if( !nw->got_topocentre ) calc_topocentre(nw);
    *lat = nw->topolat;
    *lon = nw->topolon;
}

void get_network_topocentre_xyz( network *nw, double *xyz )
{
    double llh[3];
    get_network_topocentre( nw, &(llh[CRD_LAT]), &(llh[CRD_LON]) );
    llh[CRD_HGT]=0.0;
    llh_to_xyz( nw->crdsys->rf->el, llh, xyz, 0, 0 );
}

