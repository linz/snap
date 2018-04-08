
#include "snapconfig.h"


#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <thread>
#include <vector>

#include "util/bltmatrx.h"
#include "bltmatrx_mt.h"
#include "util/chkalloc.h"
#include "util/progress.h"
#include "util/errdef.h"


enum { BLT_UNINIT, BLT_ROWS, BLT_READY };

// static int minrowsize = 1000;
// static double small = 1.0e-10;
// static double abssmall = 1.0e-30;

static int blt_number_of_threads=-1;


void blt_set_number_of_threads( int threadcount )
{
    blt_number_of_threads=threadcount;
}

static int blt_get_number_of_threads()
{
    int threadcount=blt_number_of_threads;
    if( threadcount < 0 )
    {
        threadcount=std::thread::hardware_concurrency();
    }
    if( threadcount <= 0 )
    {
        threadcount=1;
    }
    return threadcount;
}

static void blt_load_col_cache_mt( bltmatrix *blt, double **tmpcol, double **sumcol,
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

static void blt_chol_inv_mt_sumcol( bltmatrix *blt, int *dosum, double **sumcol, double **tmpcol, int i0, int i1, int c )
{
    int nrow = blt->nrow;
    // Check: Just wondering if this should be i=i0+c,for(j=1+1...
    for (int j=i1+1; j < nrow; j++ )
    {
        int col0 = blt->row[j].col;
        double *row = blt->row[j].address;

        if( col0 < i1+1 )
        {
            row += i1+1-col0;
            col0 = i1+1;
        }

        for( ; col0 <= j; col0++, row++ )
        {
            /* Sum effect of (j,col0) element, value at *row */
                if( c >= dosum[col0] && c >= dosum[j] )
                {
                    sumcol[c][col0] -= *row * tmpcol[c][j];
                    if( j != col0 )
                    {
                        sumcol[c][j] -= *row * tmpcol[c][col0];
                    }
                }
            }
        }
}

void blt_chol_inv_mt( bltmatrix *blt )
{
    int nrow;
    int nsave;
    int i,i0,i1,c,c1,j;
    double *tmp;
    double **tmpcol;
    double **sumcol;
    int *dosum;
    long ndone;

    int threadcount=blt_get_number_of_threads();
    if( threadcount < 2 )
    {
        blt_chol_inv(blt);
        return;
    }

    nrow = blt->nrow;

    tmp = (double *) check_malloc( 2 * nrow * threadcount * sizeof(double) );
    tmpcol = (double **) check_malloc( 2 * threadcount * sizeof(double *) );
    sumcol=tmpcol+threadcount;

    for( i = 0; i < threadcount; i++ )
    {
        tmpcol[i] = tmp + nrow*i;
        sumcol[i] = tmp + nrow*(i+threadcount);
    }
    dosum = (int *) check_malloc( nrow * sizeof(int) );

    init_progress_meter( blt->nelement );

    ndone = 0;
    nsave = 0;

    for (i1=nrow-1, i0=nrow-threadcount;
            i1 >= 0;
            i1=i0-1, i0 -= threadcount )
    {

        if( i0 < 0 ) i0 = 0;

        /* Save the cached row data and update with the new values ... */

        blt_load_col_cache_mt( blt, tmpcol, sumcol, dosum, i0, i1-i0+1, i1+1, nsave );
        nsave = i1-i0+1;

        /* Sum the data for the rows after i0 into the summation */

        std::vector<std::thread> threads;
        for( c = 0; c < nsave; c++ )
        {
            threads.emplace_back(std::thread( blt_chol_inv_mt_sumcol, 
                        blt, dosum, sumcol, tmpcol, i0, i1, c ));
        }
        
        for (auto &t : threads){ t.join(); }

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

    blt_load_col_cache_mt( blt, tmpcol, sumcol, dosum, 0, 0, 0, nsave );
    end_progress_meter();

    check_free( tmpcol );
    check_free( tmp );
    check_free( dosum );
}
