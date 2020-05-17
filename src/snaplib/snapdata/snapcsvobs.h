#ifndef _SNAPCSVOBS_H
#define _SNAPCSVOBS_H

#include "util/datafile.h"

int load_snap_csv_obs(const char *options, DATAFILE *df, int (*check_progress)(DATAFILE *df));

#endif
