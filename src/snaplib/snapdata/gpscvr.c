#include "snapconfig.h"
/* gpscvr.c: Code to create and maintain a covariance matrix for GPS data */
/* External routines required is:
     station_vertical( int stn, double vert[3] );
     get_topocentre( double *lat, double *lon );
*/

/*
   $Log: gpscvr.c,v $
   Revision 1.1  1995/12/22 18:45:31  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>
#include <string.h>

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <assert.h>

#include "snapdata/gpscvr.h"
#include "snapdata/datatype.h"
#include "snapdata/loaddata.h"
#include "util/symmatrx.h"
#include "util/geodetic.h"
#include "util/errdef.h"

char fixed_gps_vertical = 0;
static double fixed_rmat[3][3];
static char fixed_rmat_calculated = 0;

static VRTFUNC vertfunc = (VRTFUNC) 0;
static XFMFUNC xfmfunc = (XFMFUNC) 0;

static double dflttmat[] = { 1.0, 0.0, 0.0,
                             0.0, 1.0, 0.0,
                             0.0, 0.0, 1.0
                           };

void set_gps_vertical_fixed( int fixed )
{
    fixed_gps_vertical = fixed ? 1 : 0;
}

int gps_vertical_fixed( void )
{
    return fixed_gps_vertical;
}


static void rmat_from_vertical( double rmat[3][3] )
{
    double *vece, *vecn, *vecu;
    double r;

    vece = rmat[0];
    vecn = rmat[1];
    vecu = rmat[2];

    scalevec( vecu, 1.0/veclen(vecu) );   /* Up vector */
    vece[0] = -vecu[1];
    vece[1] = vecu[0];
    vece[2] = 0.0;
    r = veclen( vece );
    if( r > 0.0 )
    {
        scalevec( vece, 1.0/r );              /* East vector */
        vecn[2] = r;
        r = sqrt( 1.0-r*r)/r;
        if( vecu[2] > 0.0 ) r = -r;
        vecn[0] = vecu[0]*r;
        vecn[1] = vecu[1]*r;                  /* North vector */
    }
    else
    {
        vece[0] = vecn[1] = 0.0;
        vece[1] = 1.0;
        vecn[1] = -1.0;
        vece[2] = vecn[2] = 0.0;
    }
}


static void midpoint_rmat( int st1, int st2, double rmat[3][3] )
{
    double *vecu, tmp[3];
    assert(vertfunc != NULL);
    vecu = rmat[2];
    vecu[0] = vecu[1] = vecu[2] = 0.0;
    if( st1 ) (*vertfunc)( st1, vecu );
    if( st2 ) { (*vertfunc)( st2, tmp ); vecadd(vecu,tmp,vecu); }

    rmat_from_vertical( rmat );
}

static void set_midpoint_rmat( survdata *sd, double rmat[3][3] )
{
    double *vecu, tmp[3];
    assert(vertfunc != NULL);
    vecu = rmat[2];
    vecu[0] = vecu[1] = vecu[2] = 0.0;
    if( sd->from ) (*vertfunc)( sd->from, vecu );
    for( int iobs=0; iobs < sd->nobs; iobs++ )
    {
        int to=sd->obs.vdata[iobs].tgt.to;
        if( to) {(*vertfunc)( to, tmp ); vecadd(vecu,tmp,vecu); }
    }
    rmat_from_vertical( rmat );
}


static void set_gps_vertical( void )
{
    if( !fixed_rmat_calculated )
    {
        assert(vertfunc != NULL);
        (*vertfunc)( 0, fixed_rmat[2] );
        rmat_from_vertical( fixed_rmat );
    }
}


static void get_enu_rmat( int st1, int st2, double rmat[3][3] )
{
    if( fixed_gps_vertical )
    {
        if( !fixed_rmat_calculated )  set_gps_vertical();
        memcpy( rmat, fixed_rmat, sizeof(fixed_rmat) );
        return;
    }

    midpoint_rmat( st1, st2, rmat );
}

static void get_set_enu_rmat( survdata *sd, double rmat[3][3] )
{
    if( fixed_gps_vertical )
    {
        if( !fixed_rmat_calculated )  set_gps_vertical();
        memcpy( rmat, fixed_rmat, sizeof(fixed_rmat) );
        return;
    }
    set_midpoint_rmat( sd, rmat );
}


#define SWAPD(a,b)  {double tmp; tmp = a; a=b; b=tmp;}

static void apply_rf_to_enu_xform( int rfid, double xform[3][3] )
{
    double *tmat;

    /* Transpose the matrix gives enu->xyz rotation matrix */

    SWAPD(xform[0][1],xform[1][0]);
    SWAPD(xform[0][2],xform[2][0]);
    SWAPD(xform[1][2],xform[2][1]);

    /* Premultiply by reference frame matrix xyz->rf */

    if( xfmfunc )
    {
        tmat = (*xfmfunc)( rfid, 0 );
    }
    else
    {
        tmat = dflttmat;
    }

    premult3( tmat, (double *) xform, (double *) xform, 3 );
}

