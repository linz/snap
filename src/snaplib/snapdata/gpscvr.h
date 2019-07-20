#ifndef _GPSCVR_H
#define _GPSCVR_H

/*
   $Log: gpscvr.h,v $
   Revision 1.1  1995/12/22 18:46:18  CHRIS
   Initial revision

*/

#ifndef _SYMMATRX_H
#include "util/symmatrx.h"
#endif

#ifndef _SURVDATA_H
#include "snapdata/survdata.h"
#endif

typedef void (*VRTFUNC)( int id, double vrt[3] );
typedef double * (*XFMFUNC)( int reffrm, int inverse );

void init_gps_covariances( VRTFUNC vrtfunc, XFMFUNC xfmfunc );
VRTFUNC gps_vrtfunc( void );
XFMFUNC gps_xfmfunc( void );
void transform_vec_to_topo( int from, int to, int rf_id,
                            double *vec, double *cvr );
void set_gps_vertical_fixed( int fixed );
int gps_vertical_fixed( void );
double vector_standardised_residual( double vec[3], ltmat cvr, int *rank );

/* void transform_xyz_cvr_to_topocentric( survdata *vd, ltmat cvr ); */

/* Flags for calc_vecdata_vector function.  type must be one of specified.
   Options are to convert to topocentric, and to calculate standard errors
   rather than a covariance matrix */


#define VD_OBSVEC       0x01
#define VD_CALCVEC      0x02
#define VD_RESVEC       0x03
#define VD_TYPEMASK     0x03


#define VD_TOPOCENTRIC  0x04
#define VD_STDERR       0x08

/* Specify that the from station is the reference station.  The to station
   and the from station (if not REF_STN) are the index of the target station,
   from 0 to nobs-1 */

#define VD_REF_STN -1

int calc_vecdata_vector( survdata *vd, int from, int to, int type,
                         double *vec, double *cvr );

int calc_vecdata_point( survdata *vd, int to, int type,
                        double *vec, double *cvr );


void gps_covar_apply_obs_error_factor( survdata *sd, int iobs, double factor );
void gps_covar_apply_obs_offset_error( survdata *sd, int iobs, double varhor, double varvrt );
void gps_covar_apply_basestation_offset_error( survdata *vd, double varhor, double varvrt );
void gps_covar_apply_centroid_error( survdata *sd, double varhor, double varvrt );
void gps_covar_apply_set_error_factor( survdata *sd, double factor );

#endif

