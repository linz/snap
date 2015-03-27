#include "snapconfig.h"
/* Routines for managing station code translations */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "snapdata/stntrans.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "snapdata/datatype.h"
#include "util/datafile.h"
#include "snapdata/loaddata.h"
#include "snapdata/survdata.h"
#include "util/dateutil.h"
#include "util/errdef.h"
#include "util/linklist.h"
#include "util/symmatrx.h"
#include "util/progress.h"
#include "util/pi.h"

static stn_code_trans *create_stn_code_trans( const char *codeto, double datefrom, double dateto )
{
    stn_code_trans *sct=(stn_code_trans *) check_malloc(sizeof(stn_code_trans)+strlen(codeto)+1);
    char *sctcodeto = ((char *)(void *)sct)+sizeof(stn_code_trans);
    strcpy(sctcodeto,codeto);
    sct->codeto=codeto;
    sct->datefrom=datefrom;
    sct->dateto=dateto;
    sct->next=0;
    return sct;
}

static stn_code_trans_list *create_stn_code_trans_list( const char *codefrom )
{
    stn_code_trans_list *sctl=(stn_code_trans_list *) check_malloc(sizeof(stn_code_trans_list)+strlen(codefrom)+1);
    char *sctcodefrom = ((char *)(void *)sctl)+sizeof(stn_code_trans_list);
    strcpy(sctcodefrom,codefrom);
    sctl->codefrom=sctcodefrom;
    sctl->translations=0;
    sctl->next=0;
    return sctl;
}

static void add_stn_code_trans_to_list( stn_code_trans_list *list, stn_code_trans *trans )
{
    trans->next=list->translations;
    list->translations=trans;
}

static void delete_stn_code_trans_list( stn_code_trans_list *list )
{
    stn_code_trans *sct=list->translations;
    while( sct )
    {
        stn_code_trans *next=sct->next;
        check_free( sct );
        sct=next;
    }
    check_free( list );
}


stn_translation *create_station_translation( get_trans_station_func getstation, void *getstationdata )
{
    stn_translation *stt=(stn_translation *) check_malloc( sizeof(stn_translation));
    stt->stlists=0;
    stt->index=0;
    stt->getstationdata=getstationdata;
    stt->getstation=getstation;
    return stt;
}

void delete_station_translation( stn_translation *stt, network *net ) {
    stn_code_trans_list *lists=stt->stlists;
    while( lists )
    {
        stn_code_trans_list *next=lists->next;
        check_free(lists);
        lists=next;
    }
    if( stt->index ) check_free( stt->index );
    stt->index=0;
    stt->stlists=0;
    stt->getstationdata=0;
    stt->getstation=0;
    check_free( stt );
}

int add_station_translation( stn_translation *stt, const char *codefrom, const char *codeto, double datefrom, double dateto )
{
    stn_code_trans_list *stlist=stt->stlists;
    stn_code_trans *stc;
    while( stlist )
    {
        if( _stricmp(stlist->codefrom,codefrom) == 0 ) break;
        stlist=stlist->next;
    }
    if( ! stlist )
    {
        stlist=create_stn_code_trans_list( codefrom );
        stlist->next=stt->stlists;
        stt->stlists=stlist;
    }
    stc=create_stn_code_trans( codeto, datefrom, dateto );
    add_stn_code_trans_to_list( stlist, stc );
}


static int parse_stn_trans_def( stn_translation *stt, const char *def, char *errmsg, int nerrmsg )
{
    char word[50];
    char msg[80];

/*
 
   recode xxx to yyyy
   recode xxx to yyyy between date and date
   recode xxx to yyyy before date 
   recode xxx to yyyy after date 
   recode suffix xxx between/before/after for station_list
   
}

int read_station_translations( DATAFILE *d, stn_translation *stt )
{
}

const char *translate_code( stn_translation *stt, stn_translation *stt2, const char *code )
{
}

