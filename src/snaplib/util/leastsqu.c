#include "snapconfig.h"
/*

These routines handle a least squares problem formulated as

    y  =  Ap + af + e

where y are the observations
      A and a are observation equations
      p and f are parameters to be estimated.
The weight matrix (inverse of variance-covariance matrix) is
W.

The solution proceeds by eliminating f from the equations
before LSQ_SUMMING the normal equations.  We will use the notation
A' for the transpose of A, and A" for the inverse of A.

The equations are as follows:
first the following matrices are summed

	 N = A'WA
    b = A'Wy
    v = A'Wa
    w = a'Wa
    c = a'Wy

    M = (N - vw"v')
    d = b - vw"c

then
			      Variance
    p = M"d                   M"
    f = w"c - w"v'p           w" + w"v'M"vw"
	 y = Ap + af               BM"B' - aw"a'

where
    B = A - aw"v'


*********************************************************************/




/*
   $Log: leastsqu.c,v $
   Revision 1.5  2004/04/22 02:35:43  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.4  2004/02/12 02:57:06  ccrook
   Modified order of calculation in blt_chol_inv in order to be more efficient when
   paging virtual memory...

   Revision 1.3  1998/06/15 02:25:45  ccrook
   Modified to handle long integer number of observations

   Revision 1.2  1996/01/10 19:48:56  CHRIS
   Added function lsq_get_covariance_matrix_row to get a row of the
   covariance matrix in preparation to dumping out the contents.

   Revision 1.1  1996/01/03 21:58:53  CHRIS
   Initial revision

*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "util/errdef.h"
#include "util/leastsqu.h"
#include "util/bltmatrx.h"
#include "util/symmatrx.h"
#include "util/chkalloc.h"

/*------------------------------------------------------------*/
/*  Variables used in least squares summation                 */
/*                                                            */
/*  N = normal equations                                      */
/*  b = normal vector                                         */
/*  ssr =  sum of squared residuals                            */
/*  tmp = scratch array                                       */
/*  nprm = no. parameters explicitely LSQ_SOLVED for              */
/*  nobs = no. observations                                   */
/*  ndof = degrees of freedom                                 */
/*  seu = standard error of unit weight                       */
/*                                                            */
/*  The normal equations are stored in lower triangular       */
/*  storage.                                                  */
/*                                                            */
/*------------------------------------------------------------*/

static int lsq_status = LSQ_UNINIT;
static bltmatrix *N;
static double *b;
static double ssr;
static int    nprm = -1;
static long     nschp;
static long     nobs;
static long     ndof;
static double seu;
static obseqn *Asch = NULL;  /* Observation equations for Schreiber eqns */

static double *tmp;   /* A scratch work area */
static long  ntmp = 0;
static int   *cols;  /* A scratch area for integer column numbers */
static int   ncols = 0;

/*------------------------------------------------------------*/
/*  Routine to allocate memory for observations. The memory   */
/*  allocation is only increased, never decreased.  For most  */
/*  the calls the routine will simply return the address of   */
/*  the observation equation array.  The routine also         */
/*  initiallises the counters of columns                      */
/*                                                            */
/*  alloc_ls    Allocates space for least squares arrays      */
/*  init_ls     Initiallizes least squares equationss         */
/*  alloc_oe    Allocates space for a group of nobs eqns.     */
/*  alloc_vec   Allocates space for a vector                  */
/*                                                            */
/*  NOTE: alloc_ls must be called before alloc_oe             */
/*                                                            */
/*------------------------------------------------------------*/


static void sequence_error( const char *routine )
{
    handle_error( INTERNAL_ERROR, "Internal error: Out of sequence call to LSQ routine", routine);
}

static void alloc_tmp( long nval )
{
    if( nval <= ntmp ) return;
    if( ntmp ) check_free( tmp );
    tmp = (double *) check_malloc( nval * sizeof(double) );
    ntmp = nval;
}

static void alloc_cols( int ncol )
{
    if( ncol <= ncols ) return;
    if( ncols ) check_free( cols );
    cols = (int *) check_malloc( ncol * sizeof(int) );
    ncols = ncol;
}


void lsq_alloc( int nrow )
{
    if( nprm > 0 )
    {
        check_free( b );
        delete_bltmatrix( N );
        b = NULL;
        N = NULL;
    }
    nprm = nrow;
    if( nprm > 0 )
    {
        b = (double *) check_malloc(nrow * sizeof(double) );
        N = create_bltmatrix( nrow );
        alloc_tmp( (long) nrow );
    }

    lsq_status = LSQ_READY;
}


