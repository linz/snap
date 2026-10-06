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

/// Sets job_title from the "title" command, keeping at most JOBTITLELEN-1 characters.
/// Returns OK. The signature is fixed by config_store_func in readcfg.h.
int read_job_title_command(
    CFG_FILE *cfg,            ///< The configuration file being read, unused
    std::string_view string,  ///< The text of the title
    void *value,              ///< The storage address from the command table, unused
    int len,                  ///< The value length from the command table, unused
    int code                  ///< The command code from the command table, unused
);


extern int stations_read;

#endif

