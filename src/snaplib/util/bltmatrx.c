#include "snapconfig.h"

/*  This module processes banded format lower trianglar matrices for
    storage of symmetric matrices in least squares problems.  The routines
    provide for initiallization, allocation, Cholesky decomposition, and
    inversion of the matrix.  The inversion is not complete - only the
    banded components are inverted.

    The format of the banded matrix is...


       x
       x x
       0 x x
       0 0 0 x
       0 0 0 x x
       0 0 x x x x
       0 0 0 0 0 x x
       x x x x x x x x
       x x x x x x x x x

    The zero blocks are not stored.  This scheme is managed by a structure
    bltmatrix.  It consists of an array of doubles holding the non zero
    components row by row.  For each row two integers are held, a int
    integer identifies the column number of the first non-zero row for
    the array, and a long integer is a pointer into the array of doubles
    to identify the beginning of the row.

    The matrix is initiallized by creating the structure without the
    array of doubles, but with a specified number of row.  Also the number
    of the first non-sparse row is provided.  Then non-zero rows and columns
    are registered to determine their extent of the banding.  When all
    requirements have been registered, the double array and the pointers
    are initiallized.

    The initiallized matrix may then be used for summing and solving
    equations, inverting, etc.

    Note: all references to rows and columns in the matrix are 0
    based.  It is up to the calling program to ensure that calls are made
    in the correct order and that invalid rows and columns are not passed.

*/


/*
   $Log: bltmatrx.c,v $
   Revision 1.5  2004/04/22 02:35:43  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.4  2004/02/25 02:46:26  ccrook
   Simplified logic slightly, calculation of leading diagonal elements in FPU

   Revision 1.3  2004/02/12 02:57:06  ccrook
   Modified order of calculation in blt_chol_inv in order to be more efficient when
   paging virtual memory...

   Revision 1.2  2003/11/28 01:56:46  ccrook
   Updated to defer formulation of full normal equation matrix to calculation
   of inverse when it is required, rather than bypassing station reordering.

   Revision 1.1  1996/01/03 21:56:38  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>

#include "util/bltmatrx.h"
#include "util/chkalloc.h"
#include "util/progress.h"
#include "util/errdef.h"


enum { BLT_UNINIT, BLT_ROWS, BLT_READY };

#ifndef BLT_INV_CACHE_SIZE
#define BLT_INV_CACHE_SIZE 30
#endif

static int minrowsize = 1000;
static double small = 1.0e-10;
static double abssmall = 1.0e-30;

double blt_get_small( int absolute )
{
    return absolute ? abssmall : small;
}

void blt_set_small( int absolute, double value )
{
    if( absolute )
    {
        abssmall = value;
    }
    else
    {
        small = value;
    }
}

static bltrow *alloc_bltrow( int nrow )
{
    int i;
    bltrow *row = (bltrow *) check_malloc( nrow * sizeof(bltrow) );
    for( i=0; i<nrow; i++ )
    {
        row[i].col = i;
        row[i].req = i;
        row[i].alloc = 0;
        row[i].address = NULL;
    }
    return row;
}

static void delete_bltrow( bltrow *row, int nrow )
{
    int i;
    for( i=0; i < nrow; i++ )
    {
        bltrow *r = &(row[i]);
        if( r->alloc && r->address )
        {
            check_free( r->address );
        }
    }
    check_free( row );
}

static void zero_bltmatrix( bltmatrix *blt )
{
    int i, c;
    double *m;

    /* Clear out the matrix ready for summation or whatever */

    for( i = 0; i<blt->nrow; i++ )
    {
        m = blt->row[i].address;
        for( c=blt->row[i].col; c <= i; c++ ) *m++ = 0.0;
    }
}

bltmatrix *create_bltmatrix( int nrow )
{
    bltmatrix *blt;
    blt = (bltmatrix *) check_malloc( sizeof(bltmatrix) );
    blt->nrow = nrow;
    blt->nsparse = 0;
    blt->status = BLT_UNINIT;
    blt->row = alloc_bltrow( nrow );
    blt->_invncol = 0;
    blt->_pfsumcol = 0;
    return blt;
}


void delete_bltmatrix( bltmatrix *blt )
{
    if( blt->row )
    {
        delete_bltrow( blt->row, blt->nrow );
        blt->row = NULL;
    }
    check_free( blt );
}