void lsq_init( void )
{
    int i;

    if( lsq_status == LSQ_UNINIT ) sequence_error( "lsq_init" );

    if( nprm )
    {
        init_bltmatrix( N );
        for( i=0; i<nprm; i++ ) b[i] = 0.0;
    }
    ssr = 0.0;
    nobs=0;
    nschp=0;
    lsq_status = LSQ_SUMMING;
}


bltmatrix *lsq_normal_matrix( void )
{
    return N;
}

/*---------------------------------------------------------------*/
/*  Routines to sum observations into the Normal equations       */
/*  Note that non-general matrix routines have been used rather  */
/*  than generallised routines for efficiency - the observation  */
/*  equations tend to be sparse.                                 */
/*                                                               */
/*  sum_cobs    Sums a pair of correlated observations with      */
/*              a given weight. Includes components from the     */
/*              Wij and Wji terms of the weight matrix.          */
/*                                                               */
/*  sum_sobs    Sums an observation with a given weight          */
/*                                                               */
/*  sum_eobs    Sums the explicit observations accounting for    */
/*              the diagonal or lower triangular matrix          */
/*                                                               */
/*  form_iobs   Forms the schreiber equations as dummy           */
/*              observations equations                           */
/*                                                               */
/*  sum_obseqn  Sums a group of observations, applying           */
/*              corrections for implicitely LSQ_SOLVED parameters.   */
/*                                                               */
/*---------------------------------------------------------------*/

static void sum_cobs( obsrow *o1, obsrow *o2, double wo1o2 )
{
    int     i1,   i2;
    int    *o1c, *o2c;
    double *o1v, *o2v;
    double  o1vw, o1o2w;

    if( o1->flag & OE_UNUSED ) return;
    if( o1->flag & OE_UNUSED ) return;
    o1c = o1->col;
    o1v = o1->val;
    for ( i1=0; i1++<o1->ncol; o1c++, o1v++)
    {
        o2c = o2->col;
        o2v = o2->val;
        o1vw = *o1v * wo1o2;
        b[*o1c] += o1vw * o2->obsv;

        for ( i2=0; i2++<o2->ncol; o2c++, o2v++ )
        {
            o1o2w = *o2v * o1vw;
            if( *o1c == *o2c) o1o2w += o1o2w;
            BLT(N,*o1c,*o2c) += o1o2w;
        }
    }

    o2c = o2->col;
    o2v = o2->val;
    for ( i2 = 0; i2++ < o2->ncol; o2c++, o2v++ )
    {
        b[*o2c] += *o2v * wo1o2 * o1->obsv;
    }

    ssr += o1->obsv * o2->obsv * wo1o2 * 2;
}


static void sum_sobs( obsrow *o1, double wo1 )
{
    int     i1,   i2;
    int    *o1c, *o2c;
    double *o1v, *o2v;
    double  o1vw;

    if( o1->flag & OE_UNUSED ) return;

    o1c = o1->col;
    o1v = o1->val;
    for ( i1=0; i1++ < o1->ncol; o1c++, o1v++)
    {
        o2c = o1->col;
        o2v = o1->val;
        o1vw = *o1v * wo1;
        b[*o1c] += o1vw * o1->obsv;
        for ( i2=0; i2++ < i1; o2c++, o2v++ )
        {
            BLT(N,*o1c,*o2c) += *o2v * o1vw;
        }
    }

    ssr += o1->obsv * o1->obsv * wo1;
}

static int sum_eobs( obseqn *A )
{
    obsrow **ob1, **ob2;
    ltmat wgt;
    int   i1,i2;
    int   nobs;
    char  fullcvr;

    wgt = A->wgt;
    fullcvr = !(A->flag & OE_DIAGONAL_CVR);
    nobs = A->nrow;

    for (i1=0, ob1=A->obs; i1<A->nrow; i1++, ob1++ )
    {
        if (fullcvr) for(i2=0, ob2=A->obs; i2++<i1; ob2++, wgt++)
                if(*wgt != 0.0) sum_cobs( *ob1, *ob2, *wgt );
        sum_sobs( *ob1, *wgt++ );
        if( (*ob1)->flag & OE_UNUSED ) nobs--;
    }
    return nobs;
}


static int count_col( obseqn *A )
{
    int irow, ncol;

    ncol = 0;      /* The dependent variable */

    for( irow = 0; irow < A->nrow; irow++ )
    {
        ncol += A->obs[irow]->ncol;
    }

    return ncol;
}

/* Find all the columns referenced in the observations equations     */
/* Store the column numbers in cols and return the number of columns */

