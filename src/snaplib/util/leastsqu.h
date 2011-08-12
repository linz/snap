#ifndef _LEASTSQU_H
#define _LEASTSQU_H

/*
   $Log: leastsqu.h,v $
   Revision 1.3  1998/06/15 02:26:46  ccrook
   Modified to handle long integer number of observations

   Revision 1.2  1996/01/10 19:49:34  CHRIS
   Added function lsq_get_covariance_row

   Revision 1.1  1996/01/03 21:59:27  CHRIS
   Initial revision

*/

#ifndef LEASTSQU_H_RCSID
#define LEASTSQU_H_RCSID "$Id: leastsqu.h,v 1.3 1998/06/15 02:26:46 ccrook Exp $"
#endif

#ifndef _BLTMATRX_H
#include "bltmatrx.h"
#endif

#ifndef _SYMMATRX_H
#include "util/symmatrx.h"
#endif

#ifndef _LSOBSEQ_H
#include "lsobseq.h"
#endif

enum { LSQ_UNINIT, LSQ_READY, LSQ_SUMMING, LSQ_SOLVED, LSQ_INVERTED };

/* Header file for leastsqu.c - basic least squares routines for sparse
   observation equations */

/* Initiallization */

void lsq_alloc( int nprm );
void lsq_init( void );

/* Sum the observation equations into the normal equations */
/* Returns a non-zero value if an error occurs summing     */
/* the Schreiber equation component.                       */

int lsq_sum_obseqn( void *A );
void lsq_zero_b_vector( void );

/* Solve the normal equations.  Returns a non zero error   */
/* code if fails to invert the normal matrix.  The error   */
/* code is the number of the row in which the inversion    */
/* failed                                                  */
/* If the force_solution parameter is non zero the routine */
/* applies constraints to force a solution, and returns    */
/* the number of constraints applied.                      */

int lsq_solve_equations( int force_solution );
void lsq_get_stats( long *nobs, int *nprm, long *nschp, long *dof,
                    double *ssr, double *seu );

/* Get covariance row returns one row of the full covar     */
/* matrix.  It must be called immediately before or after   */
/* lsq_solve_equations.  It returns a pointer to an array   */
/* of doubles which is valid until another lsq_ routine is  */
/* called.                                                  */

double *lsq_get_covariance_row( int row );

/* Get the choleski decomposition of the normal matrix     */
/* It must be called immediately before or after           */
/* lsq_solve_equations, and may be invalidated by          */
/* subsequent lsq_... calls                                */

bltmatrix *lsq_get_decomposition( void );

/* Get the covariance matrix                               */

bltmatrix *lsq_get_covariance_matrix( void );

/* Return information from the adjustment - parameters and */
/* their covariance for a range of parameters.  Note that  */
/* the value and covariance are only returned if they are  */
/* not set to NULL                                         */

void lsq_invert_normal_equations( void );

void lsq_get_params( int row[], int nrow, double *value, ltmat covar );

void lsq_calc_obs( void *hA, double *calc, double *res,
                   double *schprm, double *schvar,
                   char diagonal, ltmat calccvr, ltmat rescvr );

/* Return a pointer to the normal equations for managing the */
/* bandwidth and other uses                                  */

bltmatrix *lsq_normal_matrix( void );

#endif