static void alloc_bltrow_arrays( bltmatrix *blt )
{
    int i;
    long nelement;
    double *lastalloc;

    /* Allocate the memory row by row - to make this efficient the
       memory is allocated in blocks of at least the minimum block size.
       */

    nelement = 0;
    lastalloc=NULL;
    for( i=0; i<blt->nrow; )
    {
        int i0 = i;
        int nalloc = 0;
        double *address;
        int rowsize;
        int alloc;
        while( i < blt->nrow )
        {
            bltrow *r = &(blt->row[i]);
            if( r->col < r->req ) r->req = r->col;
            if( i >= blt->nsparse ) r->req = 0;
            rowsize = i + 1 - r->req;
            i++;
            nalloc += rowsize;
            if( nalloc > minrowsize ) break;
        }
        address = (double *) check_malloc( nalloc * sizeof(double));
        alloc = 1;
        while( i0 < i )
        {
            int rw, r0, r1, r2;
            double *a1;
            bltrow *r = &(blt->row[i0]);
            if( r->alloc && r->address )
            {
                if( lastalloc ) check_free(lastalloc);
                lastalloc=r->address;
            }
            r0 = r->req;
            r2 = i0+1;
            r1 = r->address ? r->col : r2;
            a1 = r->address;

            r->address = address;
            r->alloc = alloc;
            r->col = r->req;

            for( rw = r0; rw < r1; rw++ ) {
                *(address++) = 0.0;
            }
            for( rw = r1; rw < r2; rw++ ) {
                *(address++) = *(a1++);
            }
            alloc = 0;
            i0++;
        }
        nelement += nalloc;
    }
    blt->nelement = nelement;
    if( lastalloc ) check_free(lastalloc);

    blt->status = BLT_READY;
}

int blt_nrows( bltmatrix *blt )
{
    return blt->nrow;
}

long blt_requested_size( bltmatrix *blt )
{
    int i;
    long nelement;
    nelement = 0;
    for( i=0; i<blt->nrow; i++ )
    {
        bltrow *r = &(blt->row[i]);
        if( r->col < r->req ) r->req = r->col;
        if( i >= blt->nsparse ) r->req = 0;
        nelement += i + 1 - r->req;
    }
    return nelement;
}

void blt_nonzero_element( bltmatrix *blt, int row, int col )
{
    int i;
    if( row < col )
    {
        i = row;
        row = col;
        col = i;
    }
    if( col < blt->row[row].req) blt->row[row].req = col;
    if( blt->status != BLT_READY) blt->status = BLT_ROWS;
}

int blt_is_nonzero_element( bltmatrix *blt, int row, int col )
{
    return blt->status == BLT_READY &&
           ((row < col) ?
            blt->row[col].col <= row   :
            blt->row[row].col <= col);
}

void blt_set_sparse_rows( bltmatrix *blt, int nsparse )
{
    blt->nsparse = nsparse;
}


void init_bltmatrix( bltmatrix *blt )
{
    int i;

    /* If no rows have been registered then assume a full matrix */

    if( blt->status == BLT_UNINIT )
    {
        for( i=0; i<blt->nrow; i++ ) blt->row[i].col = 0;
        blt->status = BLT_ROWS;
    }

    /* If the array hasn't been allocated yet then fill up the
       non-sparse rows and allocate it  */

    if( blt->status == BLT_ROWS )
    {

        alloc_bltrow_arrays( blt );
    }
    else
    {
        zero_bltmatrix( blt );
    }
}

/* Expand a BLT matrix to full rows filling missing elements with zero */

void expand_bltmatrix_to_full( bltmatrix *blt )
{
    if( ! blt ) return;

    blt_set_sparse_rows( blt, 0 );

    expand_bltmatrix_to_requested( blt );
}

/* Expand a bltmatrix according to requested (requested after initial allocation) */

void expand_bltmatrix_to_requested( bltmatrix *blt )
{
    int i;
    int expand;

    if( ! blt  ) return;

    /* If not initiallized, then there is nothing to do */

    if( blt->status == BLT_UNINIT || blt->status == BLT_ROWS ) return;

    /* Check that the matrix is not already big enough */

    expand = 0;
    for( i = 0; i < blt->nrow; i++ )
    {
        bltrow *r = &(blt->row[i]);
        if( r->req < r->col || (i >= blt->nsparse && r->col > 0) )
        {
            expand = 1;
            break;
        }
    }

    /* If needs to expand, then reallocate the rows */

    if( expand ) alloc_bltrow_arrays( blt );
}

