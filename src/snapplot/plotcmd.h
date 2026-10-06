#ifndef _PLOTCMD_H
#define _PLOTCMD_H

/*
   $Log: plotcmd.h,v $
   Revision 1.1  1996/01/03 22:24:25  CHRIS
   Initial revision

*/

#include <string>
#include <string_view>

int read_plot_command_file( const std::string &fname, int got_data );

/* Done before configuration file read */
void add_default_configuration_files( void );
/* Done before list processed */
void add_configuration_file( const std::string &fname );
/* Done after file loaded */
int process_configuration_file_list( void );

/* Done in interactive section of program */
int process_configuration_file( const std::string &fname );

void abort_snapplot_config_file( void );

/* List of configuration menu items */

int config_menu_item_count();
std::string_view config_menu_text( int i );
std::string_view config_menu_filename( int i );

/* Function to write configuration information to a file */

int save_configuration( const std::string &cfgname );
int write_config_file( FILE *out, int key_only );


#define SNAPPLOT_CONFIG_EXT ".spc"
#define SNAPPLOT_CONFIG_FILE "snapplot.spc"
#define SNAPPLOT_CONFIG_SECTION "snapplot"

#endif
