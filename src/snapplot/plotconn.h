#ifndef _PLOTCONN_H
#define _PLOTCONN_H

/*
   $Log: plotconn.h,v $
   Revision 1.2  1997/04/28 11:01:15  CHRIS
   Modified to add functions for reading and setting the sort order options.

   Revision 1.1  1996/01/03 22:25:22  CHRIS
   Initial revision

*/

#ifndef _DATATYPE_H
#include "snapdata/datatype.h"
#endif

#ifndef _SURVDATA_H
#include "snapdata/survdata.h"
#endif

#ifndef INFOWIN_H
#include "infowin.h"
#endif

#ifndef _PLOTFUNC_H
#include "plotfunc.h"
#endif

void set_binary_data( int status );
int have_binary_data( void );
void add_survdata_connections( survdata *sd, long bloc );
void add_relative_covariance( int from, int to, double cvr[6] );

#define DPEN_BY_TYPE -1
#define DPEN_BY_FILE -2
#define DPEN_BY_SRES -3
#define DPEN_BY_RFAC -4

void setup_data_pens( int pen_type );
void select_data_pen_type( void );
void setup_sres_pens( double max, int apost, int npens );
void get_sres_pen_options( double *max, int *apost, int *npens );
void setup_rfac_pens( int npens );
void get_rfac_pen_options( int *npens );

int set_datapen_definition( char *def );
void get_datapen_definition( char *def );  /* Assumes def is big enough */

/* Options for displaying highlights */

#define PCONN_HIGHLIGHT_NONE           0
#define PCONN_HIGHLIGHT_IF_EITHER      1
#define PCONN_HIGHLIGHT_IF_BOTH        2
#define PCONN_HIGHLIGHT_SRES           3
#define PCONN_HIGHLIGHT_APOST_SRES     4
#define PCONN_HIGHLIGHT_RFAC           5
#define PCONN_HIGHLIGHT_REJECTED       6
#define PCONN_HIGHLIGHT_UNUSED         7

/* Options for displaying more than one observation on a line */

#define PCONN_ONE_CONNECTION  0
#define PCONN_DIFFERENT_TYPES 1
#define PCONN_ALL_CONNECTIONS 2
#define PCONN_MERGE_FLAGS     3

#define PCONN_SHOW_ONEWAY_OBS 4

#define PCONN_REDRAW_HIGHLIGHT -1
/* For colouring obs with highlight colour */

void set_obs_highlight_option( int type, double value );
void get_obs_highlight_option( int *type, double *value );

/* For drawing a highlight box around an individual observation */

double get_obs_highlight_offset( void );


int plot_connections( map_plotter *plotter, int first, int offset_opt, double offset, int redraw );
void maximum_relative_covariance( double *h, double *v );
int nearest_connection( double e, double n, double tol, int *from, int *to );

long sres_index_count();
char *sres_list_header();
char *sres_item_description( long id );
void sres_item_info( long id, PutTextInfo *jmp );
void set_sres_display_option( int mode );
int get_sres_display_option();
void set_sres_sort_col( int col );
void set_sres_sort_option( int option );
int get_sres_sort_option();
void init_displayed_fields();
void set_displayed_fields( int *fields, int nFields );
int get_displayed_fields( int *fields, int maxFields );
int get_display_field_code( const char *name );
const char *get_display_field_name( int code );
int read_display_fields_definition( char *def );
void write_display_fields_definition( char *def, int nchar );

/* Choice of data to display in the data list window */

#define SRL_ALL      0
#define SRL_REJECTED 1
#define SRL_UNUSED   2
#define SRL_USED     3

/* Fields to display in the data list window */
/* Can display any of these and also classifications */

#define SRF_FROM   -1
#define SRF_TO     -2
#define SRF_TYPE   -3
#define SRF_STATUS -4
#define SRF_SRES   -5
#define SRF_RFAC   -6
#define SRF_LENGTH -7
#define SRF_FILE   -8
#define SRF_LINENO -9
#define SRF_OBSID  -10
#define SRF_DATE   -11

int get_connection_count( int istn );
int connection_observation_count(  int from, int index );
int get_connected_station( int from, int index, char *visible );

void list_connections( void *dest, PutTextFunc f, int istn );
void list_observations( void *dest, PutTextFunc f, int from, int to );
void list_single_observation( void *dest, PutTextFunc f, int from, int to, int obs_id );

void free_connection_resources();

#endif