static int list_cols( obseqn *A )
{
    int irow, icol, ncol, colno, i;
    obsrow *obs;

    ncol = count_col( A );
    alloc_cols( ncol );

    ncol = 0;

    for( irow = 0; irow<A->nrow; irow++ )
    {
        obs = A->obs[irow];

        for( icol = 0; icol<obs->ncol; icol++ )
        {
            colno = obs->col[icol];
            for( i=0; i<ncol; i++ )
            {
                if( cols[i] == colno ) break;
            }
            if( i >= ncol )
            {
                cols[ncol] = colno;
                ncol++;
            }
        }
    }
    return ncol;
}




static void form_iobs( obseqn *A, char sumall )
{
    int irow, icol, ncol;
    obsrow *obs;
    double  schwgt;
    double  wgt;

    /* Allocate work area for schreibers equations */

    if( Asch == NULL ) Asch = (obseqn *) create_oe( nprm );

    ncol = count_col( A );

    init_oe( (void *) Asch, 1, ncol, OE_DIAGONAL_CVR );

    /* Sum the schreibers equations */

    schwgt = 0;
    for( irow = 0; irow < A->nrow; irow++ )
    {
        obs = A->obs[irow];
        if( !sumall && obs->flag & OE_UNUSED ) continue;

        wgt = A->wgt[irow];   /* Assume that weight matrix is diagonal */
        wgt *= obs->schv;
        schwgt += wgt * obs->schv;

        oe_add_value( (void *) Asch, 1, obs->obsv*wgt );

        for( icol = 0; icol < obs->ncol; icol++ )
        {
            oe_add_param( Asch, 1, obs->col[icol]+1, obs->val[icol] * wgt );
        }
    }

    Asch->cvr[0] = schwgt;
    if( schwgt > 0 ) schwgt = 1.0/schwgt;
    Asch->wgt[0] = schwgt;
    return;
}


/* Copy the covariance matrix of the observation equations to the weight
   matrix, and eliminate unused rows and columns (actually convert to
   identity matrix so that they do not influence the inversion of the
   remaining rows */

static void copy_used_cvr_to_wgt( obseqn *A )
{
    ltmat wgt, cvr;
    long nelt;
    int i, j;
    char unused;

    wgt = A->wgt;
    cvr = A->cvr;

    nelt = ((long)A->nrow * (A->nrow+1))/2;
    while(nelt--) *wgt++ = *cvr++;

    /* Eliminate unused rows and columns (convert to the identity matrix) */

    wgt = A->wgt;
    for( i=0; i<A->nrow; i++, wgt++ )
    {
        unused = A->obs[i]->flag & OE_UNUSED;
        for( j=0; j<i; j++, wgt++ )
        {
            if( unused || A->obs[j]->flag & OE_UNUSED ) *wgt = 0.0;
        }
        if( unused ) *wgt = 1.0;
    }
}




static int calc_weight_matrix( obseqn *A )
{
    int inverse_error;
    int i;

    /* If it is diagonal simply form the reciprocal of each observation */

    inverse_error = 0;

    if( A->flag & OE_DIAGONAL_CVR )
    {
        for( i=0; i<A->nrow; i++ )
        {
            if( A->cvr[i] > 0.0 )
            {
                A->wgt[i] = 1.0/A->cvr[i];
            }
            else
            {
                A->wgt[i] = 0.0;
                inverse_error = i+1;
            }
        }
    }

    /* For a full lower triangle format - clear out rows/columns corresponding
       to unused observations, then invert the matrix */

    else
    {

        /* Copy the covariance matrix to the weight matrix */

        copy_used_cvr_to_wgt( A );

        /* Form the inverse */

        inverse_error = chol_dec( A->wgt, A->nrow );
        alloc_tmp( (long) A->nrow );
        if( !inverse_error ) chol_inv( A->wgt, tmp, A->nrow );
    }

    return inverse_error;
}



int lsq_sum_obseqn( void * hA)
{
    obseqn * A;
    int nused;
    int sts;

    if( lsq_status != LSQ_SUMMING ) sequence_error( "lsq_sum_obseqn" );

    A = (obseqn *) hA;

    /* Make sure we haven't got an invalid set of flags.. */

    if( A->flag & OE_SCHREIBER && !(A->flag & OE_DIAGONAL_CVR) )
    {
        handle_error(INTERNAL_ERROR, "Internal error: Incompatible flags passed to sum_obseqn", NO_MESSAGE );
        return 1;
    }

    /* Convert the covariance matrix to a weight matrix */

    sts = calc_weight_matrix( A );
    if( sts ) return sts;

    /* Sum the observation equations  - if there are no Schreiber equns
       or all the obs are flagged as unused, stop at this point. */

    nobs += nused = sum_eobs( A );
    if( !nused || ! (A->flag & OE_SCHREIBER) ) return 0;

    /* Correct the normal equations for the schreiber equations */
    /* and decrement the count of observations                  */

    nschp++;
    form_iobs( A, 0 );
    Asch->wgt[0] = -Asch->wgt[0];
    nobs -= sum_eobs( Asch );

    return 0;
}


