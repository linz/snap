#ifndef _STNRECODE_HPP
#define _STNRECODE_HPP

/*
   $Log: snapdata.h,v $
   Revision 1.1  1995/12/22 18:48:39  CHRIS
   Initial revision

*/

#ifndef _DATAFILE_H
#include "util/datafile.h"
#endif

#ifndef _NETWORK_H
#include "network/network.h"
#endif

#define RECODE_UNUSED      0
#define RECODE_USED        1
#define RECODE_STN_CREATED 2

typedef struct stn_recode_s
{
    const char *codeto;
    double datefrom;
    double dateto;
    int used;
    struct stn_recode_s *next;
} stn_recode;

typedef struct stn_recode_list_s
{
    const char *codefrom;
    stn_recode *translations;
    struct stn_recode_list_s *next;
} stn_recode_list;

/* get_station_func should return 
 * 0 - cannot create station
 * 1 - station already exists, no need to create
 * 2 - copy of old station created 
 */

typedef int (*get_recode_station_func)( void *data, const char *codefrom, const char *codeto );

typedef struct 
{
    stn_recode_list *stlists;
    stn_recode_list **index;
    int nindex;
    network *net;
    get_recode_station_func getstation;
    void *getstationdata;
} stn_recode_map;


/* 
 * network is used for finding station in station list
 * getstation is used when stations are recoded to confirm they exist.  It
 * may also create new stations.  It returns a status according to which it 
 * does.  getstationdata is data used by the getstation function
 */

stn_recode_map *create_stn_recode_map( network *net, get_recode_station_func getstation, void *getstationdata );

void delete_stn_recode_map( stn_recode_map *stt );

int add_station_recode( stn_recode_map *stt, const char *codefrom, const char *codeto, double datefrom, double dateto );

int read_station_recode_definition( stn_recode_map *stt, char *def, char *basefile );

void print_stn_recode_list( FILE *out, stn_recode_map *stt, int onlyused, int stn_name_width, const char *prefix );

const char *get_stn_recode( stn_recode_map *stt, stn_recode_map *sttbase, const char *code, double date );

#endif /* defined _STNRECODE_HPP */