/* Copy bltmatrix bandwidth */

int copy_bltmatrix_bandwidth( bltmatrix *bltsrc, bltmatrix *bltcopy )
{
    int i;
    int nrow;

    if( bltsrc->nrow != bltcopy->nrow ) return INCONSISTENT_DATA;
    if( bltsrc->status != BLT_READY ) return INCONSISTENT_DATA;

    /* Ensure inverse matrix has non-zero rows for all elements of
       decomposition */

    nrow = bltsrc->nrow;
    for( i = 0; i < nrow; i++ )
    {
        blt_nonzero_element(bltcopy,i,bltsrc->row[i].col);
    }
    blt_set_sparse_rows(bltcopy,nrow);
    return OK;
}

/* Copy bltmatrix */

int copy_bltmatrix( bltmatrix *bltsrc, bltmatrix *bltcopy )
{
    int i;
    int nrow;
    int sts;

    sts = copy_bltmatrix_bandwidth( bltsrc, bltcopy );
    if( sts != OK ) return sts;

    expand_bltmatrix_to_requested( bltcopy );
    if( bltcopy->status != BLT_READY ) alloc_bltrow_arrays( bltcopy );

    /* Copy the data row by row */

    nrow = bltsrc->nrow;
    nrow = bltsrc->nrow;
    for( i = 0; i < nrow; i++ )
    {
        bltrow *rs = &(bltsrc->row[i]);
        bltrow *rc = &(bltcopy->row[i]);
        int cs = rs->col;
        int cc = rc->col;
        double *as = rs->address;
        double *ac = rc->address;
        while( cc < cs ) {
            cc++;
            *ac++ = 0.0;
        }
        /* while( cc <= i ){ cc++; *ac++ = *as++; } */
        memcpy( ac, as, sizeof(double)*(i+1-cs) );
    }

    return OK;
}


/* Calculate the cholesky decomposition */

int blt_chol_dec( bltmatrix *blt, int fill )
{
    double *r1, *r2;
    double sum;
    int i,j,k,jmin,kmin;
    int nfill;
    long ndone;

    nfill = 0;
    ndone = 0;

    init_progress_meter( blt->nelement );

    for (i = 0; i<blt->nrow; i++ )
    {

        /* For each row being decomposed, skip over all initial zero's in
        the row */

        jmin  = blt->row[i].col;

        for ( j = jmin; j<= i; j++ )
        {

            kmin = blt->row[j].col;
            if( jmin > kmin ) kmin = jmin;
            r1 = blt->row[i].address + kmin - jmin;
            r2 = blt->row[j].address + kmin - blt->row[j].col;
            sum = 0.0;

            for( k=kmin; k < j; k++ ) sum -= *r1++ * *r2++;

            sum += *r1;

            if ( i==j )
            {
                if( sum < *r1 * small || sum < abssmall )
                {
                    if( !fill ) return ++i;
                    nfill++;
                    sum = 1.0;
                }
                *r1 = sqrt( sum );
            }
            else
            {
                *r1 = sum/ *r2;
            }
            ndone++;
        }
        update_progress_meter( ndone );
    }
    end_progress_meter();
    return nfill;
}


/* Solve linear equations using the cholesky decomposition */

void blt_chol_slv( bltmatrix *blt, double *b, double *r )
{
    double *r1;
    double sum;
    int i,k,ik;
    for ( i=0; i<blt->nrow; i++ )
    {
        r1 = blt->row[i].address;
        for (k = blt->row[i].col, sum=0.0; k<i; k++ ) sum -= *r1++ * r[k];
        r[i] = (b[i]+sum) / *r1;
    }
    for ( i=blt->nrow; i--; )
    {
        for (k = blt->nrow, sum=0.0 ; --k > i; )
        {
            ik = i - blt->row[k].col;
            if( ik >= 0 )
            {
                sum -= r[k]* *(blt->row[k].address + ik);
            }
        }
        r[i] += sum;
        r[i] /= BLT(blt,i,i);
    }
}

