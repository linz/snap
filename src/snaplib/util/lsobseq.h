#ifndef _LSOBSEQ_H
#define _LSOBSEQ_H

/*
   $Log: lsobseq.h,v $
   Revision 1.1  1996/01/03 22:00:39  CHRIS
   Initial revision

*/

#ifndef LSOBSEQ_H_RCSID
#define LSOBSEQ_H_RCSID "$Id: lsobseq.h,v 1.1 1996/01/03 22:00:39 CHRIS Exp $"
#endif

#ifndef _SYMMATRX_H
#include "util/symmatrx.h"
#endif

/*------------------------------------------------------------*/
/*  Structures used in storing and processing the obs. equns. */
/*  obsrow    One row of A                                    */
/*                                                            */
/*  Data types to hold the observation equations              */
/*                                                            */
/*  The observation equations are held in compressed storage  */
/*  such that for each row only the parameters with non-zero  */
/*  coefficients are included.  Parameters are numbered from  */
/*  0 to prm-1, with the observation residuals being included */
/*  as parameter prm.  No requirement is made for the         */
/*  parameters to be stored in ascending order.               */
/*                                                            */
/*  The parameters are divided into two groups, those which   */
/*  are solved for explicitely in the adjustment, and those   */
/*  which are eliminated before the normal equations are      */
/*  summed using Schreibers method.  The latter must be       */
/*  confined to a group of observation equations, and must    */
/*  be stored in a fixed order for that group of equations.   */
/*                                                            */
/*  Two structures are defined:                               */
/*                                                            */
/*  obsrow     contains a single row of the observation       */
/*             equations.                                     */
/*                                                            */
/*  obseqn     contains a group of rows of the observation    */
/*             equations which are related by statistical     */
/*             correlation, or by a common implicit           */
/*             parameter.  The weight matrix for this group   */
/*             of observations can be held in lower           */
/*             triangular or diagonal storage.                */
/*                                                            */
/*------------------------------------------------------------*/

typedef struct
{
    int    *col;    /* Column no of non-zero element */
    double *val;    /* Value of non-zero element */
    int     ncol;   /* Number of non-zero elts */
    double  obsv;   /* The value of the observation */
    double  schv;   /* Implicitely solved parameter */
    char    flag;   /* Flag for observations */
}  obsrow;

#define OE_UNUSED  1

typedef struct
{
    int     nprm;   /* Number of parameters */
    int     nrow;   /* Number of rows in equns */
    int     maxrow; /* Maximum number of rows allocated */
    int     maxcol; /* Maximum non-zero parameters */
    obsrow **obs;   /* Array of pointers to rows of A */
    ltmat   wgt;    /* Weight matrix, lt or diagonal storage */
    ltmat   cvr;    /* Covariance matrix */
    char    flag;   /* TRUE if weight matrix is diagonal */
    long    maxelt; /* Size currently allocated to the matrix*/
}  obseqn;

#define OE_LOWERTRI_CVR 0    /* Covariance is lower triangle storage */
#define OE_DIAGONAL_CVR 1    /* Covariance matrix is diagonal */
#define OE_SCHREIBER    2    /* Observation equations include Schreiber param */

void *create_oe( int nprm );
void delete_oe( void *A );
void init_oe( void *A, int nrow, int ncol, char options );


/* Routines for initiallizing observation equations - mainly defined as
   macros for efficiency */

#define oe_value( A, irow, v ) \
		  ((obseqn *)A)->obs[irow-1]->obsv = v

#define oe_add_value( A, irow, v ) \
		  ((obseqn *)A)->obs[irow-1]->obsv += v



#define oe_param( A, irow, icol, v ) \
		  { obsrow *obs;                 \
		    obs = ((obseqn *)A)->obs[(irow)-1];          \
		    obs->col[obs->ncol] = (icol)-1;  \
		    obs->val[obs->ncol] = v;     \
		    obs->ncol++;                 \
		    }

#define oe_schreiber( A, irow, v ) \
		  { ((obseqn *)A)->obs[irow-1]->schv = v; \
		    ((obseqn *)A)->flag |= OE_SCHREIBER;  \
		    }


#define oe_flag_unused( A, irow ) \
		  ((obseqn *)A)->obs[irow-1]->flag |= OE_UNUSED

#define oe_covar_ptr( A ) \
		  (((obseqn *)A)->cvr)


void oe_covar( void *A, int irow, int jrow, double v );

void oe_add_param( obseqn *A, int irow, int c, double v );

void print_obseqn( FILE *out, void *hA );

void print_obseqn_json( FILE *out, void *hA, const char *source );

#define obseqn_rows(hA) (((obseqn *)hA)->nrow)
#define obseqn_cvr_diagonal(hA) (((obseqn *)hA)->flag & OE_DIAGONAL_CVR)


#endif


