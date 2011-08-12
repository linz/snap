#ifndef _SNAPCSVOBS_H
#define _SNAPCSVOBS_H

int load_snap_csv_obs( const char *options, DATAFILE *df, int (*check_progress)( DATAFILE *df ) );

#endif
