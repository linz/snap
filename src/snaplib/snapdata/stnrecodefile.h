#ifndef _STNRECODEFILE_H
#define _STNRECODEFILE_H

#define DFLTSTRCD_EXT ".csv"

#include "snapdata/stnrecode.h"

int read_station_recode_file( stn_recode_map *stt, const char *filename, const char *basefile );

#endif