#undef SWAPD

static void get_enu_rf_xform( int st1, int st2, int rfid,
                              double xform[3][3] )
{
    get_enu_rmat( st1, st2, xform );
    apply_rf_to_enu_xform( rfid, xform );
}

static void get_set_enu_rf_xform( survdata *sd, double xform[3][3] )
{
    get_set_enu_rmat( sd, xform );
    apply_rf_to_enu_xform( sd->reffrm, xform );
}


static void get_rf_enu_xform( int st1, int st2, int rf,
                              double xform[3][3] )
{
    double rmat[3][3];
    double *tmat;

    get_enu_rmat( st1, st2, rmat );

    if( xfmfunc )
    {
        tmat = (*xfmfunc)( rf, 1 );
    }
    else
    {
        tmat = dflttmat;
    }
    premult3( (double *) rmat, tmat, (double *) xform, 3 );
}



static void calc_default_cvr( double length, ltmat cvr, double *mmerr2,
                              double *ppmerr2 )
{
    int i, j;
    double l2;

    l2 = length * length;

    for( i = 0, j=0; i < 3; i++, j+=i+1 )
    {
        cvr[j] = mmerr2[i] + l2 * ppmerr2[i];
    }

    cvr[1] = cvr[3] = cvr[4] = 0.0;
}


/* Transform the covariance matrix C to TCT' */

static void transform_cvr( double xform[3][3], ltmat cvr )
{
    double fullcvr[3][3];
    int i, j, k;

    /* Make a full copy of the covariance matrix */

    fullcvr[0][0] = cvr[0];
    fullcvr[0][1] = fullcvr[1][0] = cvr[1];
    fullcvr[1][1] = cvr[2];
    fullcvr[0][2] = fullcvr[2][0] = cvr[3];
    fullcvr[1][2] = fullcvr[2][1] = cvr[4];
    fullcvr[2][2] = cvr[5];

    /* Premultiply by xform */

    premult3( (double *) xform, (double *) fullcvr, (double *) fullcvr, 3 );

    /* Now form the transformed covariance matrix */

    for( i = 0, k=0; i<3; i++ ) for( j=0; j<=i; j++, k++ )
        {
            cvr[k] = vecdot( fullcvr[i], xform[j] );
        }

}


/* The default covariance for baselines is constructed so that each relative vector
   between stations has the default covariance.

   The error is supplied as an absolute error mmerr(0..2) and ppm error mmerr(3..5)
   This requires that the
   covariance between vectors 1-2 and 1-3 is
      (C(D12) + C(D13) - C(D23))/2.0
   where C(Dij) is the default covariance for the vector from i to j. */

