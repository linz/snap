#include "snapconfig.h"
/*
   $Log: networkm.c,v $
   Revision 1.1  1995/12/22 17:38:10  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/chkalloc.h"
#include "util/errdef.h"
#include "coordsys/coordsys.h"
#include "network/network.h"


static char rcsid[]="$Id: networkw.c,v 1.1 1995/12/22 17:38:10 CHRIS Exp $";

/*=============================================================*/
/* Routine to merge two networks.                              */

int merge_network( network *base, network *data, int mergeopts,
                   int (*select)(station *st) )
{

    coord_conversion cconv;
    double llh[3], xeu[3];
    int istn;
    int convertcoords;
    station *st, *stnew;
    station **stnewlist = 0;
    station **stdellist = 0;
    int nnew = 0;
    int *classmap=NULL;
    int nclass=0;

    int overwrite = mergeopts & NW_MERGEOPT_OVERWRITE;
    int createclass = mergeopts & NW_MERGEOPT_MERGECLASSES;
    int ndata;
    int idata;
    int i;

    convertcoords = ! identical_coordinate_systems( data->geosys, base->geosys );
    if( convertcoords && (define_coord_conversion( &cconv, data->geosys, base->geosys ) != OK) )
    {
        handle_error( INCONSISTENT_DATA, "Networks to merge have incompatible coordinate systems", NULL );
        return INCONSISTENT_DATA;
    }

    ndata = number_of_stations( data );
    if( ndata == 0 ) return OK;

    stnewlist = (station **) check_malloc( 2*ndata * sizeof(station *));
    stdellist = stnewlist + ndata;
    nnew = 0;

    /* As the find_station function uses a sorted list, and adding a station resorts it
       find all the stations first, then process them */

    for( idata = 1; idata <= ndata; idata ++ )
    {
        st = station_ptr( data, idata );
        if( select && !(*select)(st)) continue;
        istn = find_station( base, st->Code );
        if( istn && ! overwrite ) continue;
        stnewlist[nnew] = st;
        stdellist[nnew] = istn ? station_ptr(base,istn) : 0;
        nnew++;
    }

    if( nnew == 0 ) { check_free(stnewlist); return OK; }

    nclass = network_classification_count(data);
    if( nclass > 0 )
    {
        int i;
        classmap = (int *) check_malloc( (nclass+1) * sizeof(int));
        for( i = 1; i <= nclass; i++ )
        {
            classmap[i] = network_class_id( base, network_class_name(data,i), createclass);
        }
    }

    for( idata = 0; idata < nnew; idata++ )
    {
        st = stnewlist[idata];

        /* Convert the coordinates from the input system to the output
          system */


        llh[CRD_LAT] = st->ELat;
        llh[CRD_LON] = st->ELon;
        llh[CRD_HGT] = st->OHgt + st->GUnd;
        xeu[CRD_LAT] = st->GXi;
        xeu[CRD_LON] = st->GEta;
        xeu[CRD_HGT] = st->GUnd;

        if( convertcoords ) convert_coords( &cconv, llh, xeu, llh, xeu );

        if( stdellist[idata] ) remove_station( base, stdellist[idata]);

        stnew = new_network_station( base, st->Code, st->Name,
                                     llh[CRD_LAT], llh[CRD_LON], st->OHgt,
                                     xeu[CRD_LAT], xeu[CRD_LON], xeu[CRD_HGT] );

        for( i = 1; i <= nclass; i++ )
        {
            if( classmap[i] > 0 )
            {
                const char *classval = network_class_value( data, i, get_station_class(st,i));
                int tgtval = network_class_value_id(base,classmap[i],classval,1);
                set_station_class( stnew, classmap[i],tgtval );
            }
        }
    }

    check_free( stnewlist );
    if( classmap ) check_free( classmap );

    return OK;
}
