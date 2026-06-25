#include "snapconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network/network.h"
#include "network/stnoffset.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/datafile.h"
#include "util/dateutil.h"
#include "util/fileutil.h"
#include "util/geodetic.h"
#include "util/dms.h"
#include "util/errdef.h"
#include "util/pi.h"

/* Buffer size for reading words in offset file */
#define WORDLEN 32

stn_offset_comp *create_stn_offset_comp( int mode, int isxyz, int ntspoints )
{
    stn_offset_comp *component;
    component=(stn_offset_comp *) check_malloc( sizeof(stn_offset_comp)+ntspoints*sizeof(stn_tspoint));
    component->mode = mode;
    component->isxyz=isxyz;
    component->ntspoints=ntspoints;
    component->tspoints = ntspoints == 0 ? 0 :
        (stn_tspoint *)(void *)(((char*)component)+sizeof(stn_offset_comp));
    component->next=0;
    return component;
}

void add_stn_offset_comp_to_station( station *st, stn_offset_comp *comp, int isdeformation )
{
    stn_offset *sto;
    if( ! st->ts )
    {
        sto=(stn_offset *) check_malloc(sizeof(stn_offset));
        sto->isdeformation=0;
        sto->components=0;
        st->ts=sto;
    }
    else
    {
        sto=(stn_offset *)(st->ts);
    }
    if( isdeformation ) sto->isdeformation=1;
    if( ! sto->components )
    {
        sto->components=comp;
    }
    else
    {
        stn_offset_comp *prev=sto->components;
        while( prev->next ) prev=prev->next;
        prev->next=comp;
    }
}

/*=============================================================*/
/* Read station offsets from a station offset file             */
/* Reads a SNAP format file, or the very similar geodetic      */


int read_network_station_offsets( network *nw, const char *filename )
{
    DATAFILE *tsf;
    char stcode[STNCODELEN+1];
    int result=OK;
    int maxtsdata=0;
    stn_tspoint *tsdata=0;
    stn_tspoint basepoint;
    char word[WORDLEN+1];

    tsf = df_open_data_file( filename, "station offset file" );
    if( tsf == NULL ) return FILE_OPEN_ERROR;

    /* Read in the coordinate system definition */

    while( df_read_data_file(tsf) == OK )
    {
        int isxyz=STN_TS_ENU;
        int ord0=0;
        int isdef=0;
        int mode=STN_TS_STEP;
        int ok=1;
        int sts=OK;
        int ists=0;
        int stnid;
        station *stn;
        stn_offset_comp *component;

        ok =  df_read_code( tsf, stcode, STNCODELEN+1 );
        if( ! ok ) continue;

        stnid=find_station(nw,stcode);
        if( stnid == 0 ) continue;

        if( df_read_field( tsf, word, WORDLEN+1 ) )
        {
            if( _stricmp(word,"xyz") == 0 )
            {
                isxyz=STN_TS_XYZ;
            }
            else if( _stricmp(word,"height") == 0 )
            {
                ord0=2;
            }
            else if( _stricmp(word,"enu") != 0 )
            {
                df_reread_field( tsf );
            }
        }
        if( df_read_field( tsf, word, WORDLEN+1 ) )
        {
            if( _stricmp(word,"deformation") == 0 )
            {
                isdef=1;
            }
            else if( _stricmp(word,"offset") != 0 )
            {
                df_reread_field( tsf );
            }
        }
        if( df_read_field( tsf, word, WORDLEN+1 ) )
        {
            if( _stricmp(word,"velocity") == 0 )
            {
                mode=STN_TS_VELOCITY;
            }
            else if( _stricmp(word,"time_series") == 0 )
            {
                mode=STN_TS_SERIES;
                basepoint.date=0;
            }
            else if( _stricmp(word,"step") != 0 )
            {
                df_reread_field( tsf );
            }
        }


        ists=-1; 
        while( ! df_end_of_line(tsf) )
        {
            stn_tspoint *tsp;
            if( ists < 0 )
            {
                tsp=&basepoint;
            }
            else
            {
                if( ists >= maxtsdata )
                {
                    maxtsdata *= 2;
                    if( maxtsdata < 32 ) maxtsdata=32;
                    tsdata=(stn_tspoint *) check_realloc(tsdata,maxtsdata*sizeof(stn_tspoint));
                }
                tsp=tsdata+ists;
            }
            tsp->date=0.0;
            if( mode != STN_TS_SERIES || ists >= 0 )
            {
                ok=df_read_field( tsf, word, WORDLEN+1 );
                if( ok ) 
                {
                    tsp->date=snap_datetime_parse(word,0);
                    if( tsp->date==0) ok=0;
                }
            }
            tsp->denu[0]=tsp->denu[1]=tsp->denu[2]=0.0;
            for( int iord=ord0; iord<3; iord++ )
            {
                if( ok ) ok=df_read_double( tsf, &(tsp->denu[iord]) );
            }
            if( ! ok ) break;
            ists++;
            if( mode != STN_TS_SERIES ) break;
        }

        if( ! ok ) sts=INVALID_DATA;
        else if( ists < 0 ) sts=MISSING_DATA;
        else if( ! df_end_of_line(tsf) ) sts=TOO_MUCH_DATA;
        else sts=OK;

        if( sts != OK )
        {
            result=sts;
            df_data_file_error(tsf,sts,"Invalid data in station offset file");
            continue;
        }
        stn=station_ptr(nw, stnid);
        component=create_stn_offset_comp( mode, isxyz, ists );
        memcpy(&(component->basepoint),&basepoint,sizeof(stn_tspoint));
        if( ists > 0 )
        {
            memcpy(component->tspoints,tsdata,ists*sizeof(stn_tspoint));
        }
        add_stn_offset_comp_to_station( stn, component, isdef );
    }
    if( tsdata ) check_free(tsdata);
    df_close_data_file(tsf);
    return result;
}

