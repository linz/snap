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


/*=============================================================*/
/* Routine to merge two networks.                              */

int merge_network( network *base, network *data, int mergeopts,
        double mergedate, int (*select)(station *st) )
{

    coord_conversion cconv;
    double llh[3], exu[3];
    int istn;
    int convertcoords;
    station *st, *stnew;
    station **stnewlist = 0;
    station **stdellist = 0;
    ellipsoid *el=base->crdsys->rf->el;
    int nnew = 0;
    int *classmap=NULL;
    int nclass=0;

    int overwrite = mergeopts & NW_MERGEOPT_OVERWRITE;
    int addnew = mergeopts & NW_MERGEOPT_ADDNEW;
    int addclasses = mergeopts & NW_MERGEOPT_ADDCLASSES;
    int updateexu = ! overwrite && (mergeopts & NW_MERGEOPT_EXU);
    int updatecrd = ! overwrite && (mergeopts & (NW_MERGEOPT_COORDS | NW_MERGEOPT_EXU));
    int updatecls = ! overwrite && (mergeopts & NW_MERGEOPT_CLASSES);
    int updatestn = updatecrd || updatecls || overwrite;
    int ndata;
    int nbaseclass;
    int nclassnew;
    int idata;
    int preserve_ellipsoidal;
    int i;

    convertcoords = ! identical_coordinate_systems( data->geosys, base->geosys );
    if( convertcoords && (define_coord_conversion_epoch( &cconv, data->geosys, base->geosys, mergedate ) != OK) )
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
        if( istn && ! updatestn ) continue;
        if( ! istn && ! addnew ) continue;
        stnewlist[nnew] = st;
        stdellist[nnew] = istn ? station_ptr(base,istn) : 0;
        nnew++;
    }

    if( nnew == 0 ) { check_free(stnewlist); return OK; }

    nclass = network_classification_count(data);
    nbaseclass=network_classification_count(base);
    if( nclass > 0 )
    {
        int i;
        classmap = (int *) check_malloc( (nclass+1) * sizeof(int));
        for( i = 1; i <= nclass; i++ )
        {
            classmap[i] = network_class_id( base, network_class_name(data,i), addclasses);
        }
    }
    nclassnew=network_classification_count(base);
    if( nclassnew == nbaseclass ) nclassnew=0;

    preserve_ellipsoidal=0;
    if( mergeopts & NW_HGTFIXEDOPT_ELLIPSOIDAL )
    {
        preserve_ellipsoidal=1;
    }
    else if( ! (mergeopts & NW_HGTFIXEDOPT_ORTHOMETRIC ))
    {
        if( ! network_has_geoid_info(data) && 
            network_height_coord_is_ellipsoidal(data) &&
            network_has_geoid_info(base) )
        {
            preserve_ellipsoidal=1;
        }
        else 
        if( network_has_geoid_info(data) && 
            network_height_coord_is_ellipsoidal(base) &&
            ! network_has_geoid_info(base) )
        {
            preserve_ellipsoidal=1;
        }
    }

    for( idata = 0; idata < nnew; idata++ )
    {
        int loadclass=0;
        station *st0 = stdellist[idata];
        st = stnewlist[idata];


        /* Convert the coordinates from the input system to the output
          system if required */

        if( ! st0 || overwrite || (st0 && updatecrd ) )
        {
            llh[CRD_LAT] = st->ELat;
            llh[CRD_LON] = st->ELon;
            llh[CRD_HGT] = st->OHgt + st->GUnd;
            exu[CRD_LAT] = st->GXi;
            exu[CRD_LON] = st->GEta;
            exu[CRD_HGT] = st->GUnd;
            if( convertcoords ) convert_coords( &cconv, llh, exu, llh, exu );
            if( ! network_has_geoid_info(data) )
            {
                exu[CRD_LAT]=0.0;
                exu[CRD_LON]=0.0;
                exu[CRD_HGT]=0.0;
                if( ! network_height_coord_is_ellipsoidal(data) )
                {
                    llh[CRD_HGT] = st->OHgt;
                }
            }
            else
            {
                llh[CRD_HGT] -= exu[CRD_HGT];
            }
        }

        stnew=0;
        if( st0 )
        {
            if( overwrite )
            {
                remove_station( base, st0 );
            }
            else
            {
                stnew=st0;
                if( updateexu ) 
                {
                    modify_station_coords_xeu( st0, 
                        llh[CRD_LAT],llh[CRD_LON],llh[CRD_HGT], 
                        exu[CRD_LAT],exu[CRD_LON],exu[CRD_HGT], 
                        el );
                }
                else if( updatecrd )
                {
                    if( preserve_ellipsoidal )
                    {
                        llh[CRD_HGT] += exu[CRD_HGT];
                        llh[CRD_HGT] -= st0->GUnd;
                    }
                    modify_station_coords( st0, 
                        llh[CRD_LAT],llh[CRD_LON],llh[CRD_HGT], el );
                }
                loadclass=updatecls;
                if( nclassnew ) init_station_classes( st0, nclassnew );
            }
        }

        if( ! stnew )
        {
            stnew = new_network_station( base, st->Code, st->Name,
                                     llh[CRD_LAT], llh[CRD_LON], llh[CRD_HGT],
                                     exu[CRD_LAT], exu[CRD_LON], exu[CRD_HGT] );
            loadclass=1;
        }
        if( loadclass )
        {
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
    }

    check_free( stnewlist );
    if( classmap ) check_free( classmap );

    return OK;
}
