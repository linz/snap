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
    int usemark;  /* 0 if station is to be rejected, 1 otherwise */
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
 * STN_RECODE_FAIL   cannot create station
 * STN_RECODE_EXISTS station already exists, no need to create
 * STN_RECODE_NEW    copy of old station created 
 */

#define STN_RECODE_FAIL   0
#define STN_RECODE_EXISTS 1
#define STN_RECODE_NEW    2

typedef int (*get_recode_station_func)( void *data, const char *codefrom, const char *codeto );

typedef struct 
{
    stn_recode_list *stlists;
    stn_recode_list **index;
    stn_recode_list *global;
    int nindex;
    int used;
    network *net;
} stn_recode_map;

typedef struct
{
    stn_recode_map *global_map;
    stn_recode_map *file_map;
    network *net;
} stn_recode_data;


/* 
 * network is used for finding station in station list
 * getstation is used when stations are recoded to confirm they exist.  It
 * may also create new stations.  It returns a status according to which it 
 * does.  getstationdata is data used by the getstation function
 */

stn_recode_map *create_stn_recode_map( network *net );

void delete_stn_recode_map( stn_recode_map *stt );

int recodes_used( stn_recode_map *stt );

void add_stn_recode_to_map( stn_recode_map *stt, 
        const char *codefrom, const char *codeto, double datefrom, double dateto );

int read_station_recode_definition( stn_recode_map *stt, char *def, char *basefile );

void print_stn_recode_list( FILE *out, stn_recode_map *stt, int onlyused, int stn_name_width, const char *prefix );

const char *get_stn_recode( stn_recode_map *stt, const char *code, double date, int *reject );

const char *recoded_network_station( void *recode_data, const char *code, double date );

#endif /* _STNRECODE_HPP */