void lsq_zero_b_vector( void )
{
    int i;
    for( i = 0; i<nprm; i++ ) b[i] = 0.0;
    ssr = 0.0;
}


int lsq_solve_equations( int force_solution )
{
    int i;
    int sts;

    if( lsq_status != LSQ_SUMMING ) sequence_error( "lsq_solve_equations" );

    sts = 0;

    if( nprm )
    {

        sts = blt_chol_dec( N, force_solution );
        if( sts && !force_solution )
        {
            lsq_status = LSQ_UNINIT;
            return sts;
        }

        /* Save the matrix */

        for( i = 0; i<nprm; i++ ) tmp[i] = b[i];

        /* Solve the equations */

        blt_chol_slv( N, b, b );

        /* Correct the ssr for the adjustments */

        for( i = 0; i<nprm; i++ ) ssr -= tmp[i]*b[i];

    }

    /* Calculate the dof and seu */

    ndof = nobs - nprm;
    seu = 1.0;
    if( ndof > 0 ) seu = ssr/ndof;
    if( seu > 0.0 ) seu = sqrt(seu);


    lsq_status = LSQ_SOLVED;
    return sts;
}



void lsq_get_stats( long *lsnobs, int *lsnprm, long *lsnschp,
                    long *lsdof, double *lsssr, double *lsseu )
{

    if( lsq_status == LSQ_UNINIT ) sequence_error( "lsq_solve_equations" );

    if( lsnobs ) *lsnobs = nobs;
    if( lsnprm ) *lsnprm = nprm;
    if( lsnschp) *lsnschp= nschp;
    if( lsdof )  *lsdof  = ndof;
    if( lsssr )  *lsssr  = ssr;
    if( lsseu )  *lsseu  = seu;
}


/* Routine to attempt to set the least squares status to LSQ_SOLVED or
   LSQ_INVERTED */

static void set_lsq_status( int required_status, char *routine )
{
    if( lsq_status == LSQ_INVERTED ) return;
    if( lsq_status == LSQ_SUMMING ) lsq_solve_equations( 0 );
    if( lsq_status == LSQ_UNINIT ) { sequence_error( routine ); return; }
    if( required_status == LSQ_INVERTED )
    {
        if( nprm ) blt_chol_inv( N );
        lsq_status = LSQ_INVERTED;
    }
}

/* Get a row of the covariance matrix */

double *lsq_get_covariance_row( int row )
{
    int i;
    set_lsq_status( LSQ_SOLVED, "get_covariance_row" );
    if( lsq_status != LSQ_SOLVED ) {sequence_error("get_covariance_row"); return NULL;}
    if( !nprm ) return 0;
    for( i = 0; i < nprm; i++ ) tmp[i] = 0.0;
    tmp[row] = 1.0;
    blt_chol_slv( N, tmp, tmp );
    return tmp;
}

/* Get the choleski decomposition */

bltmatrix *lsq_get_decomposition()
{
    set_lsq_status( LSQ_SOLVED, "get_decomposition" );
    if( lsq_status != LSQ_SOLVED ) {sequence_error("get_decomposition"); return NULL;}
    return N;
}
/* Get the covariance matrix decomposition */

bltmatrix *lsq_get_covariance_matrix()
{
    set_lsq_status( LSQ_INVERTED, "get_decomposition" );
    if( lsq_status != LSQ_INVERTED ) {sequence_error("get_decomposition"); return NULL;}
    return N;
}


/* Invert the normal equations explicitly */

void lsq_invert_normal_equations( void )
{
    set_lsq_status( LSQ_INVERTED, "invert_normal_equations" );
    return;
}


/* Routine to return one or more parameters with their a priori covariance */
/* Doesn't check that the parameter numbers are valid */

void lsq_get_params( int row[], int nrows, double *value, ltmat covar )
{
    int c, r;
    set_lsq_status( covar ? LSQ_INVERTED : LSQ_SOLVED, "lsq_get_params" );
    if( value )
    {
        for( c = 0; c<nrows; c++ )
        {
            value[c] = b[row[c]];
        }
    }
    if( covar )
    {
        for( c = 0; c<nrows; c++ ) for( r = 0; r<=c; r++ )
            {
                *covar++ = BLT(N,row[c],row[r]);
            }
    }
}