/* Form the inverse within the banded structure using the
   cholesky decomposition.  The inverse is formed summing several
   columns
   at a time to reduce the number of page faults, as the matrix
   is stored in blocks of rows, so accessing column elements
   sequentially increases the incidence of faults, whereas accessing
   rows sequentially does not */

static void blt_load_col_cache( bltmatrix *blt, double **tmpcol, double **sumcol,
                                int *dosum, int iget, int nget, int isave, int nsave )
{

    int nrow = blt->nrow;
    int i;

    for( i = iget; i < nrow; i++ )
    {
        int col0 = blt->row[i].col;
        int c0;
        double *row = blt->row[i].address;
        double *r;

        /* If there are rows to be saved, then do so */

        if( nsave && i >= isave && dosum[i] < nsave )
        {
            for( c0 = dosum[i], r = row+isave+c0-col0; c0 < nsave; c0++, r++ )
            {
                if( isave+c0 > i ) break;
                *r = sumcol[c0][i];
            }
        }

        /* Count number of cols to get */

        dosum[i] = c0 = col0 < iget ? 0 : col0-iget;
        if( c0 >= nget ) continue;

        /* Retrieve the cols and initiallize the summation */

        for( r = row + iget + c0 - col0; c0 < nget; c0++, r++ )
        {
            if( iget + c0 > i ) break;
            tmpcol[c0][i] = *r;
            sumcol[c0][i] = 0.0;
        }
    }
}


void _blt_chol_inv_sumcol( bltmatrix *blt, int *dosum, double *sumcol, double *tmpcol, int i1, int c )
{
    int nrow = blt->nrow;
    for (int j=i1+1; j < nrow; j++ )
    {
        int col0 = blt->row[j].col;
        double *row = blt->row[j].address;

        if( col0 < i1+1 )
        {
            row += i1+1-col0;
            col0 = i1+1;
        }

        double sj=0;
        for( ; col0 <= j; col0++, row++ )
        {
            /* Sum effect of (j,col0) element, value at *row */
            if( c >= dosum[col0] && c >= dosum[j] )
            {
                sumcol[col0] -= *row * tmpcol[j];
                if( j != col0 )
                {
                    sj -= *row * tmpcol[col0];
                }
            }
        }
        sumcol[j] += sj;
    }
}

void blt_chol_inv( bltmatrix *blt )
{
    int nrow;
    int nsave;
    int i,i0,i1,c,c1,j;
    double *tmp;
    double **tmpcol;
    double **sumcol;
    int *dosum;
    int ncache;

    long ndone;

    nrow = blt->nrow;

    ncache = blt->_invncol;
    if( ncache <= 0 )
    {
        ncache=BLT_INV_CACHE_SIZE;
    }

    tmp = (double *) check_malloc( 2 * nrow * ncache * sizeof(double) );
    tmpcol = (double **) check_malloc( 2 * ncache * sizeof(double *) );
    sumcol=tmpcol+ncache;

    for( i = 0; i < ncache; i++ )
    {
        tmpcol[i] = tmp + nrow*i;
        sumcol[i] = tmp + nrow*(i+ncache);
    }
    dosum = (int *) check_malloc( nrow * sizeof(int) );

    init_progress_meter( blt->nelement );

    ndone = 0;
    nsave = 0;

    for (i1=nrow-1, i0=nrow-ncache;
            i1 >= 0;
            i1=i0-1, i0 -= ncache )
    {

        if( i0 < 0 ) i0 = 0;

        /* Save the cached row data and update with the new values ... */

        blt_load_col_cache( blt, tmpcol, sumcol, dosum, i0, i1-i0+1, i1+1, nsave );
        nsave = i1-i0+1;

        /* Sum the data for the rows after i0 into the summation using the multithreading hook
           if it has been defined. */

        if( blt->_pfsumcol )
        {
            (blt->_pfsumcol)(blt,nsave,dosum,sumcol,tmpcol,i1);
        }
        else
        {
            for ( c = nsave; c--;  )
            {
                _blt_chol_inv_sumcol(blt, dosum, sumcol[c], tmpcol[c], i1, c);
            }
        }

        /* Now process the cached columns to generate the inverse in
           sumcol */

        for( c = nsave; c--; )
        {
            double sc;
            int ic = i0 + c;
            for( j = nrow-1; j > ic; j-- )
            {
                /* Calculate the new element [ic,j] in sumcol */
                if( c < dosum[j] ) continue;
                sc = sumcol[c][j];
                sc /= tmpcol[c][ic];
                sumcol[c][j] = sc;
                ndone++;
                /* Update the sums affected by this element (ic,j) */
                for( c1 = dosum[j]; c1 < c; c1++ )
                {
                    if( c1 >= dosum[ic] )
                    {
                        sumcol[c1][ic] -= sc * tmpcol[c1][j];
                        sumcol[c1][j] -= sc * tmpcol[c1][ic];
                    }
                }
            }

            /* Process the diagonal element separately.  Main reason is
               that calculation this way improves chance of doing the entire
               sum in FPU, hence improving accuracy, and accuracy is more
               critical for diagonal element than for others. */

            sc = 1.0/tmpcol[c][ic];
            for( j = ic+1; j < nrow; j++ )
            {
                if( c >= dosum[j] ) sc -= sumcol[c][j] * tmpcol[c][j];
            }
            sc /= tmpcol[c][ic];
            sumcol[c][ic] = sc;

            for( c1 = dosum[ic]; c1 < c; c1++ )
            {
                sumcol[c1][ic] -= sc * tmpcol[c1][ic];
            }
        }

        update_progress_meter( ndone );
    }

    /* Save the last cached columns back again... */

    blt_load_col_cache( blt, tmpcol, sumcol, dosum, 0, 0, 0, nsave );
    end_progress_meter();

    check_free( tmpcol );
    check_free( tmp );
    check_free( dosum );
}

