
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

#define BLT_INV_CACHE_SIZE 32

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

/* tmpcol and sumcol are row-major flat arrays: [row * stride + col_slot].
   stride == threadcount (maximum column slots per batch). */
static void blt_load_col_cache_mt( bltmatrix *blt, double *tmpcol, double *sumcol,
                                const int stride,
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
        double *sc_i = sumcol + i * stride;
        double *tc_i = tmpcol + i * stride;

        /* If there are rows to be saved, then do so */

        if( nsave && i >= isave && dosum[i] < nsave )
        {
            for( c0 = dosum[i], r = row+isave+c0-col0; c0 < nsave; c0++, r++ )
            {
                if( isave+c0 > i ) break;
                *r = sc_i[c0];
            }
        }

        /* Count number of cols to get */

        dosum[i] = c0 = col0 < iget ? 0 : col0-iget;
        if( c0 >= nget ) continue;

        /* Retrieve the cols and initialise the summation */

        for( r = row + iget + c0 - col0; c0 < nget; c0++, r++ )
        {
            if( iget + c0 > i ) break;
            tc_i[c0] = *r;
            sc_i[c0] = 0.0;
        }
    }
}

/* Accumulates the contribution of BLT rows [j_start, j_end) into sumcol via
 * the Cholesky inversion recurrence.
 *
 *   dosum[r]    – index of the first active column slot for BLT row r.
 *   tmpcol      – row-major flat array [row * stride + col_slot], read-only.
 *   sumcol      – row-major flat array [row * stride + col_slot]; this thread
 *                 owns rows k in [j_start, j_end) and writes there directly
 *                 (no race — exclusive ownership).
 *   spill       – private row-major flat array [(k - spill_base) * stride + col_slot]
 *                 for cross-thread writes (k < j_start); null for thread 0 (unused).
 *                 Caller accumulates into sumcol after all threads join.
 *   stride      – column slots per row (= threadcount).
 *   spill_base  – base row index for spill indexing (= i1+1 for the batch).
 *
 * Returns the minimum k written into spill, or j_start if no spill writes
 * occurred, so the caller can bound the accumulation to [k_min, j_start).
 */
static int blt_chol_inv_mt_rowrange( bltmatrix *blt, const int *dosum,
                                      const double *tmpcol,
                                      double *sumcol, double *spill,
                                      const int spill_base,
                                      const int ncols, const int stride,
                                      const int i1,
                                      const int j_start, const int j_end )
{
    int k_min = j_start;
    for( int j = j_start; j < j_end; j++ )
    {
        int col0 = blt->row[j].col;
        double *row = blt->row[j].address;

        if( col0 < i1+1 )
        {
            row += i1+1 - col0;
            col0 = i1+1;
        }

        const double *tc_j = tmpcol + j * stride;
        double       *sc_j = sumcol + j * stride;

        for( int k = col0; k <= j; k++, row++ )
        {
            double elem = *row;
            int c_start = dosum[k] > dosum[j] ? dosum[k] : dosum[j];
            const double *tc_k = tmpcol + k * stride;
            if( k >= j_start )
            {
                double *sc_k = sumcol + k * stride;
                if( j != k )
                {
                    for( int c = c_start; c < ncols; c++ )
                    {
                        sc_k[c] -= elem * tc_j[c];
                        sc_j[c] -= elem * tc_k[c];
                    }
                }
                else
                {
                    for( int c = c_start; c < ncols; c++ )
                        sc_k[c] -= elem * tc_j[c];
                }
            }
            else  /* k < j_start: cross-thread, accumulate into spill */
            {
                if( k < k_min ) k_min = k;
                double *sp_k = spill + (k - spill_base) * stride;
                for( int c = c_start; c < ncols; c++ )
                {
                    sp_k[c] -= elem * tc_j[c];
                    sc_j[c] -= elem * tc_k[c];
                }
            }
        }
    }
    return k_min;
}