static void build_default_baseline_cvr( survdata *vd, double *mmerr )
{
    double xform[3][3];
    int i,iobs, jobs, iobs3, jobs3;
    double tcvr[6], dif[3];
    vecdata *t;
    ltmat cvr;
    double mmerr2[3], ppmerr2[3];

    /* Convert the mm and ppm errors to more useful numbers */

    for( i=0; i<3; i++)
    {
        mmerr2[i] = mmerr[i]*mmerr[i]*1.0e-6;
        ppmerr2[i] = mmerr[i+3]*mmerr[i+3]*1.0e-12;
    }

    /* Since baselines are common, make a special case */

    cvr = vd->cvr;
    t = vd->obs.vdata;

    if( vd->nobs == 1 )
    {
        get_enu_rf_xform( vd->from, t->tgt.to, vd->reffrm, xform );
        calc_default_cvr( veclen(t->vector), cvr, mmerr2, ppmerr2 );
        transform_cvr( xform, cvr );
        return;
    }

    if( fixed_gps_vertical ) get_enu_rf_xform( 0, 0, vd->reffrm, xform );

    /* Clear out the covariance matrix */

    for( iobs = 0; iobs < vd->nobs*3; iobs++ ) for( jobs = 0; jobs <= iobs; jobs++ )
        {
            Lij( cvr, iobs, jobs ) = 0.0;
        }

    /* Calculate the covariances of the supplied vectors and
       also store their component in the correlations */

    for( iobs = 0; iobs < vd->nobs; iobs++ )
    {

        /* Generate the covariance matrix */

        if( !fixed_gps_vertical ) get_enu_rf_xform( vd->from, t[iobs].tgt.to, vd->reffrm, xform );
        calc_default_cvr( veclen(t[iobs].vector ), tcvr, mmerr2, ppmerr2 );
        transform_cvr( xform, tcvr );

        /* Set the diagonal elements */

        iobs3 = iobs*3;
        Lij(cvr, iobs3,  iobs3  ) = tcvr[0];
        Lij(cvr, iobs3,  iobs3+1) = tcvr[1];
        Lij(cvr, iobs3+1,iobs3+1) = tcvr[2];
        Lij(cvr, iobs3  ,iobs3+2) = tcvr[3];
        Lij(cvr, iobs3+1,iobs3+2) = tcvr[4];
        Lij(cvr, iobs3+2,iobs3+2) = tcvr[5];

        /* Now handle the off-diagonal elements */

        for( jobs = 0; jobs < vd->nobs; jobs ++ )
        {
            if( jobs == iobs ) continue;
            int jobs3 = jobs*3;

            Lij(cvr, iobs3,  jobs3  ) += tcvr[0]/2;
            Lij(cvr, iobs3,  jobs3+1) += tcvr[1]/2;
            Lij(cvr, iobs3,  jobs3+2) += tcvr[3]/2;
            Lij(cvr, iobs3+1,jobs3  ) += tcvr[1]/2;
            Lij(cvr, iobs3+1,jobs3+1) += tcvr[2]/2;
            Lij(cvr, iobs3+1,jobs3+2) += tcvr[4]/2;
            Lij(cvr, iobs3+2,jobs3  ) += tcvr[3]/2;
            Lij(cvr, iobs3+2,jobs3+1) += tcvr[4]/2;
            Lij(cvr, iobs3+2,jobs3+2) += tcvr[5]/2;
        }
    }

    /* Now determine the covariances of the derived vectors */

    for( iobs = 1; iobs < vd->nobs; iobs++ ) for( jobs = 0; jobs < iobs; jobs++ )
        {

            /* Generate the covariance matrix */

            if( !fixed_gps_vertical )
                get_enu_rf_xform( t[iobs].tgt.to, t[jobs].tgt.to, vd->reffrm, xform );
            vecdif( t[iobs].vector, t[jobs].vector, dif );
            calc_default_cvr( veclen(dif), tcvr, mmerr2, ppmerr2 );
            transform_cvr( xform, tcvr );

            iobs3 = iobs * 3;
            jobs3 = jobs * 3;

            Lij(cvr, iobs3,  jobs3  ) -= tcvr[0]/2;
            Lij(cvr, iobs3,  jobs3+1) -= tcvr[1]/2;
            Lij(cvr, iobs3,  jobs3+2) -= tcvr[3]/2;
            Lij(cvr, iobs3+1,jobs3  ) -= tcvr[1]/2;
            Lij(cvr, iobs3+1,jobs3+1) -= tcvr[2]/2;
            Lij(cvr, iobs3+1,jobs3+2) -= tcvr[4]/2;
            Lij(cvr, iobs3+2,jobs3  ) -= tcvr[3]/2;
            Lij(cvr, iobs3+2,jobs3+1) -= tcvr[4]/2;
            Lij(cvr, iobs3+2,jobs3+2) -= tcvr[5]/2;

        }
}


/* The default covariance for point data is constructed from the supplied
   station variances mmerr(0..2) and the covariances mmerr(3..5) and ppmerr(6..8) errors
   between them.
   */

static void build_default_point_cvr( survdata *vd, double *mmerr )
{
    double xform[3][3];
    int havecvr;
    int nobs, i,iobs, jobs, iobs3, jobs3;
    double tcvr[6], dif[3];
    vecdata *t;
    ltmat cvr;
    double abserr2[3], mmerr2[3], ppmerr2[3];

    nobs = vd->nobs;
    cvr = vd->cvr;
    t = vd->obs.vdata;

    /* Convert the mm and ppm errors to more useful numbers */

    havecvr = 0;
    for( i=0; i<3; i++)
    {
        abserr2[i] = mmerr[i]*mmerr[i]*1.0e-6;
        if( nobs < 2 ) continue;
        mmerr2[i] = mmerr[i+3]*mmerr[i+3]*1.0e-6;
        ppmerr2[i] = mmerr[i+6]*mmerr[i+6]*1.0e-12;
        if( mmerr2[i] > 0.0 || ppmerr2[i] > 0.0) havecvr = 1;
    }

    if( fixed_gps_vertical ) get_enu_rf_xform( 0, 0, vd->reffrm, xform );

    /* Clear out the covariance matrix */

    for( iobs = 0; iobs < vd->nobs*3; iobs++ ) for( jobs = 0; jobs <= iobs; jobs++ )
        {
            Lij( cvr, iobs, jobs ) = 0.0;
        }

    /* Calculate the covariances of the supplied vectors and
       also store their component in the correlations */

    for( iobs = 0; iobs < vd->nobs; iobs++ )
    {

        /* Generate the covariance matrix */

        if( !fixed_gps_vertical ) get_enu_rf_xform( t[iobs].tgt.to, t[iobs].tgt.to, vd->reffrm, xform );
        calc_default_cvr( 0.0, tcvr, abserr2, abserr2 );
        transform_cvr( xform, tcvr );

        /* Set the diagonal elements */

        iobs3 = iobs*3;
        Lij(cvr, iobs3,  iobs3  ) = tcvr[0];
        Lij(cvr, iobs3,  iobs3+1) = tcvr[1];
        Lij(cvr, iobs3+1,iobs3+1) = tcvr[2];
        Lij(cvr, iobs3  ,iobs3+2) = tcvr[3];
        Lij(cvr, iobs3+1,iobs3+2) = tcvr[4];
        Lij(cvr, iobs3+2,iobs3+2) = tcvr[5];

        /* Now handle the off-diagonal elements */

        if( ! havecvr ) continue;
        for( jobs = 0; jobs < iobs; jobs ++ )
        {

            if( !fixed_gps_vertical ) get_enu_rf_xform( t[iobs].tgt.to, t[iobs].tgt.to, vd->reffrm, xform );
            vecdif(t[iobs].vector,t[jobs].vector, dif);
            calc_default_cvr( veclen(dif), tcvr, mmerr2, ppmerr2 );
            transform_cvr( xform, tcvr );

            /* Set the diagonal elements */

            jobs3 = jobs*3;
            Lij(cvr, iobs3,  jobs3  ) = tcvr[0];
            Lij(cvr, iobs3,  jobs3+1) = tcvr[1];
            Lij(cvr, iobs3,  jobs3+2) = tcvr[3];
            Lij(cvr, iobs3+1,jobs3  ) = tcvr[1];
            Lij(cvr, iobs3+1,jobs3+1) = tcvr[2];
            Lij(cvr, iobs3+1,jobs3+2) = tcvr[4];
            Lij(cvr, iobs3+2,jobs3  ) = tcvr[3];
            Lij(cvr, iobs3+2,jobs3+1) = tcvr[4];
            Lij(cvr, iobs3+2,jobs3+2) = tcvr[5];
        }
    }
}

