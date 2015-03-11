#include "snapconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network/network.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/datafile.h"
#include "util/dateutil.h"
#include "util/fileutil.h"
#include "util/geodetic.h"
#include "util/dms.h"
#include "util/errdef.h"
#include "util/pi.h"

#define STN_TS_STEP 0
#define STN_TS_SERIES 1
#define STN_TS_VELOCITY 2

#define STN_TS_ENU 0
#define STN_TS_XYZ 1

/* Buffer size for reading words in offset file */
#define WORDLEN 32

typedef struct
{
    double date;
    vector3 denu;
} stn_tspoint;

/* Note: tspoints is allocated in same allocation as stn_offset_comp
 * if it is required  */

typedef struct stn_offset_comp_s
{
    int mode;
    int isxyz;
    int ntspoints;
    stn_tspoint basepoint;
    stn_tspoint *tspoints;
    struct stn_offset_comp_s *next;
} stn_offset_comp;

typedef struct
{
    int isdeformation;
    stn_offset_comp *components;

} stn_offset;

stn_offset *add_offset(stn_offset *poffset, const char *offsetdef );
int calc_stn_offset( station *st, double date,  double *denu );
int stn_offset_is_deformation( station *st );

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
        int sts=OK;
        int ists=0;
        int stnid;
        station *stn;
        stn_offset_comp *component;
        stn_offset *sto;

        sts =  df_read_code( tsf, stcode, STNCODELEN+1 );
        stnid=find_station(nw,stcode);
        if( stnid == 0 ) continue;

        if( sts != OK ) continue;
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

        for( ists=-1; ! df_end_of_line(tsf); ists++ )
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
            if( mode == STN_TS_SERIES || ists >= 0 )
            {
                sts=df_read_field( tsf, word, WORDLEN+1 );
                if( sts==OK ) 
                {
                    tsp->date=snap_datetime_parse(word,0);
                    if( tsp->date==0) sts=INVALID_DATA;
                }
                tsp->denu[0]=tsp->denu[1]=0.0;
                for( int iord=ord0; iord<3; iord++ )
                {
                    if( sts == OK ) sts=df_read_double( tsf, &(tsp->denu[iord]) );
                }
            if( sts != OK ) break;
            if( mode != STN_TS_SERIES ) break;

            }
        }
        if( sts==OK && ists < 0 ) sts=MISSING_DATA;
        if( sts==OK && ! df_end_of_line(tsf) ) sts=TOO_MUCH_DATA;
        if( sts != OK )
        {
            result=sts;
            df_data_file_error(tsf,sts,"Invalid data in station offset file");
            continue;
        }
        stn=station_ptr(nw, stnid);
        component=(stn_offset_comp *) check_malloc( sizeof(stn_offset_comp)+ists*sizeof(stn_tspoint));
        component->mode = mode;
        component->isxyz=isxyz;
        component->ntspoints=ists;
        component->tspoints = ists == 0 ? 0 :
            (stn_tspoint *)(void *)(((char*)component)+sizeof(stn_offset_comp));
        memcpy(&(component->basepoint),&basepoint,sizeof(stn_tspoint));
        if( ists > 0 )
        {
            memcpy(component->tspoints,tsdata,ists*sizeof(stn_tspoint));
        }
        if( ! stn->ts )
        {
            sto=(stn_offset *) check_malloc(sizeof(stn_offset));
            sto->isdeformation=0;
            sto->components=0;
            stn->ts=sto;
        }
        else
        {
            sto=(stn_offset *)stn->ts;
        }
        if( isdef ) sto->isdeformation=1;
        if( ! sto->components )
        {
            sto->components=component;
        }
        else
        {
            stn_offset_comp *prev=sto->components;
            while( prev->next ) prev=prev->next;
            prev->next=component;
        }
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
            double factor=date-tsp->date;
            veccopy( tsp->denu, cenu );
            scalevec( cenu, factor );
        }
        else if( comp->mode==STN_TS_VELOCITY && comp->ntspoints > 0 )
        {
            stn_tspoint *tsp1=tsp;
            double factor=0.0;
            int nts=comp->ntspoints;
            if( date >= tsp->date )
            {
                tsp1=comp->tspoints;
                while( nts > 0 && date >= tsp1->date )
                {
                    tsp=tsp1;
                    tsp1++;
                    nts--;
                }
            }
            if( date >= tsp1->date ) 
            {
                factor=1.0;
            }
            else
            {
                factor=(date-tsp->date)/(tsp1->date-tsp->date);
                vecadd2( tsp->denu, 1.0-factor, tsp1->denu, factor, cenu );
            }
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
