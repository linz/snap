#ifndef _CFGPROCS_H
#define _CFGPROCS_H

/*
   $Log: cfgprocs.h,v $
   Revision 1.1  1995/12/22 17:40:25  CHRIS
   Initial revision

*/

#include <string_view>

#include "util/readcfg.h"

int load_coordinate_file( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
int add_coordinate_file( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
int set_output_coordinate_file( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
int load_offset_file( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
int load_data_file( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
int read_classification_command( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
int read_obs_modification_command( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );
int read_recode_command( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );


extern int stations_read;

#endif

