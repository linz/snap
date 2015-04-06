
/*
   $Log: bltmatrx.h,v $
   Revision 1.4  2004/04/22 02:35:43  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.3  2004/02/12 02:57:06  ccrook
   Modified order of calculation in blt_chol_inv in order to be more efficient when
   paging virtual memory...

   Revision 1.2  2003/11/28 01:56:47  ccrook
   Updated to defer formulation of full normal equation matrix to calculation
   of inverse when it is required, rather than bypassing station reordering.

   Revision 1.1  1996/01/03 21:57:24  CHRIS
   Initial revision

*/

#ifndef BLTMATRX_H_RCSID
#define BLTMATRX_H_RCSID "$Id: bltmatrx.h,v 1.4 2004/04/22 02:35:43 ccrook Exp $"
#endif
/* Definitions of banded lower triangular matrices */

#ifndef _BLTMATRIX_H
#define _BLTMATRIX_H

#ifndef _SYMMATRX_H
#include "util/symmatrx.h"   /* For definition of ltmat */
#endif


typedef struct
{
    double *address;  /* Address of first element of row */
    int col;          /* First column for which elements are held */
    int req;          /* First column for which elements are required */
    char alloc;       /* Address marks the beginning of an allocated block */
} bltrow;

typedef struct
{
    char status;      /* Status of allocation of matrix */
    int nrow;         /* Number of rows/columns */
    int nsparse;      /* The number of sparse rows, used for allocation */
    long nelement;    /* The total number of elements in the matrix */
    bltrow *row;      /* Pointer to the array of rows */
} bltmatrix;

#ifdef CHECKBLT
#include "util/errdef.h"
#define BLT(b,i,j) \
   (*( ((i) > (j)) ? b->row[(i)].address + \
			 ((j) < b->row[i].col ? handle_error(FATAL_ERROR,"Bandwidth error",NO_MESSAGE) \
					      : (j) - b->row[(i)].col \
					      ) \
		     : b->row[(j)].address + \
			 ((i) < b->row[j].col ? handle_error(FATAL_ERROR,"Bandwidth error",NO_MESSAGE) \
					      : (i) - b->row[j].col \
					      ) \
		     ))

#else
#define BLT(b,i,j) \
   (*( ((i) > (j)) ? b->row[(i)].address + (j) - b->row[(i)].col \
		     : b->row[(j)].address + (i) - b->row[(j)].col \
		     ))
#endif

bltmatrix *create_bltmatrix( int nrow );
void delete_bltmatrix( bltmatrix *blt );
int copy_bltmatrix( bltmatrix *bltsrc, bltmatrix *bltcopy );
void blt_nonzero_element( bltmatrix *blt, int row, int col );
int blt_is_nonzero_element( bltmatrix *blt, int row, int col );
void blt_set_sparse_rows( bltmatrix *blt, int nsparse );
void init_bltmatrix( bltmatrix *blt );
void expand_bltmatrix_to_full( bltmatrix *blt );
void expand_bltmatrix_to_requested( bltmatrix *blt );
int blt_chol_dec( bltmatrix *blt, int fill );
void blt_chol_slv( bltmatrix *blt, double *b, double *r );
void blt_chol_inv( bltmatrix *blt );
void print_bltmatrix( FILE *out, bltmatrix *blt, char *format, int indent );
double blt_get_small( int absolute );
void blt_set_small( int absolute, double value );
double *blt_get_row_data( bltmatrix * blt, int irow );
/* Only returns rows fully populated to diagonal */
void dump_bltmatrix( bltmatrix *blt, FILE *b );
int reload_bltmatrix( bltmatrix **pblt, FILE *b );
void print_bltmatrix_json( bltmatrix *blt, FILE *out, const char *prefix );

#endif