void delete_station_offset( station *st )
{
    if( st->ts )
    {
        stn_offset *sto=(stn_offset *) (st->ts);
        stn_offset_comp *comp=sto->components;
        while( comp )
        {
            stn_offset_comp *next=comp->next;
            check_free( comp );
            comp=next;
        }
        check_free( sto );
        st->ts=0;
    }
}

int station_has_offset( station *st )
{
    return st->ts ? 1 : 0;
}

int station_offset_is_deformation( station *st )
{
    return st->ts ? ((stn_offset *)(st->ts))->isdeformation : 0;
}

void calc_station_offset( station *st, double date, vector3 denu )
{
    stn_offset *sto=(stn_offset *) st->ts;
    stn_offset_comp *comp;

    denu[0]=denu[1]=denu[2]=0;
    if( ! sto ) return;
    for( comp=sto->components; comp; comp=comp->next )
    {
        vector3 cenu={0.0,0.0,0.0};
        stn_tspoint *tsp=&(comp->basepoint);
        if( comp->mode==STN_TS_STEP )
        {
            if( date < tsp->date ) continue;
            veccopy( tsp->denu, cenu );
        }
        else if( comp->mode==STN_TS_VELOCITY )
        {
            double factor=(date-tsp->date)/DAYS_PER_YEAR;
            veccopy( tsp->denu, cenu );
            scalevec( cenu, factor );
        }
        else if( comp->mode==STN_TS_SERIES && comp->ntspoints > 0 )
        {
            stn_tspoint *tsp1=tsp;
            double factor=0.0;
            int nts=comp->ntspoints-1;
            tsp1=comp->tspoints;
            if( date >= tsp1->date )
            {
                while( nts > 0 && date >= tsp1->date )
                {
                    tsp=tsp1;
                    tsp1++;
                    nts--;
                }
                if( date >= tsp1->date ) 
                {
                    factor=1.0;
                }
                else
                {
                    factor=(date-tsp->date)/(tsp1->date-tsp->date);
                }
            }
            vecadd2( tsp->denu, 1.0-factor, tsp1->denu, factor, cenu );
        }
        else
        {
            continue;
        }
        if( comp->isxyz == STN_TS_XYZ )
        {
            rotvec( cenu, &(st->rTopo), cenu );
        }
        vecadd( denu, cenu, denu );
    }
}

void print_station_offset( FILE *lst, station *st )
{
    stn_offset *sto=(stn_offset *)(st->ts);
    stn_offset_comp *comp;

    if( ! sto ) return;
    fprintf(lst,"%s %s\n",st->Code, sto->isdeformation ? "deformation" : "offset");
    for( comp=sto->components; comp; comp=comp->next )
    {
        stn_tspoint *tsp=&(comp->basepoint);
        for( int i = -1; i < comp->ntspoints; i++ )
        {
            int ndp=comp->mode==STN_TS_VELOCITY ? 6 : 4;
            char datestr[20]={0};
            if( i >= 0 ) tsp=comp->tspoints+i;
            if( i >= 0 || comp->mode != STN_TS_SERIES )
            {
                int y,m,d;
                date_as_ymd(tsp->date,&y,&m,&d);
                sprintf(datestr,"%02d-%02d-%04d",d,m,y);
            }
            fprintf(lst,"    %3s %-11s  %10s  %10.*lf %10.*lf %10.*lf\n",
                    i >= 0 ? "" :
                    comp->isxyz ? "XYZ" : "ENU",
                    i >= 0 ? "" :
                    comp->mode==STN_TS_SERIES ? "time series" :
                    comp->mode==STN_TS_VELOCITY ? "velocity" :
                    comp->mode==STN_TS_STEP ? "offset" : "undefined" ,
                    datestr,
                    ndp,tsp->denu[0],
                    ndp,tsp->denu[1],
                    ndp,tsp->denu[2]
                    );
        }
    }
}