static void diagonal_cvr_to_covariance( survdata *vd )
{
    int i, j, n;
    ltmat cvr;

    /* Clear out off diagonal elements and convert standard errors
       to variances */

    cvr = vd->cvr;
    n = vd->nobs * 3;
    for( i = 0; i<n; i++ )
    {
        for( j = 0; j<i; j++ ) Lij(cvr,i,j) = 0.0;
        Lij(cvr,i,i) *= Lij(cvr,i,i);
    }

}

static void topocentric_cvr_to_xyz( survdata *vd, bool correlation )
{

    double xform[3][3];
    double tcvr[6];
    int iobs, iobs1,iobs2,iobs3;
    ltmat cvr;
    vecdata *t;


    cvr = vd->cvr;
    t = vd->obs.vdata;

    /* Special case for a single vector */

    if( vd->nobs == 1 )
    {
        if( correlation )
        {
            cvr[1] *= cvr[0] * cvr[2];
            cvr[3] *= cvr[0] * cvr[5];
            cvr[4] *= cvr[2] * cvr[5];
        }
        else
        {
            cvr[1] = cvr[3] = cvr[4] = 0.0;
        }
        cvr[0] *= cvr[0];
        cvr[2] *= cvr[2];
        cvr[5] *= cvr[5];
        get_enu_rf_xform( vd->from, t->tgt.to, vd->reffrm, xform );
        transform_cvr( xform, cvr );
        return;
    }

    /* Initiallize */

    if( fixed_gps_vertical ) get_enu_rf_xform( 0, 0, vd->reffrm, xform );

    for( iobs = 0; iobs < vd->nobs; iobs++ )
    {

        /* Copy the diagonal elements to cvr */

        iobs1 = iobs*3;
        iobs2 = iobs1+1;
        iobs3 = iobs2+1;

        tcvr[1] = tcvr[3] = tcvr[4] = 0.0;
        if( correlation )
        {
            tcvr[1] = Lij(cvr,iobs1,iobs2)*Lij(cvr,iobs1,iobs1)*Lij(cvr,iobs2,iobs2);
            tcvr[3] = Lij(cvr,iobs1,iobs3)*Lij(cvr,iobs1,iobs1)*Lij(cvr,iobs3,iobs3);
            tcvr[4] = Lij(cvr,iobs2,iobs3)*Lij(cvr,iobs2,iobs2)*Lij(cvr,iobs3,iobs3);
        }
        tcvr[0] = Lij( cvr, iobs1, iobs1 )*Lij( cvr, iobs1, iobs1 );
        tcvr[2] = Lij( cvr, iobs2, iobs2 )*Lij( cvr, iobs2, iobs2 );
        tcvr[5] = Lij( cvr, iobs3, iobs3 )*Lij( cvr, iobs3, iobs3 );


        if( !fixed_gps_vertical )
            get_enu_rf_xform( vd->from, t[iobs].tgt.to, vd->reffrm, xform );

        transform_cvr( xform, tcvr );

        Lij( cvr, iobs1  , iobs1   ) = tcvr[0];
        Lij( cvr, iobs1  , iobs2 ) = tcvr[1];
        Lij( cvr, iobs2, iobs2 ) = tcvr[2];
        Lij( cvr, iobs1  , iobs3 ) = tcvr[3];
        Lij( cvr, iobs2, iobs3 ) = tcvr[4];
        Lij( cvr, iobs3, iobs3 ) = tcvr[5];
    }

}


