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

#include <forward_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "snapdata/loaddata.h"

/// A recode target starting with this character marks the station as rejected, e.g. "*ABCD"
constexpr char RECODE_IGNORE_CHAR = '*';

/// Recoding a station to this code ignores it; recoding from it applies to all other stations
constexpr std::string_view RECODE_IGNORE_CODE = "*";

struct stn_recode
{
    /// codeto is the target code without any reject marker
    stn_recode( std::string codeto, bool reject, double datefrom, double dateto, double herror, double verror, int seqid )
        : codeto( std::move(codeto) ), datefrom( datefrom ), dateto( dateto ), reject( reject ),
          seqid( seqid ), herror( herror ), verror( verror )
    {
    }

    const std::string codeto;
    const double datefrom;
    const double dateto;
    const bool reject;  ///< true if the station is to be rejected, false otherwise
    bool used = false;  ///< true once the recode has been applied to a station code
    const int seqid;
    double herror; /* if errors > 0.0 then used as obs */
    double verror;
};

/// The recodes for one station code, sorted into the order in which they are matched
using stn_recode_list = std::forward_list<stn_recode>;

/* get_station_func should return 
 * STN_RECODE_FAIL   cannot create station
 * STN_RECODE_EXISTS station already exists, no need to create
 * STN_RECODE_NEW    copy of old station created 
 */

#define STN_RECODE_FAIL   0
#define STN_RECODE_EXISTS 1
#define STN_RECODE_NEW    2

typedef int (*get_recode_station_func)( void *data, const char *codefrom, const char *codeto );

struct stn_recode_map
{
    explicit stn_recode_map( network *net ) : net( net ) {}

    std::map<std::string,stn_recode_list,station_code_order> lists;  ///< recodes by code being recoded
    stn_recode_list global;  ///< recodes from RECODE_IGNORE_CODE, applied to stations not otherwise recoded
    bool used = false;       ///< true once any recode has been applied to a station code
    network *net;
};

struct stn_recode_data
{
    stn_recode_map *global_map;
    stn_recode_map *file_map;
    network *net;
};


/* 
 * network is used for finding station in station list
 * getstation is used when stations are recoded to confirm they exist.  It
 * may also create new stations.  It returns a status according to which it 
 * does.  getstationdata is data used by the getstation function
 */

stn_recode_map *create_stn_recode_map( network *net );

void delete_stn_recode_map( stn_recode_map *stt );

bool recodes_used( stn_recode_map *stt );

void add_stn_recode_to_map( stn_recode_map *stt, 
        std::string_view codefrom, std::string_view codeto, double datefrom, double dateto );

int read_station_recode_definition( stn_recode_map *stt, std::string_view def, const std::string &basefile );

void print_stn_recode_list( FILE *out, stn_recode_map *stt, bool onlyused, int stn_name_width, std::string_view prefix );

/// Returns the recodes of code, or nullptr if it has none
const stn_recode_list *get_station_recodes( stn_recode_map *stt, std::string_view code );

std::optional<recode_result> get_stn_recode( stn_recode_map *stt, std::string_view code, double date );

std::optional<recode_result> recoded_network_station( void *recode_data, std::string_view code, double date );

#endif /* _STNRECODE_HPP */
