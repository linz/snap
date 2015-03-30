#include "snapconfig.h"
/* Routines for managing station code translations */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "snapdata/stnrecode.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "snapdata/datatype.h"
#include "util/datafile.h"
#include "snapdata/loaddata.h"
#include "snapdata/survdata.h"
#include "util/dateutil.h"
#include "util/dstring.h"
#include "util/errdef.h"
#include "util/linklist.h"
#include "util/symmatrx.h"
#include "util/progress.h"
#include "util/pi.h"

static stn_recode *create_stn_recode( const char *codeto, double datefrom, double dateto )
{
    stn_recode *sct=(stn_recode *) check_malloc(sizeof(stn_recode)+strlen(codeto)+1);
    char *sctcodeto = ((char *)(void *)sct)+sizeof(stn_recode);
    strcpy(sctcodeto,codeto);
    sct->codeto=codeto;
    sct->datefrom=datefrom;
    sct->dateto=dateto;
    sct->used=RECODE_UNUSED;
    sct->next=0;
    return sct;
}

static stn_recode_list *create_stn_recode_list( const char *codefrom )
{
    stn_recode_list *sctl=(stn_recode_list *) check_malloc(sizeof(stn_recode_list)+strlen(codefrom)+1);
    char *sctcodefrom = ((char *)(void *)sctl)+sizeof(stn_recode_list);
    strcpy(sctcodefrom,codefrom);
    sctl->codefrom=sctcodefrom;
    sctl->translations=0;
    sctl->next=0;
    return sctl;
}

static void add_stn_recode_to_list( stn_recode_list *list, stn_recode *trans )
{
    trans->next=list->translations;
    list->translations=trans;
}

static void delete_stn_recode_list( stn_recode_list *list )
{
    stn_recode *sct=list->translations;
    while( sct )
    {
        stn_recode *next=sct->next;
        check_free( sct );
        sct=next;
    }
    check_free( list );
}


stn_recode_map *create_stn_recode_map( network *net, get_recode_station_func getstation, void *getstationdata )
{
    stn_recode_map *stt=(stn_recode_map *) check_malloc( sizeof(stn_recode_map));
    stt->stlists=0;
    stt->index=0;
    stt->net=net;
    stt->getstationdata=getstationdata;
    stt->getstation=getstation;
    return stt;
}

void delete_stn_recode_map( stn_recode_map *stt, network *net ) {
    stn_recode_list *lists=stt->stlists;
    while( lists )
    {
        stn_recode_list *next=lists->next;
        delete_stn_recode_list(lists);
        lists=next;
    }
    if( stt->index ) check_free( stt->index );
    stt->index=0;
    stt->stlists=0;
    stt->getstationdata=0;
    stt->getstation=0;
    check_free( stt );
}

void add_stn_recode_to_map( stn_recode_map *stt, const char *codefrom, const char *codeto, double datefrom, double dateto )
{
    stn_recode_list *stlist=stt->stlists;
    stn_recode *stc;
    while( stlist )
    {
        if( _stricmp(stlist->codefrom,codefrom) == 0 ) break;
        stlist=stlist->next;
    }
    if( ! stlist )
    {
        stlist=create_stn_recode_list( codefrom );
        stlist->next=stt->stlists;
        stt->stlists=stlist;
    }
    stc=create_stn_recode( codeto, datefrom, dateto );
    add_stn_recode_to_list( stlist, stc );
}

typedef struct 
{
    stn_recode_map *srm;
    char *suffix;
    double datefrom;
    double dateto;
} stn_recode_data;

static void apply_recode_suffix( station *st, void *psrd )
{
    char codeto[STNCODELEN+1];
    stn_recode_data *srd=(stn_recode_data *) psrd;
    char *suffix=srd->suffix;
    int sfxlen=strlen(suffix);
    const char *codefrom=st->Code;
    int codelen=strlen(codefrom);

    /* Check if reprocessing a code for which a suffix is already applied */
    if( codelen > sfxlen && _stricmp(suffix,codefrom+(codelen-sfxlen))==0 ) return;
    if( sfxlen > STNCODELEN ) return;

    strcpy(codeto,codefrom);
    codeto[STNCODELEN-sfxlen]=0;
    strcat(codeto,suffix);
    add_stn_recode_to_map( srd->srm, codefrom, codeto, srd->datefrom, srd->dateto );
}

