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


void process_stations( network *nw, void *data, void (*function)( station *st, void *data ) )
{
    if( nw->stnlist ) sl_process_stations( nw->stnlist, data, function );
}

static void process_selected_stations1( network *nw, char *select, const char *basefile, void *data, void (*function)( station *st, void *data ), char *stnfile );

static void process_station_file_list(network *nw, char *file, const char *basefile, void *data, void (*function)(station *st, void *data))
{
    const char *spec;
    FILE *list_file;
    char buf[2048];

    spec = find_file( file,DFLTSTLIST_EXT,basefile,1,0);
    list_file = NULL;
    if( spec ) list_file = fopen( spec, "r" );

    if( !list_file )
    {
        char errmess[80];
        sprintf(errmess,"Cannot open station list file %.40s\n",file);
        handle_error( INVALID_DATA, errmess, NULL  );
        return;
    }

    while( fgets(buf,2048,list_file) )
    {
        char *b = buf;
        while( *b && isspace(*b) ) b++;
        if( ! *b || *b == COMMENT_CHAR ) continue;
        process_selected_stations1(nw,b,basefile,data,function,file);
    }
}

static void process_selected_stations1( network *nw, char *select, const char *basefile,
                                        void *data, void (*function)( station *st, void *data ), char *stnfile )
{
    char *field;
    char *s = select;
    char end = 0;
    char *delim;
    int istn;

    while( 1 )
    {
        /* Skip leading blanks */

        if( end ) *s = end;
        while( isspace(*s) ) s++;
        if( ! *s ) break;

        field = s;
        while( *s && ! isspace(*s) ) s++;
        end = *s;
        if( end ) *s = 0;

        _strupr(field);
        istn = find_station( nw, field );

        /* Is the string matched as a station */

        if( istn )
        {
            (*function)(station_ptr(nw,istn),data);
            continue;
        }

        /* If this reference a file of station definitions, then process the file.
           Not allowed if this is already in a station list file. */

        if( ! stnfile && field[0] == '@' && field[1] )
        {
            process_station_file_list( nw,field+1,basefile,data,function);
            continue;
        }

        /* If this is a classification criteria */

        for( delim = field+1; *delim && *delim != '='; delim++);
        if( *delim )
        {
            char *value = delim+1;
            int class_id = 0;
            int value_id = CLASS_VALUE_NOT_DEFINED;
            *delim = 0;
            class_id = network_class_id( nw, field, 0 );
            if( class_id )
            {
                value_id = network_class_value_id( nw, class_id, value, 0 );
            }
            if( value_id != CLASS_VALUE_NOT_DEFINED )
            {
                for( istn = number_of_stations(nw); istn; istn-- )
                {
                    station *st = station_ptr(nw,istn);
                    if( get_station_class(st,class_id) == value_id )
                    {
                        (*function)(st,data);
                    }
                }
            }
            *delim = '=';
            continue;
        }

        /* Is it matched as a range? */

        for( delim = field+1; *delim && *delim != '-'; delim++);
        if( *delim )
        {
            int ist1, ist2;
            *delim = 0;
            if( 0 != (ist1=find_station( nw,field)) &&
                    0 != (ist2=find_station( nw,delim+1)) &&
                    ist2 >= ist1 )
            {


                while( ist1 <= ist2 )
                {
                    (*function)(station_ptr(nw,ist1),data);
                    ist1++;
                }
                continue;
            }
            *delim = '-';
        }

        /* Bother - it must be a mistake */

        {
            char errmess[200];
            if( stnfile )
            {
                sprintf(errmess,"Invalid station %.50s in station list file %.50s",field,stnfile);
            }
            else
            {
                sprintf(errmess,"Invalid station %.50s in list of stations",field);
            }
            handle_error(INVALID_DATA,errmess,NULL);
        }
    }
}

void process_selected_stations( network *nw, const char *select, const char *basefile,
                                void *data, void (*function)( station *st, void *data ))
{
    char *sel = copy_string(select);
    process_selected_stations1( nw, sel, basefile, data, function, NULL);
    check_free(sel);
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
