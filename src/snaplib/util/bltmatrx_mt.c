
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

static void blt_sum_col_mt(bltmatrix *blt, int nsave, int *dosum, double **sumcol, double **tmpcol, int i1)
{
    std::vector<std::thread> threads;
    for( int c = 0; c < nsave; c++ )
    {
        threads.emplace_back(std::thread( _blt_chol_inv_sumcol,
                                          blt, dosum, sumcol[c], tmpcol[c], i1, c ));
    }

    for (auto &t : threads) {
        t.join();
    }
}

void blt_chol_inv_mt( bltmatrix *blt )
{
    int threadcount=blt_get_number_of_threads();
    if( threadcount < 2 )
    {
        blt_chol_inv(blt);
        return;
    }
    int invncol=blt->_invncol;
    blt->_invncol=threadcount;
    blt->_pfsumcol=blt_sum_col_mt;
    blt_chol_inv(blt);
    blt->_invncol=invncol;
    blt->_pfsumcol=0;
}
