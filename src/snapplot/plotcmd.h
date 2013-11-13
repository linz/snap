#ifndef _PLOTCMD_H
#define _PLOTCMD_H

/*
   $Log: plotcmd.h,v $
   Revision 1.1  1996/01/03 22:24:25  CHRIS
   Initial revision

*/

#ifndef PLOTCMD_H_RCSID
#define PLOTCMD_H_RCSID "$Id: plotcmd.h,v 1.1 1996/01/03 22:24:25 CHRIS Exp $"
#endif

int read_plot_command_file( char *command_file, int got_data );

/* Done before configuration file read */
void add_default_configuration_files( void );
/* Done before list processed */
int add_configuration_file( const char *fname );
/* Done after file loaded */
int process_configuration_file_list( void );

/* Done in interactive section of program */
int process_configuration_file( char *fname );

void abort_snapplot_config_file( void );

/* List of configuration menu items */

int config_menu_item_count();
char *config_menu_text( int i );
char *config_menu_filename( int i );

/* Function to write configuration information to a file */

int save_configuration( char *cfgname );
int write_config_file( FILE *out, int key_only );


#define SNAPPLOT_CONFIG_EXT ".spc"
#define SNAPPLOT_CONFIG_FILE "snapplot.spc"
#define SNAPPLOT_CONFIG_SECTION "snapplot"

#endif