static double calc_vtNv( obsrow *o1, bltmatrix *N, obsrow *o2 )
{
    int *c1, *c2;
    double *v1, *v2, sum;
    int i1, i2;

    c1 = o1->col; v1=o1->val;
    sum = 0.0;

    for( i1 = o1->ncol; i1--; c1++, v1++ )
    {
        c2 = o2->col; v2=o2->val;
        for( i2 = o2->ncol; i2--; c2++, v2++ )
        {
            sum += *v1 * *v2 * BLT( N, *c1, *c2 );
        }
    }

    return sum;
}


/* Vector product of a vector in sparse storage format with a full
   vector */

static double vecprd_ov( obsrow *obs, ltmat vec )
{
    int icol;
    double *val;
    int *col;
    double sum;

    sum = 0.0;
    val = obs->val;
    col = obs->col;

    for( icol = obs->ncol; icol--; )
    {
        sum += *val++ * vec[*col++];
    }

    return sum;
}


/* Vector product of a vector in sparse storage format with a column
   of a symmetric matrix - assume that all required elements are present
   in the matrix. */

static double vecprd_ol( obsrow *obs, bltmatrix *N, int irow )
{
    int icol, jcol;
    double *val;
    int *col;
    double sum;

    sum = 0.0;
    val = obs->val;
    col = obs->col;

    for( icol = obs->ncol; icol--; )
    {
        jcol = *col++;
        sum += *val++ * BLT(N, irow, jcol);
    }

    return sum;
}


/* Vector product of a vector with a matrix in symmetric format */

static double vecprd_vl( ltmat vec, ltmat cvr, int irow, int nrow )
{
    int icol;
    double sum;

    sum = 0.0;

    for( icol = 0; icol < nrow; icol++ )
    {
        sum +=  vec[icol] * Lij(cvr, irow, icol);
    }

    return sum;
}


/* This is the really meaty bit!!! */

