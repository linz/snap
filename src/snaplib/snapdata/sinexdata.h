#ifndef _SINEXDATA_H
#define _SINEXDATA_H

int load_sinex_obs( const char *options, DATAFILE *df, int (*check_progress)( DATAFILE *df ) );

#endif