void print_bltmatrix( FILE *out, bltmatrix *blt, char *format, int indent )
{
    int i, j, k, cols, wid;
    char *f;

    for( f=format; *f != 0 && (*f < '0' || *f > '9'); f++);
    if( *f )
    {
        sscanf(f,"%d",&wid);
    }
    else
    {
        wid = 10;
    }

    cols = (80-indent)/(wid+1);
    if( cols < 4 ) cols = 4;


    for (k=0; k<blt->nrow; k+=cols)
    {
        fprintf(out,"\n%.*s      ",indent,"");
        for( j=k; j<blt->nrow && j<k+cols; j++) fprintf(out,"%*d ",wid,j+1);
        fprintf(out,"\n");
        for (i=k; i<blt->nrow; i++ )
        {
            fprintf(out,"%.*s%6d%s",indent,"",i+1,blt->row[i].alloc ? "*" : " ");
            for( j=k; j<=i && j<k+cols; j++)
            {
                if( j >= blt->row[i].col )
                {
                    fprintf(out,format,BLT(blt,i,j) );
                }
                else
                {
                    fprintf(out,"%*s",wid,"-");
                }
                fputs( " ", out);
            }
            fprintf(out,"\n");
        }
    }
}

double *blt_get_row_data( bltmatrix * blt, int irow )
{
    if( irow < 0 || irow > blt->nrow
            || blt->status != BLT_READY
            || blt->row[irow].col > 0 ) return NULL;

    return blt->row[irow].address;
}

void dump_bltmatrix( bltmatrix *blt, FILE *b )
{
    int i;
    if( blt->status != BLT_READY ) return;
    fwrite(&(blt->nrow),sizeof(blt->nrow),1,b);
    for( i = 0; i < blt->nrow; i++ )
    {
        fwrite(&(blt->row[i].col),sizeof(blt->row[i].col),1,b);
    }
    for( i = 0; i < blt->nrow; i++ )
    {
        bltrow *r = &(blt->row[i]);
        int len = i + 1 - r->col;
        fwrite(r->address,sizeof(double),len,b);
    }
}

int reload_bltmatrix( bltmatrix **pblt, FILE *b )
{
    int i;
    int nrow;
    bltmatrix *blt = NULL;

    *pblt = NULL;
    if( fread(&nrow,sizeof(nrow),1,b) != 1 ) return FILE_READ_ERROR;
    if( nrow <= 0 ) return INVALID_DATA;

    blt = create_bltmatrix(nrow);
    *pblt = blt;
    for( i = 0; i < blt->nrow; i++ )
    {
        int icol;
        if( fread(&icol,sizeof(icol),1,b) != 1 ) return FILE_READ_ERROR;
        if( icol < 0 || icol > i ) return INVALID_DATA;
        blt_nonzero_element(blt,i,icol);
    }
    blt_set_sparse_rows(blt,nrow);
    init_bltmatrix(blt);
    for( i = 0; i < blt->nrow; i++ )
    {
        bltrow *r = &(blt->row[i]);
        int len = i + 1 - r->col;
        if( fread(r->address,sizeof(double),len,b) != (unsigned int) len) return FILE_READ_ERROR;
    }
    return OK;
}