static void correlation_cvr_to_covariance( survdata *vd )
{
    int i, j;
    int n;
    ltmat cvr;

    cvr = vd->cvr;
    n = vd->nobs*3;

    for( i=1; i<n; i++ ) for( j=0; j<i; j++ )
        {
            Lij(cvr,i,j) *= Lij(cvr,i,i) * Lij(cvr,j,j);
        }

    for( i=0; i<n; i++ ) Lij(cvr,i,i) *= Lij(cvr,i,i);
}



static void calc_gps_covariance( survdata *vd, int cvrtype, double *mmerr )
{

    switch( cvrtype )
    {

    case CVR_DEFAULT:
        if( datatype[vd->obs.vdata->tgt.type].ispoint )
            build_default_point_cvr( vd, mmerr );
        else
            build_default_baseline_cvr( vd, mmerr );
        break;

    case CVR_DIAGONAL:    diagonal_cvr_to_covariance( vd );
        break;

    case CVR_TOPOCENTRIC: topocentric_cvr_to_xyz( vd, false );
        break;

    case CVR_ENU_CORRELATION: topocentric_cvr_to_xyz( vd, true );
        break;

    case CVR_CORRELATION: correlation_cvr_to_covariance( vd );
        break;
    }
}


void init_gps_covariances( VRTFUNC vrt, XFMFUNC xfm )
{
    vertfunc = vrt;
    xfmfunc =  xfm;
    set_gpscvr_func( calc_gps_covariance );
}

VRTFUNC gps_vrtfunc( void )
{
    return vertfunc;
}

XFMFUNC gps_xfmfunc( void )
{
    return xfmfunc;
}


void transform_vec_to_topo( int from, int to, int rf,
                            double *vec, double *cvr )
{

    double xform[3][3];

    get_rf_enu_xform( from, to, rf, xform );
    if(vec) premult3( (double *) xform, vec, vec, 1 );
    if(cvr) transform_cvr( xform, cvr );
}


void transform_xyz_cvr_to_topocentric( survdata *vd, ltmat cvr )
{

    double xform[3][3];
    double tcvr[6];
    int iobs, iobs3;
    int jobs, jobs3, j;
    vecdata *t;

    t = vd->obs.vdata;

    /* Special case for a single vector */

    if( vd->nobs == 1 )
    {
        get_rf_enu_xform( vd->from, t->tgt.to, vd->reffrm, xform );
        transform_cvr( xform, cvr );
        return;
    }

    /* Initiallize */

    if( fixed_gps_vertical ) get_enu_rf_xform( 0, 0, vd->reffrm, xform );

    for( iobs = 0; iobs < vd->nobs; iobs++ )
    {

        /* Set up the transformation for the iobs'th observation */

        if( !fixed_gps_vertical )
            get_rf_enu_xform( vd->from, t[iobs].tgt.to, vd->reffrm, xform );

        /* Convert the iobs x iobs submatrix */

        iobs3 = iobs*3;
        tcvr[0] = Lij( cvr, iobs3  , iobs3   );
        tcvr[1] = Lij( cvr, iobs3  , iobs3+1 );
        tcvr[2] = Lij( cvr, iobs3+1, iobs3+1 );
        tcvr[3] = Lij( cvr, iobs3  , iobs3+2 );
        tcvr[4] = Lij( cvr, iobs3+1, iobs3+2 );
        tcvr[5] = Lij( cvr, iobs3+2, iobs3+2 );

        transform_cvr( xform, tcvr );

        Lij( cvr, iobs3  , iobs3   ) = tcvr[0];
        Lij( cvr, iobs3  , iobs3+1 ) = tcvr[1];
        Lij( cvr, iobs3+1, iobs3+1 ) = tcvr[2];
        Lij( cvr, iobs3  , iobs3+2 ) = tcvr[3];
        Lij( cvr, iobs3+1, iobs3+2 ) = tcvr[4];
        Lij( cvr, iobs3+2, iobs3+2 ) = tcvr[5];

        /* Now for the off-diagonal elements */

        for( jobs = 0; jobs < vd->nobs; jobs++ )
        {
            if( jobs == iobs ) continue;
            jobs3 = jobs * 3;
            for( j = jobs3; j < jobs3+3; j++ )
            {
                tcvr[0] = Lij( cvr, j, iobs3 );
                tcvr[1] = Lij( cvr, j, iobs3+1 );
                tcvr[2] = Lij( cvr, j, iobs3+2 );
                premult3( (double *) xform, tcvr, tcvr, 1 );
                Lij( cvr, j, iobs3   ) = tcvr[0];
                Lij( cvr, j, iobs3+1 ) = tcvr[1];
                Lij( cvr, j, iobs3+2 ) = tcvr[2];
            }
        }
    }

}