void blt_chol_inv_mt( bltmatrix *blt )
{
    const int threadcount = blt_get_number_of_threads();
    if( threadcount < 2 )
    {
        blt_chol_inv(blt);
        return;
    }

    const int nrow   = blt->nrow;
    const int stride = BLT_INV_CACHE_SIZE;

    /* Row-major layout: [row * stride + col_slot], so the inner loop over
       col_slot is sequential — matches ST commit f4a828c2.  stride is fixed at
       BLT_INV_CACHE_SIZE (not threadcount) so the inner c loop runs 32 iterations
       regardless of thread count, amortising per-k overhead and keeping
       arithmetic intensity high. */
    std::vector<double> tmp(2 * nrow * stride);
    double * const tmpcol_flat = tmp.data();
    double * const sumcol_flat = tmp.data() + nrow * stride;

    std::vector<int> dosum(nrow);

    /* work_prefix[j] = cumulative element count for rows 0..j-1, used to
       partition rows by work rather than by count so threads get equal-effort
       ranges.  Uses unclamped blt->row[j].col (ignores spill_base clamping),
       which overestimates actual work for early batches — safe since it only
       causes cut-points to land slightly early, never out of range. */
    std::vector<long> work_prefix(nrow + 1);
    work_prefix[0] = 0;
    for( int j = 0; j < nrow; j++ )
        work_prefix[j+1] = work_prefix[j] + j - blt->row[j].col;

    /* Global work-balanced partition: j_splits[c] is the first row of thread c
       when spill_base==0.  Computed once here and clamped per-batch.  The spill
       buffer must be sized from these actual cut-points — a count-equal split
       would underallocate when skewed work pushes a cut-point beyond its
       count-equal equivalent. */
    std::vector<int> j_splits(threadcount + 1);
    j_splits[0] = 0;
    j_splits[threadcount] = nrow;
    {
        const long total_work = work_prefix[nrow];
        for( int c = 1; c < threadcount; c++ )
        {
            const long target = (long)c * total_work / threadcount;
            int lo = j_splits[c-1], hi = nrow;
            while( lo < hi )
            {
                const int mid = lo + (hi - lo) / 2;
                if( work_prefix[mid] < target ) lo = mid + 1;
                else hi = mid;
            }
            j_splits[c] = lo;
        }
    }

    /* spill_buf holds per-thread spill accumulators for cross-thread writes
       (k < j_start for thread t).  Thread t's spill covers rows [0, j_splits[t])
       at most, each row holding stride col slots.  Only O(bandwidth) rows are
       ever written; the rest stay zero.  Thread 0 has no spill and its pointer
       is left null. */
    long spill_total = 0;
    for( int c = 1; c < threadcount; c++ )
        spill_total += (long)j_splits[c] * stride;
    std::vector<double> spill_buf(spill_total, 0.0);
    std::vector<double *> thread_spill(threadcount, nullptr);
    {
        double *ptr = spill_buf.data();
        for( int i = 1; i < threadcount; i++ )
        {
            thread_spill[i] = ptr;
            ptr += (long)j_splits[i] * stride;
        }
    }

    init_progress_meter( blt->nelement );

    long ndone = 0;
    int nsave  = 0;

    for( int i1 = nrow-1, i0 = nrow-BLT_INV_CACHE_SIZE;
            i1 >= 0;
            i1 = i0-1, i0 -= BLT_INV_CACHE_SIZE )
    {
        if( i0 < 0 ) i0 = 0;

        /* Save the cached row data and update with the new values ... */

        blt_load_col_cache_mt( blt, tmpcol_flat, sumcol_flat, stride, dosum.data(),
                               i0, i1-i0+1, i1+1, nsave );
        nsave = i1-i0+1;

        /* Sum the data for the rows after i1 into the summation.
           Clamp the global work-balanced splits to spill_base for this batch. */

        const int spill_base = i1+1;
        std::vector<int> j_starts(threadcount + 1);
        j_starts[0] = spill_base;
        j_starts[threadcount] = nrow;
        for( int c = 1; c < threadcount; c++ )
            j_starts[c] = j_splits[c] > spill_base ? j_splits[c] : spill_base;

        std::vector<int> k_mins(threadcount);
        {
            std::vector<std::thread> threads;
            for( int c = 0; c < threadcount; c++ )
            {
                const int j_start = j_starts[c];
                const int j_end   = j_starts[c+1];
                double * const spill_c = thread_spill[c];
                threads.emplace_back( [=, &k_mins]() {
                    k_mins[c] = blt_chol_inv_mt_rowrange(
                        blt, dosum.data(), tmpcol_flat,
                        sumcol_flat, spill_c,
                        spill_base, nsave, stride, i1, j_start, j_end );
                });
            }
            for( auto &t : threads ) t.join();
        }

        /* Accumulate each thread's spill into sumcol and re-zero for the next
           batch.  Thread 0 has no spill (j_start_0 = spill_base, so k >= j_start
           always).  For threads 1..threadcount-1, only [k_mins[c], j_start_c)
           was written — O(bandwidth) rows rather than O(nrow). */
        for( int c = 1; c < threadcount; c++ )
        {
            double * const sp = thread_spill[c];
            for( int j = k_mins[c]; j < j_starts[c]; j++ )
            {
                double * const sc_j = sumcol_flat + j * stride;
                double * const sp_j = sp + (j - spill_base) * stride;
                for( int c1 = 0; c1 < nsave; c1++ )
                {
                    sc_j[c1] += sp_j[c1];
                    sp_j[c1] = 0.0;
                }
            }
        }

        /* Now process the cached columns to generate the inverse in
           sumcol */

        for( int c = nsave; c--; )
        {
            const int ic = i0 + c;
            double * const sc_ic = sumcol_flat + ic * stride;
            double * const tc_ic = tmpcol_flat + ic * stride;
            for( int j = nrow-1; j > ic; j-- )
            {
                /* Calculate the new element [ic,j] in sumcol */
                if( c < dosum[j] ) continue;
                double * const sc_j = sumcol_flat + j * stride;
                double * const tc_j = tmpcol_flat + j * stride;
                double sc = sc_j[c];
                sc /= tc_ic[c];
                sc_j[c] = sc;
                ndone++;
                /* Update the sums affected by this element (ic,j) */
                for( int c1 = dosum[j]; c1 < c; c1++ )
                {
                    if( c1 >= dosum[ic] )
                    {
                        sc_ic[c1] -= sc * tc_j[c1];
                        sc_j[c1]  -= sc * tc_ic[c1];
                    }
                }
            }

            /* Process the diagonal element separately.  Main reason is
               that calculation this way improves chance of doing the entire
               sum in FPU, hence improving accuracy, and accuracy is more
               critical for diagonal element than for others. */

            double sc = 1.0/tc_ic[c];
            for( int j = ic+1; j < nrow; j++ )
            {
                double * const sc_j = sumcol_flat + j * stride;
                double * const tc_j = tmpcol_flat + j * stride;
                if( c >= dosum[j] ) sc -= sc_j[c] * tc_j[c];
            }
            sc /= tc_ic[c];
            sc_ic[c] = sc;

            for( int c1 = dosum[ic]; c1 < c; c1++ )
            {
                sc_ic[c1] -= sc * tc_ic[c1];
            }
        }

        update_progress_meter( ndone );
    }

    /* Save the last cached columns back again... */

    blt_load_col_cache_mt( blt, tmpcol_flat, sumcol_flat, stride, dosum.data(),
                           0, 0, 0, nsave );
    end_progress_meter();
}
