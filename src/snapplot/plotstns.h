#ifndef _PLOTSTNS_H
#define _PLOTSTNS_H

/*
   $Log: plotstns.h,v $
   Revision 1.2  1996/07/12 20:33:35  CHRIS
   Modified to support hidden stations.

   Revision 1.1  1996/01/03 22:29:25  CHRIS
   Initial revision

*/

#ifndef PLOTSTNS_H_RCSID
#define PLOTSTNS_H_RCSID "$Id: plotstns.h,v 1.2 1996/07/12 20:33:35 CHRIS Exp $"
#endif

#ifndef COORDSYS_H
#include "coordsys/coordsys.h"
#endif

#ifndef INFOWIN_H
#include "infowin.h"
#endif

#ifndef _PLOTFUNC_H
#include "plotfunc.h"
#endif

/* Useful numbers */

#define PSFLG_HIGHLIGHT 0x01
#define PSFLG_VISIBLE   0x02
#define PSFLG_OFFSET    0x04
#define PSFLG_HIDDEN    0x08

/* Sort list fields */

enum
{
    STNF_CODE,
    STNF_NAME,
    STNF_STS,
    STNF_LAT,
    STNF_LON,
    STNF_EAST,
    STNF_NRTH,
    STNF_HGT,
    STNF_HERR,
    STNF_HADJ,
    STNF_VERR,
    STNF_VADJ,
    STNF_CLASS,
    MAX_STNLIST_FIELDS
};

void set_plot_projection( coordsys *cs );
coordsys *plot_projection( void );
char geodetic_coordsys( void );
void init_plotstns( int adjusted );   /* Called after stations have been read */
void format_plot_coords( double e, double n, char *buf );  /* Assumes buf is big enough!? */
char *plot_crdsys_name();
int projection_defined( void );
#ifdef _BINFILE_H
int reload_covariances( BINARY_FILE *b );
#endif
int got_covariances( void );
void max_covariance_component( double *h, double *v );
void max_adjustment_component( double *h, double *v);
void get_error_ellipse( int istn, double *emax, double *emin, double *brng );
void get_height_error( int istn, double *hgterr );

void get_station_range( double *emn, double *nmn, double *emx, double *nmx );
int station_count( void );
int used_station_count( void );
int sorted_station_number( int i );
void init_station_list();
void  sort_station_list_col( int icol );
void  sort_station_list( int opt );
char *station_list_header( void );
char *station_list_item( int i );
void station_item_info( int i, PutTextInfo *jmp );
void list_station_summary( void *dest, PutTextFunc f );
void list_station_details( void *dest, PutTextFunc f, int istn );

/* Stations sorted in order of easting.  Total is used_station_count().
   sorted_x_station_number converts x station index to base index into
   network.
   first_station_past_x is the number in the sorted order of the first
   station with easting greater than specified value */

int sorted_x_station_number( int i );
int first_station_past_x( double value );

void setup_station_pens( int class_id );
void get_stationpen_definition( char *def );  /* Assumes def is big enough */
void init_plotting_stations( void );
int station_in_view( int istn );
int station_showable( int istn );
void flag_station_visible( int istn );

void highlight_station( int istn );
void unhighlight_station( int istn );
void clear_all_highlights( void );
int station_highlighted( int istn ); /* Returns 1 if station is highlighted */
int highlighted_stations( void );      /* Returns number highlighted */

void hide_station( int istn );
void unhide_station( int istn );
int station_hidden( int istn );           /* Returns 1 if the station is not to be plotted */

int station_in_view( int istn );
void get_station_coordinates( int istn, double *e, double *n );
void get_projection_coordinates( int istn, double *e, double *n );
void get_station_adjustment( int istn, double dxyz[3] );
void use_station_offsets( int mode );
int using_station_offsets( void );
void set_station_offset( int istn, double e_offset, double n_offset );
int get_station_offset( int istn, double *e_offset, double *n_offset );
int offset_station_count( void );
int nearest_station( double e, double n, double tol );
long get_xyindex_version( void );

void plot_station_symbol( map_plotter *plotter, double px, double py, int symbol, int pen );
int plot_stations( map_plotter *plotter, int first, int highlightonly );
int plot_covariances( map_plotter *plotter, int first );
int plot_height_errors( map_plotter *plotter, int first );
int plot_station_names( map_plotter *plotter, int first );
int plot_adjustments( map_plotter *plotter, int first );

void calc_error_ellipse( double cvr[], double *emax, double *emin, double *azemax );

void free_station_resources();

#endif
