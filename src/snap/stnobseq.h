#ifndef _STNOBSEQ_H
#define _STNOBSEQ_H

/*
   $Log: stnobseq.h,v $
   Revision 1.3  2003/11/23 23:05:19  ccrook
   Updated iteration output to display number of stations exceeding adjustment
   tolerance

   Revision 1.2  1998/05/21 04:01:59  ccrook
   Added support for deformation model to be applied in the adjustment.

   Revision 1.1  1996/01/03 22:12:42  CHRIS
   Initial revision

*/

#ifndef STNOBSEQ_H_RCSID
#define STNOBSEQ_H_RCSID "$Id: stnobseq.h,v 1.3 2003/11/23 23:05:19 ccrook Exp $"
#endif

#ifndef _STNADJ_H
#include "snap/stnadj.h"
#endif

#ifndef _BINFILE_H
#include "util/binfile.h"
#endif

void count_stn_obs( int type, int stn, char unused );
int init_station_rowno( void );
int find_station_row( int row, char *param, int plen );
void set_station_obseq( station *st, vector3 dst, void *hA, int irow, double date );
void init_rf_scale_error( double value, int adjust );
double rf_scale_error( double dist, void *hA, int irow );
void max_station_adjustment( double tol, int *pmaxadjstn,
                             double *pmaxadj, int *pnstnadj );
void update_station_coords( void );
void print_coordinate_changes( FILE *out );
void print_adjusted_coordinates( FILE *out );
void write_station_csv();
void sum_floating_stations( int iteration );
void calc_error_ellipse( double cvr[], double *emax, double *emin, double *azemax );
void get_station_covariance( station *st, double cvr[] );
void dump_station_covariances( BINARY_FILE *b );
void print_adjusted_coordinates( FILE *out );
void print_floated_stations( FILE *out );
void print_station_offsets( FILE *out );

#endif