void print_bltmatrix_json( bltmatrix *blt, FILE *out, int nprefix, int options, const char *format )
{
    int ir,ic,ir0,ic0,col0;
    int nrows=blt->nrow;
    int matonly = options & BLT_JSON_MATRIX_ONLY;
    int rows = options & BLT_JSON_FULL;
    int lower = rows == BLT_JSON_LOWER;

    if( matonly && ! rows ) {
        rows=BLT_JSON_LOWER;
    }
    if( ! format ) format = "%15.8le";

    if( ! matonly )
    {
        fprintf(out,"{\n%*s\"nrows\": %d,\n%*s\"nsparse\": %d,\n%*s\"col0\": [",
                nprefix,"",nrows,nprefix,"",blt->nsparse,nprefix,"");
        for( ir=0; ir < nrows; ir++ )
        {
            if( ir )
            {
                if( ir %20 == 0 ) fprintf(out,",\n%*s",nprefix,"");
                else fprintf(out,",");
            }
            fprintf( out, "%d", blt->row[ir].col );
        }
        fprintf(out,"],\n");
        fprintf(out,"%*s\"matrix\": ",nprefix,"");
        nprefix += 2;
    }

    fprintf(out,"[");
    for( ir=0; ir < nrows; ir++ )
    {
        fprintf(out,"%s\n%*s  [",ir ? "," : "",nprefix,"");
        for( ic=0; ic < nrows; ic++ )
        {
            double val=0.0;
            if( ic > ir )
            {
                if( lower ) continue;
                ic0=ir;
                ir0=ic;
            }
            else
            {
                ic0=ic;
                ir0=ir;
            }
            col0=blt->row[ir0].col;
            if( ic0 < col0 )
            {
                if( ! rows ) continue;
            }
            else
            {
                val=blt->row[ir0].address[ic0-col0];
            }
            if( ic )
            {
                fprintf(out,",");
                if( ic % 10 == 0 ) fprintf(out,"\n%*s  ",nprefix,"");
            }
            fprintf( out, format, val );
        }
        fprintf(out,"]");
    }
    fprintf(out,"]");

    if( ! matonly )
    {
        nprefix -= 2;
        fprintf(out,"\n%*s}",nprefix,"");
    }
}


#ifdef TESTBLT

#include "util/symmatrx.h"

/*

Test code uses a very crude input file

First line is number of rows, number of sparse rows, and minimum allocation block size
5 5 4

Next row is vector b for Nx=b
2 -3 4 1 8

Next set of data is the lower triangle of the matrix
3
1 4
0 -1 4
0  0 0 2
0  1 0 1 5

Then to test expanding a matrix, add another number of sparse rows,
plus as many zero based non-zero row/columns as required

4
3 2

*/