double vector_standardised_residual( double vec[3], ltmat cvr, int *rank )
{

    ltmat r1, r2, ri, rj;
    double sum, prod;
    int i,j,k;
    static double small = 1.0e-10;

    /* Cholesky decomposition */

    *rank = 3;
    for (i = 0, ri=cvr; i<3; i++, ri+=i )
    {
        for ( j=0, rj = cvr; j<= i; j++, rj+=j )
        {
            r1=ri; r2=rj; sum=0.0;
            for( k=0; k<j; k++ ) sum -= *r1++ * *r2++;
            sum += *r1;
            if ( i==j )
            {
                if( sum < small ) { (*rank)--; *r1 = 1.0; }
                else *r1 = sqrt( sum );
            }
            else
                *r1 = sum/ *r2;
        }
    }

    /* Form L"v, and its dot product with itself (this is the number we want) */

    prod = 0.0;

    for ( i=0, r1=cvr; i<3; i++ )
    {
        for (k = 0, sum=0.0; k<i; k++ ) sum -= *r1++ * vec[k];
        vec[i] = (vec[i]+sum) / *r1++;
        prod += vec[i] * vec[i];
    }

    /* Convert to standardised residual by dividing by its rank and
       square rooting */

    if( *rank )
    {
        prod /= *rank;
        if( prod > 0 ) prod = sqrt(prod); else prod = 0.0;
    }

    else
    {
        prod = 1.0;
    }

    return prod;
}


int calc_vecdata_vector( survdata *vd, int from, int to, int type,
                         double *vec, double *cvr )
{
    double lcvr[6];
    double *pvec, *pcvr;
    ltmat source;
    int vtype;
    int sfrom, sto;

    if( datatype[vd->obs.vdata[0].tgt.type].ispoint ) return calc_vecdata_point(vd,to,type,vec,cvr);

    if( vd->format != SD_VECDATA ) return INVALID_DATA;
    pvec = vec;
    pcvr = cvr;
    if( pcvr && type & VD_STDERR ) pcvr = lcvr;
    if( to < VD_REF_STN || to > vd->nobs ) return INVALID_DATA;
    if( from < VD_REF_STN || from > vd->nobs ) return INVALID_DATA;
    vtype = type & VD_TYPEMASK;
    switch( vtype )
    {
    case VD_OBSVEC:   source = vd->cvr; break;
    case VD_CALCVEC:  source = vd->calccvr; break;
    case VD_RESVEC:   source = vd->rescvr; break;
    default:          return INVALID_DATA;
    }

    /* Calculate the vector */

    if( pvec )
    {
        if( to == VD_REF_STN)
        {
            pvec[0] = pvec[1] = pvec[2] = 0.0;
        }
        else
        {
            vecdata *v;
            double *vs = 0;
            v = &vd->obs.vdata[to];
            switch( vtype )
            {
            case VD_OBSVEC:   vs  = v->vector; break;
            case VD_CALCVEC:  vs = v->calc; break;
            case VD_RESVEC:   vs = v->residual; break;
            }
            pvec[0] = vs[0]; pvec[1] = vs[1]; pvec[2] = vs[2];
        }
        if( from != VD_REF_STN )
        {
            vecdata *v;
            double *vs = 0;
            v = &vd->obs.vdata[from];
            switch( vtype )
            {
            case VD_OBSVEC:   vs  = v->vector; break;
            case VD_CALCVEC:  vs = v->calc; break;
            case VD_RESVEC:   vs = v->residual; break;
            }
            pvec[0] -= vs[0]; pvec[1] -= vs[1]; pvec[2] -= vs[2]; \
        }
    }

    /* Calculate the covariance matrix */

    if( pcvr )
    {
        int i3 = 0;
        if( to != VD_REF_STN )
        {
            i3 = to * 3;
            pcvr[0] = Lij(source,i3,  i3 );
            pcvr[1] = Lij(source,i3+1,i3);
            pcvr[2] = Lij(source,i3+1,i3+1);
            pcvr[3] = Lij(source,i3+2,i3);
            pcvr[4] = Lij(source,i3+2,i3+1);
            pcvr[5] = Lij(source,i3+2,i3+2);
        }
        else
        {
            pcvr[0] = pcvr[1] = pcvr[2] = pcvr[3] = pcvr[4] = pcvr[5] = 0.0;
        }
        if( from != VD_REF_STN )
        {
            int j3, ic, jc;
            double *pc;
            j3 = from * 3;
            pcvr[0] += Lij(source,j3,  j3 );
            pcvr[1] += Lij(source,j3+1,j3);
            pcvr[2] += Lij(source,j3+1,j3+1);
            pcvr[3] += Lij(source,j3+2,j3);
            pcvr[4] += Lij(source,j3+2,j3+1);
            pcvr[5] += Lij(source,j3+2,j3+2);

            if( to != VD_REF_STN ) for( pc = pcvr, ic = 0; ic < 3; ic++ )
                {
                    int i3c = i3 + ic;
                    int j3c = j3 + ic;
                    for( jc = 0; jc <= ic; jc++, pc++ )
                    {
                        *pc -= Lij(source, i3c, j3+jc) + Lij( source, j3c, i3+jc );
                    }
                }
        }
    }

    /* Convert to topocentric if required */

    sfrom = (from == VD_REF_STN) ? vd->from : vd->obs.vdata[from].tgt.to;
    sto =  (to == VD_REF_STN) ? vd->from : vd->obs.vdata[to].tgt.to;
    if( type & VD_TOPOCENTRIC )
        transform_vec_to_topo( sfrom, sto, vd->reffrm, pvec, pcvr);

    if( pcvr && type & VD_STDERR )
    {
        cvr[0] = (pcvr[0] > 0) ? sqrt(pcvr[0]) : 0.0;
        cvr[1] = (pcvr[2] > 0) ? sqrt(pcvr[2]) : 0.0;
        cvr[2] = (pcvr[5] > 0) ? sqrt(pcvr[5]) : 0.0;
    }

    return OK;
}



