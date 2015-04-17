#ifndef _RFTRNADJ_H
#define _RFTRNADJ_H

/*
   $Log: rftrnadj.h,v $
   Revision 1.1  1996/01/03 22:08:51  CHRIS
   Initial revision

*/

#ifndef RFTRNADJ_H_RCSID
#define RFTRNADJ_H_RCSID "$Id: rftrnadj.h,v 1.1 1996/01/03 22:08:51 CHRIS Exp $"
#endif

void set_use_refframe_topocentre( int use );

void init_rftrans_prms_list( void );

void update_rftrans_prms_list( int get_covariance );

void print_rftrans_list( FILE *out );

void vd_rftrans_corr_vector( int rf, double vd[3], double date,
                             double dst1[3][3], double dst2[3][3],
                             void *hA, int irow ) ;

void vd_rftrans_corr_point( int rf, double vd[3], double date,
                            double dst1[3][3], void *hA, int irow ) ;

#endif
