#ifndef _SNAPCSVOBS_H
#define _SNAPCSVOBS_H

#include "util/datafile.h"

#include <string>

int load_snap_csv_obs(const std::string &options, DATAFILE *df, int (*check_progress)(DATAFILE *df));

#endif