int calc_vecdata_point( survdata *vd, int to, int type,
                        double *vec, double *cvr )
{
    double lcvr[6];
    double *pvec, *pcvr;
    ltmat source;
    int vtype;
    int sto;

    if( vd->format != SD_VECDATA ) return INVALID_DATA;
    pvec = vec;
    pcvr = cvr;
    if( pcvr && type & VD_STDERR ) pcvr = lcvr;
    if( to < 0 || to >= vd->nobs ) return INVALID_DATA;
    vtype = type & VD_TYPEMASK;
    switch( vtype )
    {
    case VD_OBSVEC:   source = vd->cvr; break;
    case VD_CALCVEC:  source = vd->calccvr; break;
    case VD_RESVEC:   source = vd->rescvr; break;
    default:          return INVALID_DATA;
    }

    /* Calculate the vector */

    if( pvec )
    {
        vecdata *v;
        double *vs = 0;
        v = &vd->obs.vdata[to];
        switch( vtype )
        {
        case VD_OBSVEC:   vs  = v->vector; break;
        case VD_CALCVEC:  vs = v->calc; break;
        case VD_RESVEC:   vs = v->residual; break;
        }
        pvec[0] = vs[0]; pvec[1] = vs[1]; pvec[2] = vs[2];

    }

    /* Calculate the covariance matrix */

    if( pcvr )
    {
        int i3 = 0;
        i3 = to * 3;
        pcvr[0] = Lij(source,i3,  i3 );
        pcvr[1] = Lij(source,i3+1,i3);
        pcvr[2] = Lij(source,i3+1,i3+1);
        pcvr[3] = Lij(source,i3+2,i3);
        pcvr[4] = Lij(source,i3+2,i3+1);
        pcvr[5] = Lij(source,i3+2,i3+2);
    }

    /* Convert to topocentric if required */

    sto =  vd->obs.vdata[to].tgt.to;

    if( type & VD_TOPOCENTRIC )
        transform_vec_to_topo( sto, sto, vd->reffrm, pvec, pcvr);

    if( pcvr && type & VD_STDERR )
    {
        cvr[0] = (pcvr[0] > 0) ? sqrt(pcvr[0]) : 0.0;
        cvr[1] = (pcvr[2] > 0) ? sqrt(pcvr[2]) : 0.0;
        cvr[2] = (pcvr[5] > 0) ? sqrt(pcvr[5]) : 0.0;
    }

    return OK;
}

void gps_covar_apply_obs_error_factor( survdata *sd, int iobs, double factor )
{
    vecdata *vd=sd->obs.vdata+iobs;
    trgtdata *tgt=&(vd->tgt);
    int ncvr=sd->nobs*3;
    int i3=iobs*3;
    double factor2=factor*factor;
    if( ! sd->cvr ) return;
    for( int row=0; row<i3; row++ )
    {
        for( int col=i3; col < i3+3; col++ ) Lij(sd->cvr,row,col) *= factor;
    }
    for( int row=i3; row<i3+3; row++ )
    {
        for( int col=i3; col <= row; col++ ) Lij(sd->cvr,row,col) *= factor2;
    }
    for( int row=i3+3; row<ncvr; row++ )
    {
        for( int col=i3; col < i3+3; col++ ) Lij(sd->cvr,row,col) *= factor;
    }
    tgt->errfct *= factor;
}

void gps_covar_apply_obs_offset_error( survdata *sd, int iobs, double varhor, double varvrt )
{
    double xform[3][3];
    int iobs3 = iobs*3;
    double ocvr[6];
    vecdata *vd=sd->obs.vdata+iobs;
    ltmat cvr=sd->cvr;
    if( ! cvr ) return;
    get_enu_rf_xform( 0, vd->tgt.to, sd->reffrm, xform );
    ocvr[0]=varhor;
    ocvr[1]=0.0;
    ocvr[2]=varhor;
    ocvr[3]=0.0;
    ocvr[4]=0.0;
    ocvr[5]=varvrt;
    transform_cvr( xform, ocvr );
    Lij( cvr, iobs3  , iobs3   ) += ocvr[0];
    Lij( cvr, iobs3  , iobs3+1 ) += ocvr[1];
    Lij( cvr, iobs3+1, iobs3+1 ) += ocvr[2];
    Lij( cvr, iobs3  , iobs3+2 ) += ocvr[3];
    Lij( cvr, iobs3+1, iobs3+2 ) += ocvr[4];
    Lij( cvr, iobs3+2, iobs3+2 ) += ocvr[5];
}