void lsq_calc_obs( void *hA, double *calc, double *res,
                   double *schprm, double *schvar,
                   char diagonal, ltmat calccvr, ltmat rescvr )
{

    obseqn *A;
    obsrow *obs;
    int irow, jrow, nrow, icol, jcol, ncol, i;
    int *col;
    double sum, *val;
    ltmat cvr, rcvr, ocvr, atna, ratna;
    char schreiber;
    char none_used;
    char wgtdiag;
    char irowunused, jrowunused;
    char simple, used;
    double schv;
    double cvrff = 0.0;

    if( calccvr || schvar )
    {
        set_lsq_status( LSQ_INVERTED,"lsq_calc_obs");
    }
    else
    {
        set_lsq_status( LSQ_SOLVED, "lsq_calc_obs" );
    }

    A = (obseqn *) hA;

    none_used = 0;
    schv = 0;

    /* If we were using Schreibers equations then calculate the value of
       the implicitely LSQ_SOLVED parameter (f) */


    schreiber = A->flag & OE_SCHREIBER;

    if( schreiber )
    {
        none_used = 1;
        for( irow = 0; irow < A->nrow; irow++ )
        {
            if( ! (A->obs[irow]->flag & OE_UNUSED ) ) { none_used = 0; break; }
        }

        calc_weight_matrix( A );
        form_iobs( A, none_used );

        schv = Asch->obs[0]->obsv;
        if( !none_used )
        {
            col = Asch->obs[0]->col;
            val = Asch->obs[0]->val;
            for( icol = Asch->obs[0]->ncol; icol--; col++, val++ )
            {
                schv -= *val * b[*col];
            }
        }
        schv = schv * Asch->wgt[0];
    }

    /* Determine the calculated parameters and residuals */

    for( irow = 0; irow < A->nrow; irow++ )
    {
        obs = A->obs[irow];
        *calc = obs->schv * schv + vecprd_ov( obs, b );

        if( res )
        {
            *res = obs->obsv - *calc;
            res++;
        }

        calc++;
    }

    /* Determine the implicit parameter and its variance if we have one */

    if( schreiber )
    {
        if(schprm) *schprm = schv;
        if(schvar || calccvr )
        {

            /* Determine the weight, then correct for the observation if
               required */

            cvrff = Asch->wgt[0];

            if( !none_used )
            {
                cvrff += Asch->wgt[0]*Asch->wgt[0] *
                         calc_vtNv( Asch->obs[0], N, Asch->obs[0] );
            }

            if( schvar ) *schvar = cvrff;
        }
    }

    /* OK - now do we want to calculate covariances, and if so are they
       full or diagonal

       NOTE: We can only calculate the covariance of the residuals if we
       have first calculated that for the calculated observations.

       */

    if( !calccvr ) return;

    /* If we have schreiber equations then calculate the covariance of the
       p, the parameters, and f, the implicitely LSQ_SOLVED parameter.  This
       covariance is -M"vw", unless no observations were actually used,
       in which case it is zero.  Store in tmp.  We only initiallize the
       columns of tmp that are referenced in this set of observations, but
       we need to make sure that this includes columns of rejected
       observations.  */

    if( schreiber )
    {
        obs = Asch->obs[0];
        if( none_used )
        {
            for( i=0; i<obs->ncol; i++ ) tmp[obs->col[i]] = 0.0;
        }
        else
        {

            /* Initiallize for unused columns */

            for( irow = 0; irow < A->nrow; irow++ )
            {
                if( A->obs[irow]->flag & OE_UNUSED )
                {
                    for( i=A->obs[irow]->ncol; i--; )
                    {
                        tmp[A->obs[irow]->col[i]] = 0;
                    }
                }
            }
            for( i=0; i<obs->ncol; i++ )
            {
                icol = obs->col[i];
                tmp[icol] = - Asch->wgt[0] * vecprd_ol( obs, N, icol );
            }
        }
    }

    /* Now determine the variances of the calculated quantities */
    /* This is determined by taking the product of the observation
       equations with the full covariance matrix of the derived parameters,
       comprising M" (in N), the covariance of f and p (in tmp),
       and the variance of f (in cvrff) */

    cvr = calccvr;

    for( irow = 0; irow < A->nrow; irow++ )
        for( jrow = diagonal ? irow : 0; jrow <= irow; jrow++, cvr++ )
        {
            *cvr = calc_vtNv( A->obs[irow], N, A->obs[jrow] );
            if( schreiber )
            {
                sum = A->obs[irow]->schv *
                      vecprd_ov( A->obs[jrow], tmp );
                *cvr += sum;
                if( irow == jrow ) *cvr += sum;
                else *cvr += A->obs[jrow]->schv *
                                 vecprd_ov( A->obs[irow], tmp );
                *cvr += A->obs[irow]->schv * A->obs[jrow]->schv * cvrff;
            }
        }

    if( !rescvr ) return;

    /* Now all that's left is the covariances of the residuals.  The only
       real problem is that the observations may not be used - otherwise
       this would be a simple difference between the observed and calculated
       residuals.  This is quite straightforward if there is no correlation
       between used and unused observations.  In this case we can use the
       following equations.

      Cobs(i,j) - Ccalc(i,j)   where both are used
      Cobs(i,j) + Ccalc(i,j)   where neither are used
      Cobs(i,j)                where only one is used.

       If the weight matrix is not diagonal there may be covariances -
       However we are still OK if either all or none of the observations
       are used...

       The variable simple is used to identify the easy cases*/

    wgtdiag = A->flag & OE_DIAGONAL_CVR;
    simple = wgtdiag;

    if( !simple )
    {
        simple = 1;
        used = A->obs[0]->flag & OE_UNUSED;
        for( irow = 1; irow < A->nrow; irow++ )
        {
            if( (A->obs[irow]->flag & OE_UNUSED) != used ) { simple = 0; break; }
        }
    }

    cvr = calccvr;
    rcvr = rescvr;
    ocvr = A->cvr;

    for( irow = 0; irow < A->nrow; irow++, cvr++, rcvr++ )
    {
        irowunused = A->obs[irow]->flag & OE_UNUSED;
        if( !diagonal )
        {
            for( jrow = 0; jrow < irow; jrow++, cvr++, rcvr++ )
            {
                *rcvr = wgtdiag ? 0.0 : *ocvr++;
                jrowunused = A->obs[jrow]->flag & OE_UNUSED;
                if( irowunused && jrowunused )
                {
                    *rcvr += *cvr;
                }
                else if( !(irowunused || jrowunused) )
                {
                    *rcvr -= *cvr;
                }
            }
        }
        else
        {
            if( !wgtdiag ) ocvr += irow;
        }
        *rcvr = *ocvr++;
        if( irowunused )
        {
            *rcvr += *cvr;
        }
        else
        {
            *rcvr -= *cvr;
        }
    }

    if( simple ) return;

    /* The worst is now left - sorting out correlation of the unused
       observations with the parameters.. */

    /* NOTE:  This bit will never be encountered if we have implicit
       parameters (Schreiber equations) since we restrict ourselves to
       diagonal weight matrices in that case */

    /* Allocate space for a nrow x nrow matrix plus a nprm vector.  We
       want to form the product

       A'N"A~

       where A~ has the unused rows eliminated.

       The product is formed by calculating a column of N"A~ at a time
       into tmp, then forming the product of that row with A'.  Note that
       we only need the elements of tmp for which there are non-zero columns
       in A.  We form a list of non-zero columns in cols, and keep a count
       of them in ncol.

       */

    ncol = list_cols( A );
    nrow = A->nrow;

    /* Allocate space for the arrays */

    alloc_tmp( (long) nrow * nrow + nprm );

    atna = tmp + nprm;

#define ATNA(i,j)  atna[(long)i * A->nrow + j]

    for( icol = 0; icol < nrow; icol++ )
    {

        obs = A->obs[icol];

        /* Ignore unused rows (as in A~) */

        if( obs->flag & OE_UNUSED )
        {
            for( irow = 0; irow < nrow; irow ++ ) ATNA(irow,icol) = 0.0;
        }

        /* Otherwise form the product A'N"A~ */

        else
        {

            /* First form a row of N"A~ */

            for( jcol = 0; jcol < ncol; jcol++ )
            {
                tmp[cols[jcol]] = vecprd_ol( obs, N, cols[jcol] );
            }

            /* Then multiply by A' */

            for( irow = 0; irow < nrow; irow ++ )
            {
                ATNA(irow,icol) = vecprd_ov( A->obs[irow], tmp );
            }
        }
    }

    /* Now postmultiply A'N"A~ by the inverse of the covariance matrix
       of the observations used - the weight matrix shouldn't have
       been calculated yet. */

    copy_used_cvr_to_wgt( A );
    chol_dec( A->wgt, nrow );

    for( irow = 0; irow < nrow; irow ++ )
    {
        ratna = atna + (long) irow * nrow;
        chol_slv( A->wgt, ratna, ratna, nrow );
    }

    /* Finally postmultiply by the covariance of the used and unused
       parameters and use this to correct the residuals. To simplify
       this, zero out all columns for which the corresponding data
       has been used */

    for( icol = 0; icol < nrow; icol++ )
    {
        if( A->obs[icol]->flag & OE_UNUSED )
        {
            for( irow = 0; irow < nrow; irow++ )
            {
                ATNA(irow,icol) = 0.0;
            }
        }
    }

    /* Now apply the corrections to the covariance matrix of the residuals */


    for( irow = 0; irow < nrow; irow++ )
    {
        irowunused = A->obs[irow]->flag & OE_UNUSED;
        for( jrow = diagonal ? irow : 0; jrow <= irow; jrow++ )
        {
            jrowunused = A->obs[jrow]->flag & OE_UNUSED;
            if( irowunused )
            {
                Lij(rescvr,irow,jrow) -=
                    vecprd_vl( atna+(long)jrow * nrow, A->cvr, irow, nrow );
            }
            if( jrowunused )
            {
                Lij(rescvr,irow,jrow) -=
                    vecprd_vl( atna+(long)irow * nrow, A->cvr, jrow, nrow );
            }
        }
    }


#undef ATNA

}