int main(int argc, char *argv[] )
{
    FILE *in, *out;
    double *N,*b,*tmp,*N1,*b1,*p;
    bltmatrix *blt, *bltc;
    int *col;
    int nprm, nmem, nes;
    int i, j;
    double v;
    int expanded = 0;

    if( argc<2 || NULL == (in=fopen(argv[1],"r")) )
    {
        xprintf("Need input file as parameter\n");
        return;
    }

    out = stdout;
    if( argc>2 && NULL == (out = fopen(argv[2],"w")) )
    {
        xprintf("Cannot open output file\n");
        return;
    }



    fscanf(in,"%d%d%d",&nprm,&nes,&minrowsize);
    if( nprm <= 0 )
    {
        xprintf("Invalid number of parameters\n");
        return;
    }

    nmem = (nprm * (nprm+1))/2 + nprm;

    b = check_malloc( (2*nmem+nprm)*sizeof(double) );
    if( !b )
    {
        xprintf("Not enough memory");
        return;
    }
    blt = create_bltmatrix( nprm );
    bltc = create_bltmatrix( nprm );

    N = b+nprm;
    b1 = b+nmem;
    N1 = b1+nprm;
    tmp = b1+nmem;

    for( i = 0; i<nmem; i++ ) {
        fscanf(in,"%lf",&v);
        b[i] = b1[i] = v;
    }
    for( i = 0; i<nprm; i++ ) for( j=0; j<=i; j++ )
        {
            if( Lij(N,i,j) != 0.0 ) blt_nonzero_element( blt, i, j );
        }
    blt_set_sparse_rows( blt, nes );

    init_bltmatrix( blt );
    for( i = 0; i<nprm; i++ ) for( j=0; j<=i; j++ )
        {
            if( Lij(N,i,j) != 0.0 )
            {
                BLT(blt,i,j) = Lij(N,i,j);
            }
        }

    fprintf(out,"Matrix as read\n\n");
    print_ltmat( out, N, nprm, "%10.4lf", 0 );

    fprintf(out,"\nInput matrix\n");
    print_bltmatrix( out, blt, "%10.4lf", 0 );
    fprintf(out,"\nInput vector\n");
    for( i=0; i<nprm; i++ ) fprintf(out,"%3d  %10.4lf\n",i+1,b[i]);

    fprintf(out,"\n\nFULL SYMMETRIC SOLUTION\n");

    fprintf(out,"\nCholesky decomposition status = %d\n",(int)chol_dec(N,nprm));
    chol_slv( N, b1, b1, nprm );

    fprintf(out,"\nDecomposed matrix\n\n");
    print_ltmat( out, N, nprm, "%10.4lf", 0 );
    fprintf(out,"\nSolution vector\n");
    for( i=0; i<nprm; i++ ) fprintf(out,"%3d  %10.4lf\n",i+1,b1[i]);

    chol_inv( N, tmp, nprm );
    fprintf(out,"\nInverted matrix\n\n");
    print_ltmat( out, N, nprm, "%10.4lf", 0 );

    fprintf(out,"\n\nBANDED SOLUTION\n");

    fprintf(out,"\nCholesky decomposition status = %d\n",(int)blt_chol_dec(blt,0));

    fprintf(out,"\nDecomposed matrix\n");
    print_bltmatrix( out, blt, "%10.4lf", 0 );
    blt_chol_slv( blt, b, b1 );
    fprintf(out,"\nSolution vector\n");
    for( i=0; i<nprm; i++ ) fprintf(out,"%3d  %10.4lf\n",i+1,b1[i]);

    copy_bltmatrix(blt,bltc);

    if( fscanf(in,"%d",&nes) == 1 )
    {
        blt_set_sparse_rows(blt,nes);

        while( fscanf(in,"%d%d",&i,&j) == 2 )
        {
            blt_nonzero_element(blt,i,j);
        }
        expand_bltmatrix_to_requested(blt);

        fprintf(out,"\nExpanded choleski matrix\n");
        print_bltmatrix( out, blt, "%10.4lf", 0 );
        expanded = 1;
    }

    blt_chol_inv( blt );
    fprintf(out,"\nInverted matrix\n\n");
    print_bltmatrix( out, blt, "%10.4lf", 0 );

    fprintf(out,"\nCopy of decomposed matrix\n");
    print_bltmatrix( out, bltc, "%10.4lf", 0 );

    if( expanded )
    {
        copy_bltmatrix(bltc,blt);
        fprintf(out,"\nDecomposition copied back\n");
        print_bltmatrix( out, blt, "%10.4lf", 0 );
    }

    // Test dump and restore of file

    {
        FILE *bin;
        char *binfile  = "blt.bin";
        fprintf(out,"\nTesting dump and reload using %s\n",binfile);
        bin = fopen(binfile,"w+b");
        if( bin )
        {
            bltmatrix *blt2;
            dump_bltmatrix( blt, bin );
            fclose(bin);
            bin = fopen( binfile,"r+b");
            if( bin && reload_bltmatrix(&blt2,bin) == OK )
            {
                fprintf(out,"Reloaded matrix\n");
                print_bltmatrix(out,blt,"%10.4lf",0);
            }
            else
            {
                fprintf(out,"Reload failed\n");
            }
            if( bin ) fclose(bin);
        }
    }
}
#endif