static int parse_stn_trans_def( stn_recode_map *stt, char *def, char *basefile )
{
    char msg[80];
    char *field;
    char *codefrom;
    char *codeto;
    double datefrom;
    double dateto;
    char *suffix;
    int ok=1;

/*
 
   recode xxx to yyyy
   recode xxx to yyyy between date and date
   recode xxx to yyyy before date 
   recode xxx to yyyy after date 
   recode suffix xxx between/before/after for station_list
*/
    field=next_field( &def );
    if( ! field )
    {
        strcpy(msg,"Missing source station code in recode definition");
        ok=0;
    }
    else if ( _stricmp(field,"suffix") == 0 )
    {
        suffix=next_field(&def);
    }
    else
    {
        codefrom=field;
        field=next_field( &def );
        if( field && _stricmp(field,"to")==0 ) field=next_field( &def );
        codeto = field;
        if( ! codeto )
        {
            strcpy(msg,"Missing target station code in recode definition");
            ok=0;
        }
    }

    datefrom=UNDEFINED_DATE;
    dateto=UNDEFINED_DATE;
    field=next_field(&def);
    if( field )
    {
        char *fromdef=0;
        char *todef=0;
        if( _stricmp(field,"between")==0 )
        {
            fromdef=next_field(&def);
            if( ! fromdef )
            {
                strcpy(msg,"Date missing after \"before\"");
                ok=0;
            }
            if( ok )
            {
                field=next_field(&def);
                if( ! ( field && _stricmp(field,"to") == 0 ) )
                {
                    strcpy(msg,"\"to\" missing after \"between\"");
                    ok=0;
                }
            }
            if( ok )
            {
                todef=next_field(&def);
                if( ! todef )
                {
                    strcpy(msg,"Date missing after \"to\"");
                    ok=0;
                }
            }
        }
        else if( _stricmp(field,"before")==0 )
        {
            fromdef=next_field(&def);
            if( ! datefrom )
            {
                strcpy(msg,"Date missing after \"before\"");
                ok=0;
            }
        }
        else if( _stricmp(field,"after")==0 )
        {
            todef=next_field(&def);
            if( ! dateto )
            {
                strcpy(msg,"Date missing after \"after\"");
                ok=0;
            }
        }
        else 
        {
            sprintf(msg,"Undefined field %.50s",field);
            ok=0;
        }
        if( ok && fromdef )
        {
            datefrom=snap_datetime_parse( fromdef, 0 );
            if( ! datefrom )
            {
                sprintf(msg,"Invalid from date \"%.50s\"",fromdef);
                ok=0;
            }
        }
        if( ok && todef )
        {
            dateto=snap_datetime_parse( todef, 0 );
            if( ! dateto )
            {
                sprintf(msg,"Invalid from date \"%.50s\"",fromdef);
                ok=0;
            }
        }
    }

    if( suffix && ok )
    {
        if( ! *def )
        {
            strcpy(msg,"Station list missing from recode suffix definition");
            ok=0;
        }
        else
        {
            stn_recode_data srd;
            srd.srm=stt;
            srd.suffix=suffix;
            srd.datefrom=datefrom;
            srd.dateto=dateto;
            process_selected_stations( stt->net, def, basefile, &srd, apply_recode_suffix );
        }
    }
    else if( ok )
    {
        add_stn_recode_to_map( stt, codefrom, codeto, datefrom, dateto );
    }

    if( ! ok )
    {
        handle_error(INVALID_DATA,"Error reading station recoding",msg);
    }
    return ok ? OK : INVALID_DATA;
}


int read_stn_recode_map( DATAFILE *d, stn_recode_map *stt )
{
}

/*
 



*/

const char *get_recode( stn_recode_map *stt, stn_recode_map *sttbase, const char *code, double *date )
{
}

