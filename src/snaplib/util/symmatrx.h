
/*
   $Log: symmatrx.h,v $
   Revision 1.2  2004/04/22 02:35:27  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:57:05  CHRIS
   Initial revision

*/

#ifndef _SYMMATRX_H
#define _SYMMATRX_H

typedef double *ltmat;


/*------------------------------------------------------------*/
/*  Inline code defining the (i,j) element of matrix N        */
/*  stored in lower triangular format                         */
/*                                                            */
/*  Lij                                                       */
/*------------------------------------------------------------*/

#define Lij(N,i,j)  N[((i)>(j)) ?((long)(i)*(i+1))/2+j :((long)(j)*(j+1))/2+i]

/* Routines defined in symmatrx.c */

int chol_dec( ltmat N, int np );
void chol_slv( ltmat N, ltmat b, ltmat r, int np );
void chol_inv( ltmat N, ltmat tmp, int np );

/* Routines which fill zero diagonal elements to force a solution */
/* Require a working vector of np*2 elements which must be        */
/* preserved between calls.                                       */

int pvt_chol_dec( ltmat N, int *col, int np );
void pvt_chol_slv( ltmat N, ltmat b, ltmat r, int *col, int np );
void pvt_chol_inv( ltmat N, ltmat tmp, int *col, int np );

void print_ltmat( FILE *out, ltmat N, int nrow, const char *format, int indent );
void print_ltmat_json( FILE *out, ltmat N, int nrow, const char *format, int indent );

#endif
