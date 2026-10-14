#ifndef _STNRECODEFILE_H
#define _STNRECODEFILE_H

#define DFLTSTRCD_EXT ".csv"

#include <string_view>
#include <string>

struct stn_recode_map;

int read_station_recode_file(stn_recode_map* stt, std::string_view filename, std::string_view basefile);

#endif