void gps_covar_apply_set_error_factor( survdata *sd, double factor )
{
    int i3=sd->nobs*3;
    double factor2=factor*factor;
    int i;
    vecdata *vd;
    ltmat cvr=sd->cvr;
    if( ! cvr ) return;
    for( int row=0; row<i3; row++ )
    {
        for( int col=0; col <= row; col++ ) Lij(cvr,row,col) *= factor2;
    }
    for( i = 0, vd=sd->obs.vdata; i<sd->nobs; i++, vd++ )
    {
        vd->tgt.errfct *= factor;
    }
}

void gps_covar_apply_centroid_error( survdata *sd, double varhor, double varvrt )
{
    double xform[3][3];
    double ocvr[6];
    ltmat cvr=sd->cvr;
    if( ! cvr ) return;
    get_set_enu_rf_xform( sd, xform );
    ocvr[0]=varhor;
    ocvr[1]=0.0;
    ocvr[2]=varhor;
    ocvr[3]=0.0;
    ocvr[4]=0.0;
    ocvr[5]=varvrt;
    transform_cvr( xform, ocvr );
    for( int jobs=0; jobs < sd->nobs; jobs++ )
    {
        int jobs3=jobs*3;
        for( int iobs=0; iobs<jobs; iobs++ )
        {
            int iobs3=iobs*3;
            Lij( cvr, iobs3  , jobs3   ) += ocvr[0];
            Lij( cvr, iobs3  , jobs3+1 ) += ocvr[1];
            Lij( cvr, iobs3  , jobs3+2 ) += ocvr[3];
            Lij( cvr, iobs3+1, jobs3   ) += ocvr[1];
            Lij( cvr, iobs3+1, jobs3+1 ) += ocvr[2];
            Lij( cvr, iobs3+1, jobs3+2 ) += ocvr[4];
            Lij( cvr, iobs3+2, jobs3   ) += ocvr[3];
            Lij( cvr, iobs3+2, jobs3+1 ) += ocvr[4];
            Lij( cvr, iobs3+2, jobs3+2 ) += ocvr[5];
        }
        Lij( cvr, jobs3  , jobs3   ) += ocvr[0];
        Lij( cvr, jobs3  , jobs3+1 ) += ocvr[1];
        Lij( cvr, jobs3+1, jobs3+1 ) += ocvr[2];
        Lij( cvr, jobs3  , jobs3+2 ) += ocvr[3];
        Lij( cvr, jobs3+1, jobs3+2 ) += ocvr[4];
        Lij( cvr, jobs3+2, jobs3+2 ) += ocvr[5];
    }
}

void gps_covar_apply_basestation_offset_error( survdata *sd, double varhor, double varvrt )
{
    double xform[3][3];
    double ocvr[6];
    ltmat cvr=sd->cvr;
    if( ! cvr ) return;
    get_enu_rf_xform( 0, sd->from, sd->reffrm, xform );
    ocvr[0]=varhor;
    ocvr[1]=0.0;
    ocvr[2]=varhor;
    ocvr[3]=0.0;
    ocvr[4]=0.0;
    ocvr[5]=varvrt;
    transform_cvr( xform, ocvr );
    for( int jobs=0; jobs < sd->nobs; jobs++ )
    {
        int jobs3=jobs*3;
        for( int iobs=0; iobs<jobs; iobs++ )
        {
            int iobs3=iobs*3;
            Lij( cvr, iobs3  , jobs3   ) += ocvr[0];
            Lij( cvr, iobs3  , jobs3+1 ) += ocvr[1];
            Lij( cvr, iobs3  , jobs3+2 ) += ocvr[3];
            Lij( cvr, iobs3+1, jobs3   ) += ocvr[1];
            Lij( cvr, iobs3+1, jobs3+1 ) += ocvr[2];
            Lij( cvr, iobs3+1, jobs3+2 ) += ocvr[4];
            Lij( cvr, iobs3+2, jobs3   ) += ocvr[3];
            Lij( cvr, iobs3+2, jobs3+1 ) += ocvr[4];
            Lij( cvr, iobs3+2, jobs3+2 ) += ocvr[5];
        }
        Lij( cvr, jobs3  , jobs3   ) += ocvr[0];
        Lij( cvr, jobs3  , jobs3+1 ) += ocvr[1];
        Lij( cvr, jobs3+1, jobs3+1 ) += ocvr[2];
        Lij( cvr, jobs3  , jobs3+2 ) += ocvr[3];
        Lij( cvr, jobs3+1, jobs3+2 ) += ocvr[4];
        Lij( cvr, jobs3+2, jobs3+2 ) += ocvr[5];
    }
}