#ifdef TESTLSQ




/*===================================================================*/
/*  TEST CODE FOR LEAST SQUARES ROUTINES                             */



int read_obseqn( FILE *in, void *hA, char *diag )
{
    int nrow, ncol, icol;
    char opts[5];
    char flag;
    char full;
    double val;
    int i, j;

    if( fscanf(in, "%d%d%4s",&nrow,&ncol,&opts ) != 3 ) return 0;
    if( opts[0] == 'L' ) flag = OE_LOWERTRI_CVR; else flag = OE_DIAGONAL_CVR;
    if( opts[1] == 'S' ) flag |= OE_SCHREIBER;
    if( opts[2] == 'F' ) full = 1; else full = 0;
    if( opts[3] == 'D' ) *diag = 1; else *diag = 0;

    init_oe( hA, nrow, ncol, flag );

    for( i = 0; i++ < nrow; )
    {
        if(fscanf(in,"%lf", &val ) == 0)
        {
            if( fgetc(in) == '*' )
            {
                oe_flag_unused( hA, i);
                if( fscanf(in,"%lf",&val) != 1 ) return 0;
            }
            else
                return 0;
        }
        oe_value( hA, i, val );
        if( flag & OE_SCHREIBER )
        {
            if( fscanf(in, "%lf", &val ) != 1 ) return 0;
            oe_schreiber( hA, i, val );
        }
        if( full )
        {
            for( icol = 0; icol++<nprm; )
            {
                fscanf(in,"%lf",&val);
                if( val != 0.0 ) oe_param( hA, i, icol, val );
            }
        }
        else
        {
            for(;;)
            {
                fscanf(in,"%d",&icol);
                if( icol <= 0 ) break;
                fscanf(in,"%lf",&val);
                oe_param( hA, i, icol, val );
            }
        }
    }

    for( i = 0; i++ < nrow; )
    {
        for( j = flag & OE_DIAGONAL_CVR ? i-1 : 0; j++ < i; )
        {
            if( fscanf(in, "%lf", &val ) != 1 ) return 0;
            oe_covar( hA, i, j, val );
        }
    }
    return 1;
}




