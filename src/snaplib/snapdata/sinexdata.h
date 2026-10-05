#ifndef _SINEXDATA_H
#define _SINEXDATA_H

#include <string>

int load_sinex_obs( const std::string &options, DATAFILE &df, bool (*check_progress)( DATAFILE &df ) );

#endif
