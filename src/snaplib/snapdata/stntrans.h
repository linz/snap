#ifndef _STNTRANS_HPP
#define _STNTRANS_HPP

/*
   $Log: snapdata.h,v $
   Revision 1.1  1995/12/22 18:48:39  CHRIS
   Initial revision

*/

#ifndef _DATAFILE_H
#include "util/datafile.h"
#endif

typedef struct stn_code_trans_s
{
    const char *codeto;
    double datefrom;
    double dateto;
    int station_created;
    struct stn_code_trans_s *next;
} stn_code_trans;

typedef struct stn_code_trans_list_s
{
    const char *codefrom;
    stn_code_trans *translations;
    struct stn_code_trans_list_s *next;
} stn_code_trans_list;

/* get_station_func should return 
 * 0 - cannot create station
 * 1 - station already exists, no need to create
 * 2 - copy of old station created 
 */

typedef int (*get_trans_station_func)( void *data, const char *codefrom, const char *codeto );

typedef struct stn_translation
{
    stn_code_trans_list *stlists;
    stn_code_trans_list *index;
    void *getstationdata;
    get_trans_station_func getstation;
} stn_translation;


stn_translation *create_station_translation( get_trans_station_func getstation, void *getstationdata );

void delete_station_translation( stn_translation *stt );

int add_station_translation( stn_translation *stt, const char *codefrom, const char *codeto, double datefrom, double dateto );

int read_station_translations( DATAFILE *d, stn_translation *stt );

const char *translate_code( stn_translation *stt, stn_translation *stt2, const char *code );

#endif /* defined _STNTRANS_HPP */