int main( int argc, char *argv[] )
{
    FILE *in, *out;
    int nprm;
    long nobs, nschp, dof;
    int maxrow;
    double seu, ssr;
    void *hA;
    obseqn *A;
    int i;
    double val, var;
    ltmat calccvr, rescvr;
    double *calcval, *resval;
    double schval, schvar;
    long nelt;
    char diag;


    if( argc < 2 ) { xprintf("Need input file name as parameter\n"); return 0; }
    if( NULL == (in = fopen(argv[1],"r") ) )
    {
        xprintf("Cannot open file %s\n",argv[1]);
        return 0;
    }
    if( argc < 3 )
    {
        out = stdout;
    }
    else
    {
        out = fopen(argv[2],"w");
        if( out == NULL ) { xprintf("Cannot open output file %s\n",argv[2]); return 0;}
    }

    /* Read in the order of the equations */

    fscanf(in,"%d",&nprm);

    /* Initiallize the equations... */

    lsq_alloc( nprm );
    lsq_init( );

    /* Read in the observation equations... */

    hA = create_oe(nprm);

    maxrow = 0;
    while( read_obseqn( in, hA, &diag ) )
    {
        lsq_sum_obseqn( hA );
        print_obseqn( out, hA );
        A = (obseqn *) hA;
        if( A->nrow > maxrow ) maxrow = A->nrow;
    }

    /* Dump out the least squares summation, including b and ssr */

    fputs("\n\nNormal matrix plus b and ssr in final row/column\n",out);
    print_bltmatrix( out, N, "%10.3le", 0 );

    fprintf(out,"\n\nSOLUTION GIVES STATUS %d\n", (int) lsq_solve_equations( 0 ) );

    lsq_get_stats( &nobs, &nprm, &nschp, &dof, &ssr, &seu );

    fprintf(out,"\nSTATISTICS\n");
    fprintf(out,"nobs = %ld\nnprm = %d\ndof = %ld\nssr = %.4lf\nseu = %.4lf\n\n",
            nobs,(int) nprm,dof,ssr,seu);

    for( i = 0; i++ < nprm ; )
    {
        lsq_get_params( i, 1, &val, (ltmat) &var );
        fprintf(out,"Param %d = %.4lf  var %.8lf\n",(int) i,val,var);
    }

    /* Dump out the least squares summation, including b and ssr */

    fputs("\n\nCovariance matrix of parameters\n",out);
    print_bltmatrix( out, N, "%10.3le", 0 );

    /* Create arrays that will be needed..  */

    nelt = ( (long) maxrow * (maxrow+1))/2;
    calccvr = (ltmat) malloc( nelt * sizeof(double) );
    rescvr = (ltmat) malloc( nelt * sizeof(double) );
    if( !calccvr || !rescvr ) handle_error( MEM_ALLOC_ERROR, NULL, NULL) ;

    calcval = (double *) check_malloc( maxrow * sizeof(double) );
    resval = (double *) check_malloc( maxrow * sizeof(double) );

    /* Now go over all observations dumping out the parameters */

    fseek( in, 0L, SEEK_SET );
    fscanf( in, "%*d" );

    while( read_obseqn( in, hA, &diag ))
    {

        A = (obseqn *) hA;

        lsq_calc_obs( hA, calcval, resval, &schval, &schvar,
                      diag, calccvr, rescvr );

        fprintf(out,"\n\nObservation equations\n   observed  calculated  residual\n");

        for( i = 0; i<A->nrow; i++ )
        {
            fprintf(out,"   %10.4lf  %10.4lf  %10.4lf\n",A->obs[i]->obsv,
                    calcval[i], resval[i] );
        }

        if( diag )
        {
            fprintf(out,"\nCovariance of calculated values and residuals\n");
            for( i=0; i<A->nrow; i++ )
            {
                fprintf(out,"%3d   %10.3le  %10.3le\n",(int) i,calccvr[i],rescvr[i]);
            }
        }
        else
        {
            fprintf(out,"\nCovariance of calculated values\n");
            print_ltmat( out, calccvr, A->nrow, "%10.3le", 0 );

            fprintf(out,"\nCovariance of residual values\n");
            print_ltmat( out, rescvr, A->nrow, "%10.3le", 0 );
        }

        if( A->flag & OE_SCHREIBER )
        {
            fprintf(out,"\n Schreiber %10.4lf  var %10.3le\n",schval,schvar );
        }

    }

    return 0;
}

#endif
