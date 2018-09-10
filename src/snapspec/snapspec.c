#include "snapconfig.h"

/*
   $Log: snapspec.c,v $
   Revision 1.4  2004/03/10 21:18:54  ccrook
   Update version number after identifying error in specification testing.

   Revision 1.3  2004/03/05 02:33:38  ccrook
   Updated to fix incorrect calculation of horizontal scaling factors...

   Revision 1.2  2004/02/26 20:23:37  ccrook
   Updated to better handle corruption in binary file.

   Revision 1.1  2003/05/28 01:40:48  ccrook
   Added snapspec modules


*/

#define TEST_SMALL_CVR 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "util/snapctype.h"

#include "util/errdef.h"
#include "util/chkalloc.h"
#include "util/fileutil.h"
#include "util/linklist.h"
#include "util/bltmatrx.h"
#include "util/bltmatrx_mt.h"

#include "util/binfile.h"
#include "snapdata/survdata.h"
#include "snap/survfile.h"
#include "snap/bindata.h"
#include "snap/stnadj.h"
#include "snap/snapglob.h"
#include "snap/snapglob_bin.h"
#include "coordsys/coordsys.h"
#include "network/network.h"
#include "util/readcfg.h"
#include "util/dstring.h"
#include "util/probfunc.h"
#include "util/get_date.h"
#include "util/dms.h"
#include "util/pi.h"
#include "util/bltmatrx_mt.h"
#include "util/getversion.h"
#include "util/writecsv.h"

#include "dbl4_adc_sdc.h"
#include "dbl4_utl_error.h"


char spec_run_time[GETDATELEN];
static double test_confidence = 95.0;
static int test_apriori = 1;
static double varmult = 1.0;
static int min_order=0;
static SysCodeType dfltOrder;
static double vrtHorRatio = 1.0;

#define RELACC_BLOCK_SIZE 4196
#define MAX_ORDER 20

typedef struct
{
    double verr2;
    double emax2;
    /*   float verr2; */
} stn_relacc;

#define SRA_LOGLEVEL_OUTPUT 2048 /* Output log to stdout as well as file */

#define SRA_HVMODE_AUTO 0
#define SRA_HVMODE_VRT  1
#define SRA_HVMODE_HOR  2
#define SRA_HVMODE_3D   3

typedef struct
{
    FILE *logfile;
    hSDCTest hsdc;
    unsigned char outputlog;
    int nstn;
    int have_srcorders;
    short *src_orderid;
    short *role;
    short *order;
    short *priority;
    double *horvar;
    double *vrtvar;
    char *alloc;
    stn_relacc **cols;
    bltmatrix *bltdec;  /* Choleski decomposition of normal equns */
    bltmatrix *bltreq;  /* Requested non-zero rows */
    bltmatrix *blt;     /* Calculated covariance */
    int bltupdated;     /* True if covar has been calc'd (used for caching) */
    char testhor;
    char testvrt;
    int loglevel;
    int autominorder;
} stn_relacc_array;

typedef struct cfg_stack_s
{
    CFG_FILE *cfg;
    struct cfg_stack_s *next;
} cfg_stack;

cfg_stack *cfgs = NULL;

#define CACHE_COVARIANCE_EXT ".scc"
#define CACHE_COVARIANCE_SIG "SNAPSPEC_CACHED_COVARIANCE"
#define CACHE_COVARIANCE_SECTION "SNAPSPEC_CACHED_COVARIANCE"

#define SNAPSPEC_CSV_EXT "-snapspec.csv"
#define SNAPSPEC_CRD_EXT "-snapspec.crd"

/*============================================================*/


static double calc_error_ellipse_semimajor2( double cvr[] )
{
    double v1, v2, v3, v4;

    v1 = (cvr[2]+cvr[0])/2.0;
    v2 = (cvr[2]-cvr[0])/2.0;
    v3 = cvr[1];
    v4 = v2*v2+v3*v3;
    if( v4 > 0.0 )  { v4 = sqrt(v4); v1 += v4; }
    return v1;
}

/*============================================================*/

static double relacc_get_covar( stn_relacc_array *ra, int istn, int jstn );

static stn_relacc_array *create_relacc()
{
    stn_relacc_array *ra;
    short *src_orderid;
    short *role;
    short *order;
    short *priority;
    double *horvar;
    double *vrtvar;
    int i;

    int haveorders = network_order_count(net) > 0;
    int nstns = number_of_stations( net );
    ra = (stn_relacc_array *) check_malloc( sizeof(stn_relacc_array) );
    src_orderid = (short *) check_malloc( nstns * sizeof(short));
    role = (short *) check_malloc( nstns * sizeof(short));
    order = (short *) check_malloc( nstns * sizeof(short));
    priority = (short *) check_malloc( nstns * sizeof(short));
    horvar = (double *) check_malloc( nstns * sizeof(double));
    vrtvar = (double *) check_malloc( nstns * sizeof(double));
    for( i = 0; i < nstns; i++ )
    {
        src_orderid[i] = 0;
        if( haveorders )
        {
            station *st=stnptr(i+1);
            src_orderid[i] = (short) network_station_order(net,st);
        }
        role[i] = 0;
        order[i] = 0;
        priority[i] = SDC_NO_PRIORITY;
        horvar[i] = 0.0;
        vrtvar[i] = 0.0;
    }

    ra->nstn = nstns;
    ra->hsdc = NULL;
    ra->alloc = NULL;
    ra->cols = NULL;
    ra->have_srcorders = haveorders;
    ra->src_orderid = src_orderid;
    ra->role = role;
    ra->order = order;
    ra->priority = priority;
    ra->horvar = horvar;
    ra->vrtvar = vrtvar;
    ra->logfile = NULL;
    ra->loglevel = 0;
    ra->bltdec = NULL;
    ra->bltreq = NULL;
    ra->blt = NULL;
    ra->bltupdated = 0;
    ra->outputlog = 0;
    ra->autominorder = 0;
    return ra;
}

static void delete_relacc( stn_relacc_array *ra )
{
    int i;
    ra->nstn = 0;
    check_free( ra->role );
    ra->role = NULL;
    check_free( ra->order );
    ra->order = NULL;
    check_free( ra->priority );
    ra->priority = NULL;
    check_free( ra->horvar );
    ra->horvar = NULL;
    check_free( ra->vrtvar );
    ra->vrtvar = NULL;
    if( ra->cols )
    {
        for( i = 0; i < ra->nstn; i++ )
        {
            if( ra->alloc[i] ) { check_free( ra->cols[i] ); }
        }
        check_free( ra->cols );
        ra->cols = NULL;
        check_free( ra->alloc );
        ra->alloc = NULL;
    }
    if( ra->bltdec ) delete_bltmatrix( ra->bltdec );
    ra->bltdec = NULL;
    if( ra->bltreq ) delete_bltmatrix( ra->bltreq );
    ra->bltreq = NULL;
    if( ra->blt ) delete_bltmatrix( ra->blt );
    ra->blt = NULL;
    check_free( ra );
}

static void relacc_alloc_cache( stn_relacc_array *ra )
{
    int istn;
    int istn0;
    int i;
    char *alloc;
    stn_relacc **cols;
    int nstns = ra->nstn;

    if( ra->cols ) return;

    ra->alloc = alloc = (char *) check_malloc( nstns * sizeof(char));
    ra->cols = cols = (stn_relacc **) check_malloc( nstns * sizeof(stn_relacc *) );

    istn = 1;
    istn0 =  1;
    while( istn <= nstns )
    {
        stn_relacc *r;
        int nblk = 0;
        do
        {
            nblk += istn;
            istn++;
        }
        while ( istn <= nstns && nblk < RELACC_BLOCK_SIZE );

        r = (stn_relacc *) check_malloc( nblk * sizeof( stn_relacc ));
        for( i = 0; i < nblk; i++ )
        {
            r[i].emax2 = SDC_COVAR_UNAVAILABLE;
            r[i].verr2 = SDC_COVAR_UNAVAILABLE;
        }
        for( nblk = 0; istn0 < istn; nblk += istn0, istn0++ )
        {
            alloc[istn0-1] = nblk == 0 ? 1 : 0;
            cols[istn0-1] = r + nblk;
        }
    }
}

static stn_relacc *get_relacc( stn_relacc_array *ra, int istn, int jstn )
{
    if( ra->cols == NULL || istn < 1 || jstn < 1 || istn > ra->nstn || jstn > ra->nstn ) return NULL;
    if( istn < jstn ) { int itmp = istn; istn = jstn; jstn = itmp; }
    return ra->cols[istn-1] + jstn-1;
}

static void add_relacc( stn_relacc_array *ra, int istn, int jstn, double emax2, double verr2 )
{
    if( istn == jstn )
    {
        ra->horvar[istn-1] = emax2;
        ra->vrtvar[istn-1] = verr2;
        return;
    }
    else
    {
        stn_relacc *r = get_relacc( ra, istn, jstn );
        if( r )
        {
            r->emax2 = emax2;
            r->verr2 = verr2;
        }
    }
}

static int relacc_create_blt_req( stn_relacc_array *ra )
{
    if( ! ra->bltreq )
    {
        ra->bltreq = create_bltmatrix( ra->bltdec->nrow ); 
        if( ra->blt )
        {
            copy_bltmatrix_bandwidth( ra->blt, ra->bltreq );
        }
    }
    return 1;
}

static int relacc_calc_requested_covar( stn_relacc_array *ra )
{
    bltmatrix *bltdec = ra->bltdec;
    bltmatrix *blt = ra->bltreq;
    if( ! blt || ! bltdec ) return 0;

    if( ra->blt ) 
    {
        delete_bltmatrix( ra->blt ); 
        ra->blt=NULL;
    }

    if( ra->loglevel & SDC_LOG_STEPS )
    {
        fprintf(ra->logfile,"   Calculating covariances from decomposition\n");
        // long nrow=blt_nrows( blt );
        // long nelement=blt_requested_size( blt );
        // double pcntfull=100.0*((double) nelement)/((((double) nrow) * ((double) nrow)+1)/2.0);
        // fprintf(ra->logfile,"   Matrix size %d rows %ld elements %.2lf%% full\n",
        //         nrow, nelement, pcntfull );
    }
    copy_bltmatrix( bltdec, blt );
    blt_chol_inv_mt( blt );

    ra->blt=blt;
    ra->bltupdated=1;
    ra->bltreq=NULL;

    return 1;
}

static void relacc_record_missing_covar( stn_relacc_array *ra, int istn, int jstn )
{
    stn_adjustment *isa;
    stn_adjustment *jsa;

    isa = stnadj(stnptr(istn));
    jsa = stnadj(stnptr(jstn));

    /* If the matrix for requests is not created yet, then create it */

    if( ! ra->bltreq ) { relacc_create_blt_req( ra ); }

    int irow = isa->hrowno-1;
    int jrow = jsa->hrowno-1;
    int minrow = irow < jrow ? irow : jrow;
    if( irow < 0 || jrow < 0 ) return;

    /* Record the elements required in the covariance matrix */

    blt_nonzero_element( ra->bltreq, irow, minrow );
    blt_nonzero_element( ra->bltreq, irow+1, minrow );
    blt_nonzero_element( ra->bltreq, jrow, minrow );
    blt_nonzero_element( ra->bltreq, jrow+1, minrow );
}

static double relacc_calc_missing_covar( stn_relacc_array *ra, int istn, int jstn )
{
    bltmatrix *blt;
    stn_adjustment *isa;
    stn_adjustment *jsa;
    double cvr[3];
    int irx, iry, jrx, jry;
    int i;

    blt = ra->blt;
    isa = stnadj(stnptr(istn));
    jsa = stnadj(stnptr(jstn));

    /* Note: assume that we only get here if both stations are having
       horizontal coordinates calculated */

    if( ! isa->hrowno || ! jsa->hrowno )
    {
        return SDC_COVAR_UNAVAILABLE;
    }

    irx = iry = isa->hrowno-1;
    jrx = jry = jsa->hrowno-1;

    for( i = 0; i < 3; i++ )
    {
        if( i == 1 ) { iry++; jry++; }
        else if( i == 2 ) { irx++; jrx++; }

        cvr[i] = BLT(blt,irx,iry) - BLT(blt,irx,jry) - BLT(blt,jrx,iry) + BLT(blt,jrx,jry);
    }

    return calc_error_ellipse_semimajor2(cvr);
}

static double relacc_try_calc_missing_covar( stn_relacc_array *ra, int istn, int jstn )
{
    stn_adjustment *isa = stnadj(stnptr(istn));
    stn_adjustment *jsa = stnadj(stnptr(jstn));

    int irow = isa->hrowno-1;
    int jrow = jsa->hrowno-1;
    int minrow = irow < jrow ? irow : jrow;

    /* Check the elements required in the covariance matrix */

    if (blt_is_nonzero_element( ra->blt, irow, minrow ) &&
            blt_is_nonzero_element( ra->blt, irow+1, minrow ) &&
            blt_is_nonzero_element( ra->blt, jrow, minrow ) &&
            blt_is_nonzero_element( ra->blt, jrow+1, minrow ) )
    {
        return relacc_calc_missing_covar( ra, istn, jstn );
    }

    return SDC_COVAR_UNAVAILABLE;
}

static double relacc_get_covar( stn_relacc_array *ra, int istn, int jstn )
{

    if( istn == jstn )
    {
        return ra->horvar[istn-1];
    }
    else
    {
        stn_relacc *rac = get_relacc( ra, istn, jstn );
        double emax2;

        if( rac )
        {
            emax2 = rac->emax2;
        }

        else
        {
            stn_adjustment *isa = stnadj(stnptr(istn));
            stn_adjustment *jsa = stnadj(stnptr(jstn));

            /* If one or other station is not adjusted horizontally, then just need to use
               the variance of the other  */

            if( ! isa->hrowno  )
            {
                emax2 = ra->horvar[jstn-1];
            }
            else if( ! jsa->hrowno )
            {
                emax2 = ra->horvar[istn-1];
            }

            /* Else if we've already calculated a covariance matrix,
               then try using it */

            else if( ra->blt )
            {
                emax2 = relacc_try_calc_missing_covar( ra, istn, jstn );
            }
            else
            {
                emax2 = SDC_COVAR_UNAVAILABLE;
            }
        }
        return emax2;
    }
}

static void relacc_record_missing_verr( stn_relacc_array *ra, int istn, int jstn )
{
    stn_adjustment *isa;
    stn_adjustment *jsa;

    isa = stnadj(stnptr(istn));
    jsa = stnadj(stnptr(jstn));

    int irow = isa->vrowno-1;
    int jrow = jsa->vrowno-1;
    if( irow < 0 || jrow < 0 ) return;

    /* If the matrix for requests is not created yet, then create it */

    if( ! ra->bltreq ) { relacc_create_blt_req( ra ); }

    blt_nonzero_element( ra->bltreq, irow, jrow );
}

static double relacc_calc_missing_verr( stn_relacc_array *ra, int istn, int jstn )
{
    bltmatrix *blt;
    stn_adjustment *isa;
    stn_adjustment *jsa;
    int irow, jrow;

    blt = ra->blt;
    isa = stnadj(stnptr(istn));
    jsa = stnadj(stnptr(jstn));

    /* Note: assume that we only get here if both stations are having
       vertical coordinates calculated */

    if( ! isa->vrowno || ! jsa->vrowno )
    {
        return SDC_COVAR_UNAVAILABLE;
    }

    irow = isa->vrowno-1;
    jrow = jsa->vrowno-1;

    return BLT(blt,irow,irow) - BLT(blt,irow,jrow) - BLT(blt,jrow,irow) + BLT(blt,jrow,jrow);
}

static double relacc_try_calc_missing_verr( stn_relacc_array *ra, int istn, int jstn )
{
    stn_adjustment *isa = stnadj(stnptr(istn));
    stn_adjustment *jsa = stnadj(stnptr(jstn));

    int irow = isa->vrowno-1;
    int jrow = jsa->vrowno-1;

    /* Check the elements required in the covariance matrix */

    if (blt_is_nonzero_element( ra->blt, irow, jrow ))
    {
        return relacc_calc_missing_verr( ra, istn, jstn );
    }

    return SDC_COVAR_UNAVAILABLE;
}

static double relacc_get_verr( stn_relacc_array *ra, int istn, int jstn )
{

    if( istn == jstn )
    {
        return ra->vrtvar[istn-1];
    }
    else
    {
        double verr;

        stn_relacc *rac = get_relacc( ra, istn, jstn );
        if( rac )
        {
            verr = rac->verr2;
        }
        else
        {
            /* If in second pass, then covariance should be available */

            stn_adjustment *isa = stnadj(stnptr(istn));
            stn_adjustment *jsa = stnadj(stnptr(jstn));

            /* If one or other station is not adjusted horizontally, then just need to use
               the variance of the other  */

            if( ! isa->vrowno  )
            {
                verr = ra->vrtvar[jstn-1];
            }
            else if( ! jsa->vrowno )
            {
                verr = ra->vrtvar[istn-1];
            }

            /* Else if we've already calculated a covariance matrix,
               then try using it */

            else if( ra->blt )
            {
                verr = relacc_try_calc_missing_verr( ra, istn, jstn );
            }
            else
            {
                verr = SDC_COVAR_UNAVAILABLE;
            }
        }
        return verr;
    }
}

static int check_relacc_complete( stn_relacc_array *ra )
{
    int istn;
    int jstn;

    if( ! ra->cols ) return MISSING_DATA;

    for( istn = 2; istn <= ra->nstn; istn++ )
    {
        for( jstn = 1; jstn < istn; jstn++ )
        {
            stn_relacc *r = get_relacc( ra, istn, jstn );
            if( r->emax2 == SDC_COVAR_UNAVAILABLE ) return MISSING_DATA;
        }
    }
    return OK;
}

/*======================================================================*/

#if 0
/* Some of the code from SNAP for strict calculation of relative covariances */

void set_station_obseq( station *st, vector3 dst, void *hA, int irow, double date )
{
    stn_adjustment *sa;
    double denu[3];

    sa = stnadj( st );

    if( got_deformation && calc_deformation( st, date, denu ) == OK )
    {
        oe_add_value( hA, irow, -(denu[0]*dst[0] + denu[1]*dst[1] + denu[2]*dst[2]) );
    }

    if( sa->hrowno )
    {
        oe_param( hA, irow, sa->hrowno, dst[0] );
        oe_param( hA, irow, sa->hrowno+1, dst[1] );
    }
    if( sa->vrowno ) oe_param( hA, irow, sa->vrowno, dst[2] );
}

static void calc_relative_obseq( station *st1, station *st2, void *hA )
{
    vector3 dif;
    vector3 dst1[3], dst2[3];
    int axis;
    calc_vec_dif( st1, 0.0, st2, 0.0, dif, dst1, dst2 );
    init_oe( hA, 3, 2*dimension, OE_DIAGONAL_CVR );
    for( axis = 0; axis < 3; axis++ )
    {
        int irow = axis+1;
        set_station_obseq( st1, dst1[axis], hA, irow, UNDEFINED_DATE );
        set_station_obseq( st2, dst2[axis], hA, irow, UNDEFINED_DATE );
        oe_value( hA, irow+axis, dif[axis] );
        oe_covar( hA, irow, irow, 1.0 );
    }
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

void transform_vec_to_topo( int from, int to, int rf,
                            double *vec, double *cvr )
{

    double xform[3][3];

    get_rf_enu_xform( from, to, rf, xform );
    if(vec) premult3( (double *) xform, vec, vec, 1 );
    if(cvr) transform_cvr( xform, cvr );
}

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

static void calc_covar( int istn1, int istn2, double *relcvr )
{
    /* Calculate the vector covariance (XYZ system) */
    double vec[3];
    calc_relative_obseq( stnptr(istn1), stnptr(istn2), hA );
    lsq_calc_obs( hA, vec, NULL, NULL, NULL, 0, relcvr, NULL );
    /* Convert to topocentric */
    transform_vec_to_topo( istn1, istn2, 0, vec, relcvr );
}

#endif

/*======================================================================*/



static int reload_covariances( BINARY_FILE *b, stn_relacc_array *ra )
{
    int istn;
    double cvr[6];
    double emax2;
    int nstns;

    if( find_section( b, "STATION_COVARIANCES" ) != OK ) return MISSING_DATA;

    nstns = number_of_stations( net );

    for( istn = 0; istn++ < nstns; )
    {
        fread( cvr, sizeof(cvr), 1, b->f );
        emax2 = calc_error_ellipse_semimajor2( cvr );
        add_relacc( ra, istn, istn, emax2, cvr[5] );
    }

    return check_end_section( b );
}


static int reload_relative_covariances( BINARY_FILE *b, stn_relacc_array *ra )
{
    int from, to;
    double cvr[6];
    double emax2;

    if( find_section( b, "STATION_RELATIVE_COVARIANCES" ) != OK ) return MISSING_DATA;

    relacc_alloc_cache(ra);

    for(;;)
    {
        if( fread( &from, sizeof(from), 1, b->f ) != 1 ) break;
        if( from < 0 ) break;
        fread( &to, sizeof(to), 1, b->f );
        fread( cvr, sizeof(cvr), 1, b->f );

        emax2 = calc_error_ellipse_semimajor2( cvr );
        add_relacc( ra, from, to, emax2, cvr[5] );
    }

    return check_end_section( b );
}

static int reload_choleski_decomposition( BINARY_FILE *b, stn_relacc_array *ra )
{
    bltmatrix *blt = NULL;
    int sts;

    if( find_section(b, "CHOLESKI_DECOMPOSITION") != OK ) return MISSING_DATA;

    sts = reload_bltmatrix( &blt, b->f );
    if( sts != OK ) return sts;

    ra->bltdec = blt;
    return OK;
}

static char *cache_covariance_filename( char *bfn )
{
    int flen=path_len(bfn,1);
    char *cfn=(char *) check_malloc(flen+strlen(CACHE_COVARIANCE_EXT)+1);
    strncpy(cfn,bfn,flen);
    strcpy(cfn+flen,CACHE_COVARIANCE_EXT);
    return cfn;
}

static int try_reload_cached_covariance( stn_relacc_array *ra, char *cfn )
{
    BINARY_FILE *c;
    char cruntime[GETDATELEN];
    c=open_binary_file(cfn,CACHE_COVARIANCE_SIG);
    if( ! c ) return 0;
    if( find_section(c,CACHE_COVARIANCE_SECTION) != OK )
    {
        close_binary_file(c);
        return 0;
    }
    fread( cruntime, GETDATELEN, 1, c->f );
    if( _strnicmp(cruntime,run_time,GETDATELEN) != 0 )
    {
        printf("Covariance cache file out of date - deleting %s\n",cfn);
        close_binary_file(c);
        _unlink(cfn);
        return 0;
    }

    if( ra->blt )
    {
        delete_bltmatrix(ra->blt);
        ra->blt=0;
    }
    if( reload_bltmatrix( &(ra->blt), c->f ) !=  OK )
    {
        return 0;
    }
    close_binary_file(c);
    ra->bltupdated=0;
    printf("Using covariance from cache file %s\n",cfn);
    return 1;
}

static void cache_covariance_matrix( stn_relacc_array *ra, char *cfn )
{
    if( ! ra->blt ) return;
    if( ! ra->bltupdated ) return;
    BINARY_FILE *c;
    c=create_binary_file(cfn,CACHE_COVARIANCE_SIG);
    if( ! c ) return;
    create_section(c,CACHE_COVARIANCE_SECTION);
    fwrite(run_time,GETDATELEN,1,c->f);
    dump_bltmatrix(ra->blt,c->f);
    end_section(c);
    close_binary_file(c);
    printf("Created covariance cache file %s\n",cfn);
}

/*======================================================================*/
/* Functions used by SDC module                                         */

// #pragma warning ( disable : 4100 )

static long f_station_id( void *, int stn )
{
    return stn+1;
}

static int f_station_role ( void *env, int stn )
{
    station *st;
    stn_adjustment *sa;
    int role;
    int istn = f_station_id( env, stn );
    stn_relacc_array *ra = (stn_relacc_array *) env;
    role = ra->role[stn];
    if( role == SDC_IGNORE_MARK || role == SDC_CONTROL_MARK ) return role;

    st = stnptr(istn);
    sa = stnadj( st );

    if( ignored_station(istn) || rejected_station(istn) ) 
    {
        role = SDC_IGNORE_MARK; 
    }
    else if( (sa->hrowno && ra->testhor) || (sa->vrowno && ra->testvrt) )
    {
        role = ra->role[stn];
    }
    else
    {
        role = SDC_CONTROL_MARK;
    }
    ra->role[stn]=role;
    return role;
}

static int f_station_priority ( void *env, int stn )
{
    stn_relacc_array *ra = (stn_relacc_array *) env;
    return ra->priority[stn];
}

static double f_distance2( void *env, int stn1, int stn2 )
{
    double dist2 = -1.0;
    int istn1, istn2;
    if( stn1 == stn2 ) return 0.0;
    istn1 = f_station_id( env, stn1 );
    istn2 = f_station_id( env, stn2 );
    if( dist2 < 0 )
    {
        station *stf;
        station *stt;
        stf = station_ptr( net, istn1 );
        stt = station_ptr( net, istn2 );
        if( stf && stt )
        {
            dist2 = calc_distance( stf, 0.0, stt, 0.0, NULL, NULL );
            dist2 *= dist2;
        }
        else
        {
            /* Should really be assertion error. */
            dist2 = 0.0;
        }
    }
    return dist2;
}

static double f_error2( void *env, int stn1, int stn2 )
{
    int istn1, istn2;
    stn_relacc_array *ra = (stn_relacc_array *) env;
    double emax2 = 0.0;

    istn1 = f_station_id( env, stn1 );
    istn2 = f_station_id( env, stn2 );
    emax2 = relacc_get_covar( ra, istn1, istn2 );
    if( emax2 > 0.0 ) emax2 *= varmult;
    return emax2;
}

static double f_vrterror2( void *env, int stn1, int stn2 )
{
    int istn1, istn2;
    stn_relacc_array *ra = (stn_relacc_array *) env;
    double verr = 0.0;

    istn1 = f_station_id( env, stn1 );
    istn2 = f_station_id( env, stn2 );
    verr = relacc_get_verr( ra, istn1, istn2 );
    if( verr > 0.0 ) verr *= varmult;
    return verr;
}

static void f_request_covar( void *env, int stn1, int stn2 )
{
    int istn1, istn2;
    stn_relacc_array *ra = (stn_relacc_array *) env;
    istn1 = f_station_id( env, stn1 );
    istn2 = f_station_id( env, stn2 );
    relacc_record_missing_covar( ra, istn1, istn2 );
    relacc_record_missing_verr( ra, istn1, istn2 );
}

static int f_calc_requested( void *env )
{
    stn_relacc_array *ra = (stn_relacc_array *) env;
    return relacc_calc_requested_covar( ra );
}

static void f_set_order( void *env, int stn, int order )
{
    stn_relacc_array *ra = (stn_relacc_array *) env;
    ra->order[stn] = (short) (order+1);
}

static void f_write_log( void *env, const char *text )
{
    stn_relacc_array *ra = (stn_relacc_array *) env;
    if( ra->logfile ) { fputs( text, ra->logfile ); }
    if( ra->outputlog ) { puts( text ); }
}

static hSDCTest create_test( int maxorder )
{
    hSDCTest hsdc;

    hsdc = sdcCreateSDCTest( maxorder );
    hsdc->pfStationId = f_station_id;
    hsdc->pfStationRole = f_station_role;
    hsdc->pfStationPriority = f_station_priority;
    hsdc->pfDistance2 = f_distance2;
    hsdc->pfRequestCovar = f_request_covar;
    hsdc->pfCalcRequested = f_calc_requested;
    hsdc->pfError2 = f_error2;
    hsdc->pfVrtError2 = f_vrterror2;
    hsdc->pfSetOrder = f_set_order;
    hsdc->pfWriteLog = f_write_log;

    return hsdc;
}

static void delete_test( hSDCTest hsdc )
{
    sdcDropSDCTest( hsdc );
}

static int run_tests( hSDCTest hsdc )
{
    return sdcCalcSDCOrders2( hsdc, min_order );
}

static void write_results( hSDCTest hsdc, stn_relacc_array *ra )
{
    FILE *out = ra->logfile;
    int nstns = ra->nstn;
    int order;
    char header[80];

    if( ! out ) return;

    fprintf(out,"Results of order calculations\n");

    for( order = -2; order <= hsdc->norder; order++ )
    {
        int headed = 0;
        if( order == -2 )
        {
            sprintf(header,"Control stations");
        }
        else if (order == -1 )
        {
            sprintf(header,"Rejected stations");
        }
        else if (order == 0 )
        {
            sprintf(header,"Stations assigned order %s", dfltOrder);
        }
        else if (order > 0 )
        {
            sprintf(header,"Stations achieving order %s",hsdc->tests[order-1].scOrder);
        }
        for( int istn = 1; istn <= nstns; istn++ )
        {
            if( ra->order[istn-1] != order ) continue;
            if( ! headed )
            {
                fprintf(out,"\n%s\n",header);
                headed = 1;
            }
            fprintf(out,"%s\n",stnptr(istn)->Code);
        }
    }
}


static void write_station_index( hSDCTest, stn_relacc_array *ra )
{
    FILE *out = ra->logfile;
    int i;

    fprintf(out, "\nStation lookup for SDC test module\n");
    fprintf(out,"    id %-*s role prty\n",stn_name_width,"code");
    for( i = 0; i < ra->nstn; i++ )
    {
        int ist0 = i+1;
        fprintf(out,"  %4d %-*s %4d %4d\n",
                ist0,stn_name_width,stnptr(ist0)->Code,
                f_station_role(ra,i), f_station_priority(ra,i));
    }
    fprintf(out,"\n");
}


static const char *relacc_order_string( stn_relacc_array *ra, short order )
{
    const char *control="C";
    const char *ignored="I";

    if( order == SDC_CONTROL_MARK ) return control;
    if( order == SDC_IGNORE_MARK ) return ignored;
    if( order == 0 ) return dfltOrder;
    return ra->hsdc->tests[order-1].scOrder;
}

static char *output_filename( const char *filename, const char *basename, const char *ext )
{
    if( filename && _stricmp(filename,"-") != 0 )
    {
        return copy_string(filename);
    }
    int flen=path_len(basename,1);
    char *ofilename=(char *) check_malloc(flen+strlen(ext)+1);
    strncpy(ofilename,basename,flen);
    strcpy(ofilename+flen,ext);
    return ofilename;
}

static void write_output_csv( char *csvname, stn_relacc_array *ra )
{
    output_csv *csv=open_output_csv(csvname,0);
    if( ! csv )
    {
        printf("\nCannot open results csv file %s\n",csvname);
        return;
    } 

    int haveorders = ra->have_srcorders;
    int geocentric_coords = is_geocentric( net->crdsys ) ? 1 : 0;
    int projection_coords = is_projection( net->crdsys ) ? 1 : 0;
    int ellipsoidal = net->options & NW_ELLIPSOIDAL_HEIGHTS;
    projection *prj = net->crdsys->prj;
    ellipsoid *elp = net->crdsys->rf->el;

    write_csv_header( csv, "code" );
    write_csv_header( csv, "crdsys" );
    if( geocentric_coords )
    {
        /* Set ellipsoidal as true so that ellipsoidal height is calced */
        ellipsoidal=1;
        write_csv_header( csv,"X");
        write_csv_header( csv,"Y");
        write_csv_header( csv,"Z");
    }
    else
    {
        if( projection_coords )
        {
            write_csv_header( csv,"easting");
            write_csv_header( csv,"northing");
        }
        else
        {
            write_csv_header( csv,"longitude");
            write_csv_header( csv,"latitude");
        }
        write_csv_header( csv, ellipsoidal ? "ellheight" : "height" );
    }
    if( haveorders ) write_csv_header(csv,"src_order");
    write_csv_header(csv,"mode");
    write_csv_header( csv, "adj_e" );
    write_csv_header( csv, "adj_n" );
    write_csv_header( csv, "adj_h" );
    write_csv_header( csv, "errhor" );
    write_csv_header( csv, "errhgt" );
    write_csv_header( csv, "limit_order" );
    write_csv_header( csv, "priority" );
    write_csv_header( csv, "calc_order" );
    end_output_csv_record(csv);

    station *st;
    for( reset_station_list(net,0); NULL != (st = next_station(net)); )
    {
        int idra=st->id-1;
        stn_adjustment *sa = stnadj(st);
        if( sa->flag.ignored ) continue;
        double height = st->OHgt;
        if( ellipsoidal ) height += st->GUnd;

        int adjusted = sa->hrowno || sa->vrowno;

        write_csv_string( csv, st->Code );
        write_csv_string(csv,net->crdsys->code);

        if( geocentric_coords )
        {
            double llh[3], xyz[3];
            llh[CRD_LON]=st->ELon;
            llh[CRD_LAT]=st->ELat;
            llh[CRD_HGT]=height;
            llh_to_xyz( elp, llh, xyz, 0, 0);
            write_csv_double( csv, xyz[CRD_X], coord_precision );
            write_csv_double( csv, xyz[CRD_Y], coord_precision );
            write_csv_double( csv, xyz[CRD_Z], coord_precision );
        }
        else
        {
            if( projection_coords )
            {
                double easting, northing;
                geog_to_proj( prj, st->ELon, st->ELat, &easting, &northing );
                write_csv_double( csv, easting, coord_precision );
                write_csv_double( csv, northing, coord_precision );
            }
            else
            {
                write_csv_double( csv, st->ELon*RTOD, coord_precision+5 );
                write_csv_double( csv, st->ELat*RTOD, coord_precision+5 );
            }

            write_csv_double( csv, height, coord_precision );
        }

        if( haveorders )
        {
            write_csv_string( csv, network_order(net,ra->src_orderid[idra]) );
        }

        if( adjusted )
        {
            double de, dn, dh;
            char mode[3] = { '-', '-', 0 };
            if( sa->flag.float_h ) mode[0] = 'h';
            else if( sa->flag.adj_h ) mode[0] = 'H';
            if( sa->flag.float_v ) mode[1] = 'v';
            else if( sa->flag.adj_v ) mode[1] = 'V';

            dn = ( st->ELat - sa->initELat ) * st->dNdLt;
            de = ( st->ELon - sa->initELon ) * st->dEdLn;
            dh = st->OHgt - sa->initOHgt;

            write_csv_string(csv,mode);
            write_csv_double( csv, de, coord_precision );
            write_csv_double( csv, dn, coord_precision );
            write_csv_double( csv, dh, coord_precision );

            if( ra->horvar[idra] >= 0.0 )
            {
                write_csv_double( csv, sqrt(ra->horvar[idra]), coord_precision );
            }
            else
            {
                write_csv_null_field( csv );
            }

            if( ra->vrtvar[idra] >= 0.0 )
            {
                write_csv_double( csv, sqrt(ra->vrtvar[idra]), coord_precision );
            }
            else
            {
                write_csv_null_field( csv );
            }

        }
        else
        {
            write_csv_null_field( csv );
            write_csv_null_field( csv );
            write_csv_null_field( csv );
            write_csv_null_field( csv );
            write_csv_null_field( csv );
            write_csv_null_field( csv );
        }
        
        write_csv_string( csv, relacc_order_string( ra, ra->role[st->id-1]) );
        if( ra->priority[st->id-1] == SDC_NO_PRIORITY )
        {
            write_csv_null_field(csv);
        }
        else
        {
            write_csv_int(csv,(int) ra->priority[st->id-1]);
        }
        write_csv_string( csv, relacc_order_string( ra, ra->order[st->id-1]) );

        end_output_csv_record(csv);
    }
    close_output_csv( csv );
}


static int station_order;

static int stations_of_order( station *st )
{
    stn_adjustment *sa = stnadj( st );
    return sa->obscount == station_order;
}

static void write_coord_files( hSDCTest hsdc, stn_relacc_array *ra, char *fname )
{
    int i;
    int iorder;
    char *crdfilebuf;
    char comment[80];
    crdfilebuf = (char *) check_malloc( strlen(fname)+SYSCODE_LEN+10);

    /* Copy the order to the stnadjustment obscount element */

    for( i = 0; i < ra->nstn; i++ )
    {
        int istn = i+1;
        station * st = stnptr(istn);
        stn_adjustment *sa = stnadj( st );
        sa->obscount = ra->order[i];
    }

    /* For each order check that there is a station achieving that order, and if
       so create a station file */

    for( iorder = 0; iorder <= hsdc->norder; iorder++ )
    {
        for( i = 0; i < ra->nstn; i++ )
        {
            char *order;
            if( ra->order[i] != iorder ) continue;
            order = iorder ? hsdc->tests[iorder-1].scOrder: dfltOrder;
            sprintf(crdfilebuf,"%s_%s.crd",fname,order);
            sprintf(comment,"Stations assigned order %s by snapspec - run at %s",
                    order,spec_run_time);
            station_order = iorder;
            write_network(net,crdfilebuf,comment,coord_precision,stations_of_order);
            break;
        }
    }

    check_free(crdfilebuf);
}

static void copy_unused_roles_to_orders( stn_relacc_array *ra )
{
    for( int istn = 0; istn < number_of_stations(net); istn++ )
    {
        if( ra->role[istn] == SDC_CONTROL_MARK || ra->role[istn] == SDC_IGNORE_MARK )
        {
            ra->order[istn]=ra->role[istn];
        }
    }
}

static void update_station_orders( hSDCTest hsdc, stn_relacc_array *ra )
{
    int istn;

    /* Force station orders onto network here - alternative is to raise error
       if they are not already included. */

    int order_class = add_network_orders( net );

    for( istn = 0; istn++ < number_of_stations(net); )
    {
        int iorder;
        char *order;
        station *stn;


        stn = station_ptr(net,istn);

        iorder = ra->order[istn-1];
        if( iorder < 0 || iorder > hsdc->norder ) continue;

        order = iorder ? hsdc->tests[iorder-1].scOrder: dfltOrder;

        set_station_class( stn, order_class, network_order_id( net, order, 1 ));
    }

}


static void update_crdfile( char *fname )
{
    write_network( net, fname, "Coordinate orders updated by snapspec",coord_precision,0);
    printf("Updated station orders in %s\n",fname);
}

static int get_max_control_order( hSDCTest hsdc, stn_relacc_array *ra, const char **max_order_str )
{
    int *order_lookup;
    int nnetorder;
    int nbadorder;
    int orderid;
    int max_order = -1;
    int i,j;
    int istn;
    int sorted;

    if( ! network_order_count(net) )
    {
        if( ra->logfile )
        {
            fprintf(ra->logfile,
                    "Cannot determine control station orders as network doesn't have order information\n"
                   );
        }
        printf("Cannot determine control station orders as network doesn't have order information\n");
        return -1;
    }

    /* See if the orders are sorted */

    sorted = 1;
    for( i = 0; i < hsdc->norder; i++ )
    {
        char *next = (i == hsdc->norder - 1) ? dfltOrder : hsdc->tests[i+1].scOrder;
        if( stncodecmp(next,hsdc->tests[i].scOrder) <= 0 ) { sorted = 0; break; }
    }

    /* Set up the order_lookup array which converts from the network station orders
       to the SDC test orders */

    nnetorder = network_order_count(net);
    order_lookup = (int *) check_malloc( sizeof(int) * (nnetorder+1) );

    /* Orders will be defined as -2: undefined, -1 less than lowest test, >=0 control order */
    /* Note that network order 0 is the default "undefined" order, ie "-", and so is always
       undefined. If orders are sorted then can infer lowest controlled order from orders
       not included in tests... */

    order_lookup[0] = -2;

    for( i = 1; i < nnetorder; i++ )
    {
        const char *order = network_order(net,i);
        order_lookup[i] = sorted ? -1 : -2;

        for( j = 0; j <= hsdc->norder; j++ )
        {
            char *testorder = j < hsdc->norder ? hsdc->tests[j].scOrder : dfltOrder;
            int cmp = stncodecmp(testorder,order);
            if( cmp == 0 ) { order_lookup[i] = j; break; }
            if( sorted )
            {
                if( cmp > 0 ) break;
                order_lookup[i] = j;
            }
        }
    }

    nbadorder = 0;

    for( istn = 0; istn++ < number_of_stations( net ); )
    {
        station *st;
        stn_adjustment *sa;
        int iorder;

        st = station_ptr(net,istn);
        sa = stnadj( st );

        if( ignored_station(istn) || rejected_station(istn) ) continue;

        if( (ra->testhor && sa->hrowno) || (ra->testvrt && sa->vrowno) ) continue;

        orderid = network_station_order(net,st);
        iorder = order_lookup[orderid];
        if( iorder == -2 )
        {
            if( ra->logfile )
            {
                fprintf(ra->logfile,"Control station %s has unrecognized order %s\n",
                        st->Code, network_order( net, orderid ) );
            }
            nbadorder++;
            order_lookup[orderid] = -3;
        }
        else if( iorder > max_order )
        {
            max_order = iorder;
            (*max_order_str) = network_order(net,orderid);
        }
        else if( sorted && orderid > 0)
        {
            const char *orderstr = network_order( net, orderid );
            if( ! (*max_order_str) || strcmp((*max_order_str),orderstr) > 0 )
            {
                (*max_order_str) = orderstr;
            }
        }
    }

    if( nbadorder )
    {
        for( i = 0; i <= nnetorder; i++ )
        {
            if( order_lookup[i] == -3 )
            {
                if( i == 0 )
                {
                    printf("Control stations have undefined orders\n");
                }
                else
                {
                    printf("Control stations order %s not defined in snapspec configuration\n",
                           network_order(net,i) );
                }
            }
        }
        max_order = -2;
    }

    check_free( order_lookup );
    return max_order;
}

static int setup_hv_mode( int hvmode, hSDCTest hsdc, stn_relacc_array *ra )
{
    int adjhor;
    int adjvrt;
    int adj3d;
    int testhor;
    int testvrt;
    int i;
    int nstns;
    FILE *out = ra->logfile;

    adjhor = adjvrt = adj3d = 0;
    nstns = number_of_stations(net);
    for( i = 1; i <= nstns; i++ )
    {
        stn_adjustment *isa = stnadj(stnptr(i));
        if( isa->hrowno ) adjhor++;
        if( isa->vrowno ) adjvrt++;
        if( isa->hrowno && isa->vrowno ) adj3d++;
    }

    testhor = 0;
    testvrt = 0;
    for( i = 0; i < hsdc->norder; i++ )
    {
        hSDCOrderTest test = &(hsdc->tests[i]);
        if( test->blnTestHor ) testhor=1;
        if( test->blnTestVrt ) testvrt=1;
    }

    if( hvmode == SRA_HVMODE_HOR ) testvrt = 0;
    if( hvmode == SRA_HVMODE_VRT ) testhor = 0;

    if( hvmode == SRA_HVMODE_HOR && testhor == 0 )
    {
        printf("snapspec: Requested horizontal testing but configuration doesn't have horizontal tests\n");
        fprintf(out,"Requested horizontal testing but configuration doesn't have horizontal tests\n");
        return INCONSISTENT_DATA;
    }

    if( hvmode == SRA_HVMODE_VRT && testvrt == 0 )
    {
        printf("snapspec: Requested vertical testing but configuration doesn't have vertical tests\n");
        fprintf(out,"Requested vertical testing but configuration doesn't have vertical tests\n");
        return INCONSISTENT_DATA;
    }

    if( hvmode == SRA_HVMODE_3D && (testvrt == 0 || testhor == 0) )
    {
        printf("snapspec: Requested 3d testing but configuration doesn't have 3d tests\n");
        fprintf(out,"Requested 3d testing but configuration doesn't have 3d tests\n");
        return INCONSISTENT_DATA;
    }

    if( hvmode == SRA_HVMODE_HOR && adjhor == 0 )
    {
        printf("snapspec: Requested horizontal testing but no stations are adjusted horizontally\n");
        fprintf(out,"Requested horizontal testing but no stations are adjusted horizontally\n");
        return INCONSISTENT_DATA;
    }

    if( hvmode == SRA_HVMODE_VRT && adjvrt == 0 )
    {
        printf("snapspec: Requested vertical testing but no stations are adjusted vertically\n");
        fprintf(out,"Requested vertical testing but no stations are adjusted vertically\n");
        return INCONSISTENT_DATA;
    }

    if( hvmode == SRA_HVMODE_3D && adj3d == 0 )
    {
        printf("snapspec: Requested 3d testing but no stations are adjusted in three dimensions\n");
        fprintf(out,"Requested 3d testing but no stations are adjusted in three dimensions\n");
        return INCONSISTENT_DATA;
    }

    if( hvmode == SRA_HVMODE_AUTO )
    {
        if( testhor && ! adjhor )
        {
            fprintf(out,"Horizontal tests will not be applied as no stations are adjusted horizontally\n");
            testhor = 0;
        }
        if( testvrt && ! adjvrt )
        {
            fprintf(out,"Vertical tests will not be applied as no stations are adjusted vertically\n");
            testvrt = 0;
        }
        if( ! testhor && ! testvrt )
        {
            printf("snapspec: Requested testing is inconsistent with adjustment\n");
            fprintf(out,"Requested testing is inconsistent with adjustment\n");
            return INCONSISTENT_DATA;
        }
    }

    for( i = 0; i < hsdc->norder; i++ )
    {
        hSDCOrderTest test = &(hsdc->tests[i]);
        if( ! testhor ) test->blnTestHor = BLN_FALSE;
        if( ! testvrt ) test->blnTestVrt = BLN_FALSE;
    }

    if( testhor && testvrt && (adjhor != adj3d || adjvrt != adj3d) )
    {
        printf("Warning: testing 3d adjustment but some stations are adjusted hor/vrt only\n");
        fprintf(out,"Warning: testing 3d adjustment but some stations are adjusted hor/vrt only\n");
    }
    return OK;
}

/*======================================================================*/



static int read_test_command(CFG_FILE *cfg, char *string, void *value, int, int )
{
    char *name;
    char *data;
    char type[21];
    char errtype[5];
    int nfld;
    int nchr;
    double err;
    int errdirflg;
    int errdirflgv;
    hSDCOrderTest test;
    hSDCTest hsdc = * (hSDCTest *) value;

    name = strtok(string," ");
    data = strtok(NULL,"\n");
    if( ! name || ! data )
    {
        send_config_error( cfg, INVALID_DATA, "Missing data in specification command" );
        return OK;
    }

    if( strlen(name) > SYSCODE_LEN )
    {
        send_config_error( cfg, INVALID_DATA, "Order name too long in test command");
        return OK;
    }

    if( hsdc->norder >= hsdc->maxorder )
    {
        send_config_error( cfg, INVALID_DATA, "Too many specification commands" );
        return OK;
    }

    test = hsdc->tests+hsdc->norder;
    hsdc->norder++;
    test->idOrder = hsdc->norder;
    strncpy( test->scOrder, name, SYSCODE_LEN );
    test->scOrder[SYSCODE_LEN] = 0;

    test->blnAutoRange = BLN_FALSE;
    test->dblRange = 0.0;
    test->blnTestHor = BLN_FALSE;
    test->dblAbsTestAbsMax = 1000.0;
    test->dblAbsTestDDMax  = 1000.0;
    test->dblAbsTestDFMax  = 1000.0;
    test->dblRelTestAbsMin = 0.0;
    test->dblRelTestDFMax  = 1000.0;
    test->dblRelTestDDMax  = 0.0;
    test->blnTestVrt = BLN_FALSE;
    test->dblAbsTestAbsMaxV = 1000.0;
    test->dblAbsTestDDMaxV  = 1000.0;
    test->dblAbsTestDFMaxV  = 1000.0;
    test->dblRelTestAbsMinV = 0.0;
    test->dblRelTestDFMaxV  = 1000.0;
    test->dblRelTestDDMaxV  = 0.0;

    errdirflg = 0;
    errdirflgv = 0;

    for(;;)
    {
        int errtypflg = 0;
        int errdir = 0;
        int vflag = 0;
        int haveppm = 0;

        nfld = sscanf(data,"%20s%n",type,&nchr);
        if( nfld <= 0 ) break;
        if( nfld != 1 )
        {
            send_config_error( cfg, INVALID_DATA, "Invalid specification accuracy");
            return OK;
        }
        data += nchr;

        if( _stricmp(type,"autorange") == 0 )
        {
            test->blnAutoRange = BLN_TRUE;
            continue;
        }
        if( _stricmp(type,"range") == 0 )
        {
            nfld = sscanf(data,"%lf%4s%n",&err,errtype,&nchr);
            if( nfld != 2 || strcmp(errtype,"m") != 0 )
            {
                send_config_error(cfg,INVALID_DATA, "Invalid range in specification");
                return OK;
            }
            data += nchr;
            test->dblRange = err;
            continue;
        }

        if( type[0] == 'h' || type[0] == 'v' )
        {
            vflag = type[0] == 'v' ? 64 : 0;

            if( _stricmp(type+1, "_abs_max") == 0 )
            {
                errdir = 1;
            }
            else if( _stricmp(type+1, "_rel_to_control") == 0 )
            {
                errdir = 2;
                haveppm = 1;
            }
            else if( _stricmp(type+1, "_rel") == 0 )
            {
                errdir = 4;
                haveppm = 1;
            }
            else if( _stricmp(type+1, "_rel_min_abs") == 0 )
            {
                errdir = 8;
            }
        }

        if( vflag )
        {
            if( ! errdir || errdir & errdirflgv )
            {
                send_config_error( cfg, INVALID_DATA, "Invalid accuracy specification");
                return OK;
            }
            errdirflgv |= errdir;
        }
        else
        {
            if( ! errdir || errdir & errdirflg )
            {
                send_config_error( cfg, INVALID_DATA, "Invalid accuracy specification");
                return OK;
            }
            errdirflg |= errdir;
        }

        for(;;)
        {
            double err;
            int errtyp = haveppm ? 0 : 32;

            nfld = sscanf(data,"%lf%4s%n",&err,errtype,&nchr);
            if( nfld <= 0 ) break;
            data += nchr;
            if( nfld != 2 )
            {
                send_config_error(cfg,INVALID_DATA, "Invalid accuracy in specification");
                return OK;
            }
            if( _stricmp(errtype,"MM") == 0 )
            {
                errtyp = 16;
            }
            else if( _stricmp(errtype,"PPM") == 0 && haveppm )
            {
                errtyp = 32;
            }
            if( ! errtyp || errtyp & errtypflg )
            {
                send_config_error(cfg,INVALID_DATA,
                                  "Invalid error type in specification -  must be mm or ppm");
                return OK;
            }
            switch( errtyp + errdir + vflag)
            {
            case 17: test->dblAbsTestAbsMax = err/1000; break;
            case 18: test->dblAbsTestDFMax = err/1000; break;
            case 34: test->dblAbsTestDDMax = err/10000; break;
            case 24: test->dblRelTestAbsMin = err/1000; break;
            case 20: test->dblRelTestDFMax = err/1000; break;
            case 36: test->dblRelTestDDMax = err/10000; break;

            case 81: test->dblAbsTestAbsMaxV = err/1000; break;
            case 82: test->dblAbsTestDFMaxV = err/1000; break;
            case 98: test->dblAbsTestDDMaxV = err/10000; break;
            case 88: test->dblRelTestAbsMinV = err/1000; break;
            case 84: test->dblRelTestDFMaxV = err/1000; break;
            case 100: test->dblRelTestDDMaxV = err/10000; break;
            }

            errtypflg |= errtyp;
            if( errtypflg == 48 ) break;
        }
        if( errtypflg == 0 )
        {
            send_config_error( cfg, INVALID_DATA, "Missing error values in specification");
            return OK;
        }

        if( errdirflg == 15 && errdirflgv == 15) break;
    }

    if( errdirflg ) test->blnTestHor = BLN_TRUE;
    if( errdirflgv ) test->blnTestVrt = BLN_TRUE;

    if( ! errdirflg && ! errdirflgv )
    {
        send_config_error( cfg, INVALID_DATA, "Specification doesn't include any tolerances");
        return OK;
    }

    return OK;
}

static int find_order( hSDCTest hsdc, char *order )
{
    int iorder = -1;
    int i;
    for( i = 0; i < hsdc->norder; i++ )
    {
        if( _stricmp(order,hsdc->tests[i].scOrder) == 0 )
        {
            iorder = i;
            break;
        }
    }
    return iorder;
}

typedef struct
{
    short order;
    stn_relacc_array *ra;
} limit_order_params;

static void set_max_order( station *st, void *data )
{
    limit_order_params *p = (limit_order_params *)data;
    int istn = st->id;
    if( istn > 0 ) p->ra->role[istn-1] = p->order;
}

static void set_priority( station *st, void *data )
{
    limit_order_params *p = (limit_order_params *)data;
    int istn = st->id;
    if( istn > 0 ) p->ra->priority[istn-1] = p->order;
}

static int read_limit_order_command(CFG_FILE *cfg, char *string, void *value, int, int )
{
    limit_order_params p;
    char *name;
    char *data;
    int nerr;

    name = strtok(string," ");
    data = strtok(NULL,"\n");
    if( ! name || ! data )
    {
        send_config_error( cfg, INVALID_DATA, "Missing order or station list in limit_order command" );
        return OK;
    }

    if( strlen(name) > SYSCODE_LEN )
    {
        send_config_error( cfg, INVALID_DATA, "Order name too long in limit_order command");
        return OK;
    }

    p.ra = * (stn_relacc_array **) value;
    p.order = find_order(p.ra->hsdc,name);
    if( p.order == -1 )
    {
        send_config_error( cfg, INVALID_DATA, "Unrecognized order in limit_order command");
        return OK;
    }

    set_error_location( get_config_location(cfg));
    nerr = get_error_count();

    process_selected_stations( net,data,cfg->name,&p,set_max_order);

    set_error_location(NULL);
    cfg->errcount += (get_error_count()-nerr);

    return OK;
}

static int read_ignore_command(CFG_FILE *cfg, char *string, void *value, int, int )
{
    limit_order_params p;
    int nerr;

    p.ra = * (stn_relacc_array **) value;
    p.order = SDC_IGNORE_MARK;

    set_error_location( get_config_location(cfg));
    nerr = get_error_count();

    process_selected_stations( net,string,cfg->name,&p,set_max_order);

    set_error_location(NULL);
    cfg->errcount += (get_error_count()-nerr);

    return OK;
}

static int read_set_priority_command(CFG_FILE *cfg, char *string, void *value, int, int )
{
    limit_order_params p;
    char *prioritystr;
    char *data;
    char check;
    int priority;
    int nerr;

    prioritystr = strtok(string," ");
    data = strtok(NULL,"\n");
    if( ! prioritystr || ! data )
    {
        send_config_error( cfg, INVALID_DATA, "Missing priority or station_list in set_priority command" );
        return OK;
    }

    if( sscanf(prioritystr,"%d%c",&priority,&check) != 1 || priority < 0 )
    {
        send_config_error( cfg, INVALID_DATA, "Invalid priority in set_priority command - must be a positive number");
        return OK;
    }

    p.ra = * (stn_relacc_array **) value;
    p.order = priority;

    set_error_location( get_config_location(cfg));
    nerr = get_error_count();

    process_selected_stations( net,data,cfg->name,&p,set_priority);

    set_error_location(NULL);
    cfg->errcount += (get_error_count()-nerr);

    return OK;
}

static int read_confidence(CFG_FILE *cfg, char *string, void *, int, int )
{
    if( sscanf(string,"%lf%%",&test_confidence) < 1 ||
            test_confidence <= 0 || test_confidence >= 100 )
    {
        send_config_error( cfg, INVALID_DATA, "Invalid confidence in specification");
    }
    return OK;
}


static int read_error_type(CFG_FILE *cfg, char *string, void *, int, int )
{
    char error_type[32];
    sscanf(string,"%31s",error_type);
    if( _stricmp(error_type,"apriori") == 0 )
    {
        test_apriori = 1;
    }
    else if( _stricmp(error_type,"aposteriori") == 0 )
    {
        test_apriori = 0;
    }
    else
    {
        send_config_error( cfg, INVALID_DATA, "Invalid error_type in specification");
    }
    return OK;
}

/* Disable warning about truncation from int* to int in OFFSETOF ... it's really
   what is supposed to happen!
*/

// #pragma warning ( disable : 4305 )

#define STN_CONFIG_BUFSIZE 127

static int read_station_config_file( const char *filename, stn_relacc_array *ra, int csv )
{
    char record[STN_CONFIG_BUFSIZE+1];
    char *field[3];
    int codefield=-1;
    int orderfield=-1;
    int priorityfield=-1;
    int sts=OK;
    int nbadstn=0;
    char *eol;
    limit_order_params p;
    FILE *f = fopen(filename,"r");
    if( ! f ) return FILE_OPEN_ERROR;
    p.ra=ra;

    int first=1;
    int nrec=0;
    while( ! feof(f) )
    {
        if( ! fgets(record,STN_CONFIG_BUFSIZE,f) ) break;
        nrec++;
        /* Split into fields */
        eol=strchr(record,'\n');
        field[0]=field[1]=field[2]=0;
        int nfield=0;
        if( csv )
        {
            for( char *c=record; *c; c++ ){ if( *c == '"' ) *c=' '; }
            field[0]=record;
            nfield=1;
            for( char *c=record; nfield<3 && *c; c++ )
            {
                if( *c == ',' ){ field[nfield++]=c+1; *c=0; }
            }
            for( int ifld=0; ifld < nfield; ifld++ )
            {
                char *c=field[ifld];
                while(isspace(*c)) c++;
                field[ifld]=c;
                while(*c && ! isspace(*c)) c++;
                *c=0;
            }
        }
        else
        {
            field[0]=strtok(record," \n\t");
            if( field[0] ) { field[1]=strtok(NULL," \n\t"); }
            if( field[1] ) { field[2]=strtok(NULL," \n\t"); }
            if( field[2] ) nfield=3;
            else if( field[1] ) nfield=2;
            else if( field[0] ) nfield=1;
        }
        if( first )
        {
            first=0;
            for( int ifld=0; ifld < nfield; ifld++ )
            {
                char *fld=field[ifld];
                if( _stricmp(fld,"code") == 0 ) codefield=ifld;
                else if( _stricmp(fld,"order") == 0 ) orderfield=ifld;
                else if( _stricmp(fld,"priority") == 0 ) priorityfield=ifld;
            }
            if( codefield < 0 )
            {
                sts=MISSING_DATA;
                handle_error(sts,"Station configuration file does not include a \"code\" field",
                        filename);
                break;
            }
            if( orderfield < 0 && priorityfield < 0 )
            {
                handle_error(sts,"Station configuration file does not include a \"order\" or \"priority\" field\n",
                        filename);
            }
        }
        else
        {
            char *code=field[codefield];
            int istn = find_station(net,code);
            if( istn <= 0 ) nbadstn++;
            if( istn > 0 && orderfield > 0 && field[orderfield] && *(field[orderfield])) 
            {
                if( strcmp(field[orderfield],"-") != 0 && strcmp(field[orderfield],"") != 0 )
                {
                    int order=SDC_IGNORE_MARK;
                    if( strcmp(field[orderfield],"*") != 0 )
                    {
                        order = find_order(p.ra->hsdc,field[orderfield]);
                        if( order >= 0 )
                        {
                            ra->role[istn-1]=order;
                        }
                        else
                        {
                            char errmsg1[80];
                            char errmsg2[MAX_FILENAME_LEN+80];
                            sprintf(errmsg1,"Invalid order %.10s in station configuration file",
                                    field[orderfield]);
                            sprintf(errmsg2,"Line %d file %*s",nrec,MAX_FILENAME_LEN,filename);
                            handle_error(INVALID_DATA,errmsg1,errmsg2);
                            sts=INVALID_DATA;
                        }
                    }
                    else
                    {
                        ra->role[istn-1]=order;
                    }
                }
            }
            if( istn > 0 && priorityfield > 0 && field[priorityfield] && *(field[priorityfield])) 
            {
                if( strcmp(field[priorityfield],"-") != 0 && strcmp(field[priorityfield],"") != 0 )
                {
                    int priority;
                    char check;
                    if( strcmp(field[priorityfield],"*") == 0 )
                    {
                        ra->role[istn-1]=SDC_IGNORE_MARK;
                    }
                    else if( sscanf(field[priorityfield],"%d%c",&priority,&check) == 1 )
                    {
                        ra->priority[istn-1]=priority;
                    }
                    else
                    {
                        char errmsg1[80];
                        char errmsg2[MAX_FILENAME_LEN+80];
                        sprintf(errmsg1,"Invalid priority %.10s in station configuration file",
                                field[priorityfield]);
                        sprintf(errmsg2,"Line %d file %*s",nrec,MAX_FILENAME_LEN,filename);
                        handle_error(INVALID_DATA,errmsg1,errmsg2);
                        sts=INVALID_DATA;
                    }
                }
            }
        }
        if( ! eol ){ while( 1 ) { int c=fgetc(f); if( c == '\n' || c == EOF ) break; } }
    }
    fclose(f);
    if( nbadstn > 0 )
    {
        char errmsg1[80];
        char errmsg2[MAX_FILENAME_LEN+80];
        sprintf(errmsg1,"%d unrecognized stations in station configuration file",
                nbadstn);
        sprintf(errmsg2,"File %*s",MAX_FILENAME_LEN,filename);
        handle_error(INFO_ERROR,errmsg1,errmsg2);
    }
    return sts;
}

static int read_station_config_command(CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_configuration_command(CFG_FILE *cfg, char *string, void *value, int len, int code );
static int read_options_command(CFG_FILE *cfg, char *string, void *value, int len, int code ); 
static int read_log_level_command(CFG_FILE *cfg, char *string, void *value, int len, int code );

static config_item cfg_commands[] =
{
    {"configuration",NULL,CFG_ABSOLUTE,0,read_configuration_command,0,1},
    {"test",NULL,CFG_ABSOLUTE,0,read_test_command,CFG_REQUIRED,1},
    {"log_level",NULL,CFG_ABSOLUTE,0,read_log_level_command,CFG_ONEONLY,1},
    {"test_config_options",NULL,OFFSETOF(SDCTest,options),0,readcfg_int,CFG_ONEONLY,1},
    {"output_log",NULL,OFFSETOF(stn_relacc_array,outputlog),0,readcfg_boolean,CFG_ONEONLY,2},
    {"confidence",NULL,CFG_ABSOLUTE,0,read_confidence,CFG_ONEONLY,0},
    {"vertical_error_factor",&vrtHorRatio,CFG_ABSOLUTE,0,readcfg_double,CFG_ONEONLY,0},
    {"error_type",NULL,CFG_ABSOLUTE,0,read_error_type,CFG_ONEONLY,0},
    {"default_order",dfltOrder,CFG_ABSOLUTE,SYSCODE_LEN+1,STORE_AS_STRING,CFG_ONEONLY,0},
    {"limit_order",NULL,CFG_ABSOLUTE,0,read_limit_order_command,0,2},
    {"ignore",NULL,CFG_ABSOLUTE,0,read_ignore_command,0,2},
    {"station_configuration_file",NULL,CFG_ABSOLUTE,0,read_station_config_command,0,2},
    {"set_priority",NULL,CFG_ABSOLUTE,0,read_set_priority_command,0,2},
    {"options",NULL,CFG_ABSOLUTE,0,read_options_command,0,2},
    {NULL}
};

// #pragma warning ( default : 4305 )

static int read_configuration_command(CFG_FILE *cfg, char *string, void *, int, int )
{
    char *basecfn;
    const char *cfn;
    CFG_FILE *cfg2;
    cfg_stack *stack;
    int nerr;

    basecfn = strtok(string," \t\n");
    if( ! basecfn )
    {
        send_config_error( cfg, INVALID_DATA, "Configuration file name missing");
        return OK;
    }
    cfn = find_file( basecfn, ".cfg", cfg->name, FF_TRYALL, "snapspec" );

    if( cfn )
    {
        int recurse = 0;
        if( strcmp(cfn,cfg->name) == 0 ) recurse = 1;
        for( stack = cfgs; stack && ! recurse; stack = stack->next )
        {
            if( strcmp(stack->cfg->name,cfn) == 0 ) recurse = 1;
        }
        if( recurse )
        {
            char buf[120];
            sprintf(buf,"Error - recursive load of configuration %.60s",basecfn);
            send_config_error(cfg, INVALID_DATA,buf);
            return OK;
        }
    }

    cfg2 = NULL;
    if( cfn ) { cfg2 = open_config_file( cfn, '!' );}
    if( !cfg2 )
    {
        char buf[120];
        sprintf(buf,"Cannot open configuration file %.60s",basecfn);
        send_config_error( cfg, INVALID_DATA, buf);
        return OK;
    }

    {
        cfg_stack stack_entry;
        stack = cfgs;
        stack_entry.cfg = cfg;
        stack_entry.next = stack;
        cfgs = &stack_entry;

        nerr = read_config_file( cfg2, cfg_commands );

        cfgs = stack;
    }

    close_config_file( cfg2 );

    if( nerr > 0 )
    {
        char buf[120];
        sprintf(buf,"Errors processing configuration file %.60s",basecfn);
        send_config_error( cfg, INVALID_DATA, buf);
        return OK;
    }

    return OK;
}

static int read_station_config_command(CFG_FILE *cfg, char *string, void *value, int, int )
{
    char *stcfgfn;
    char *format;
    char *remainder;
    const char *cfn;
    int csv=0;

    stcfgfn = strtok(string," \t\n");
    if( ! stcfgfn )
    {
        send_config_error( cfg, INVALID_DATA, "Station configuration file name missing");
        return OK;
    }

    format=strtok(NULL," \t\n");
    remainder=strtok(NULL," \t\n");
    if( format )
    {
        if( _stricmp(format,"csv") == 0 )
        {
            csv=1;
        }
        else
        {
            remainder=format;
        }
    }
    if( remainder )
    {
        char buf[120];
        sprintf(buf,"Error - invalid format %.60s in station_configuration_file command",
                remainder);
        send_config_error(cfg, INVALID_DATA, buf );
        return OK;
    }

    cfn = find_file( stcfgfn, csv ? ".csv" : ".dat", cfg->name, FF_TRYALL, 0 );

    if( cfn )
    {
        int sts=read_station_config_file( cfn, * (stn_relacc_array **) value, csv );
        if( sts  != OK )
        {
            char buf[100+MAX_FILENAME_LEN];
            sprintf(buf,"Errors encountered reading station configuration file %.*s",
                    MAX_FILENAME_LEN,cfn);
            send_config_error( cfg, INVALID_DATA, buf);
        }
    }
    else
    {
        char buf[100+MAX_FILENAME_LEN];
        sprintf(buf,"Cannot open station configuration file %.*s",MAX_FILENAME_LEN,stcfgfn);
        send_config_error( cfg, INVALID_DATA, buf);
    }
    return OK;
}

static int read_options_command(CFG_FILE *cfg, char *string, void *value, int, int )
{
    char *option;
    stn_relacc_array *ra = * (stn_relacc_array **) value;
    for( option=strtok(string," \t\n"); option; option=strtok(NULL," \t\n") )
    {
        if( _stricmp(option,"limit_orders_by_control") == 0 
                || _stricmp(option,"auto_min_order") == 0 )
        {
            ra->autominorder=1;
        }
        else if( _stricmp(option,"fail_if_no_relacc_tests") == 0 )
        {
            ra->hsdc->options |= SDC_OPT_FAIL_NORELACC;
        }
        else if( _stricmp(option,"no_rel_acc_by_abs_optimisation") == 0 )
        {
            ra->hsdc->options |= SDC_OPT_NO_SHORTCIRCUIT_CVR;
        }
        else if( _stricmp(option,"strict_rel_acc_by_abs_optimisation") == 0 )
        {
            ra->hsdc->options |= SDC_OPT_STRICT_SHORTCIRCUIT_CVR;
        }
        else
        {
            char errmsg[100];
            sprintf(errmsg,"Invalid value %.40s in option command",
                    option);
            send_config_error(cfg, INVALID_DATA, errmsg );
        }
    }
    return OK;
}

static int read_log_level_command(CFG_FILE *cfg, char *string, void *value, int, int )
{
    hSDCTest sdc=*(hSDCTest *) value;
    int level;
    if( readcfg_int(cfg,string,&level,sizeof(int),0) == OK )
    {
        sdc->loglevel |= level;
    }
    else
    {
        for( char *option=strtok(string," \t\n"); option; option=strtok(NULL," \t\n") )
        {
            level=0;
            if( _stricmp(option,"steps") == 0 )
            {
                level=SDC_LOG_STEPS;
            }
            else if( _stricmp(option,"test_details") == 0 )
            {
                level=SDC_LOG_TESTS;
            }
            else if( _stricmp(option,"accuracy_calcs") == 0 )
            {
                level=SDC_LOG_CALCS;
            }
            else if( _stricmp(option,"distance_calcs") == 0 )
            {
                level=SDC_LOG_DISTS;
            }
            else if( _stricmp(option,"rel_acc_calcs") == 0 )
            {
                level=SDC_LOG_CALCS2;
            }
            else if( _stricmp(option,"debug") == 0 )
            {
                level=SDC_LOG_ALL;
            }
            else if( _stricmp(option,"timing") == 0 )
            {
                level=SDC_LOG_TIMESTAMP;
            }
            else
            {
                char errmsg[100];
                sprintf(errmsg,"Invalid value %.40s in option command",
                        option);
                send_config_error(cfg, INVALID_DATA, errmsg );
                continue;
            }
            sdc->loglevel |= level;
        }
    }
    return OK;
}

static void set_sdctest_pointer( hSDCTest *phsdc )
{
    config_item *ci;
    for( ci = cfg_commands; ci->option; ci++ )
    {
        if( ci->code & 1 ) ci->value = phsdc;
    }
}

static void set_relacc_pointer( stn_relacc_array **ra )
{
    config_item *ci;
    for( ci = cfg_commands; ci->option; ci++ )
    {
        if( ci->code & 2 ) ci->value = ra;
    }
}

static int read_configuration( CFG_FILE *cfg, hSDCTest hsdc )
{
    int nerr;
    int i;
    stn_relacc_array *ra = (stn_relacc_array *)(hsdc->env);
    set_sdctest_pointer( &hsdc );
    set_relacc_pointer( &ra );
    strcpy(dfltOrder,"NONE");
    nerr = read_config_file( cfg, cfg_commands );
    ra->loglevel = hsdc->loglevel;
    if( hsdc->norder < 1 )
    {
        send_config_error( cfg, MISSING_DATA, "No tests are defined in the configuration");
        nerr++;
    }
    ra->testhor = 0;
    ra->testvrt = 0;
    for( i = 0; i < hsdc->norder; i++ )
    {
        SDCOrderTest *test = &(hsdc->tests[i]);
        test->dblVertHorRatio = vrtHorRatio;
        if( test->blnTestHor ) ra->testhor = 1;
        if( test->blnTestVrt ) ra->testvrt = 1;
    }
    return nerr;
}

static void set_test_confidence( hSDCTest hsdc )
{
    double prob;
    double htolfactor;
    // double vtolfactor;

    prob = 1-test_confidence/100.0;

    /* Fix up specification scale factors for confidence and
       degrees of freedom */

    if( test_apriori )
    {
        // vtolfactor = sqrt(fabs(inv_chi2_distn( prob, 1 )));
        htolfactor = sqrt(fabs(inv_chi2_distn( prob, 2 )));
    }
    else
    {
        // vtolfactor = sqrt(fabs(inv_f_distn( prob, 1, dof )));
        htolfactor = sqrt(fabs(inv_f_distn( prob, 2, dof )*2));
    }

    hsdc->dblErrFactor = htolfactor;
    // At the moment vertical tolerance not implemented properly ...
}


/*======================================================================*/



static const char *default_cfg_name = "snapspec";

int main( int argc, char *argv[] )
{
    char *bfn;
    CFG_FILE *cfg = 0;
    const char *cfn;
    const char *basecfn, *ofn;
    int nerr;
    hSDCTest hsdc;
    BINARY_FILE *b;
    FILE *out;
    stn_relacc_array *ra;
    char *min_order_str = NULL;
    char *modestr = NULL;
    const char *max_control_str = NULL;
    char *outputcsvname = NULL;
    char *updatecrdfile = NULL;
    char *splitcrdfile = NULL;
    int use_cache = 0;
    char *cvrcachefile = 0;
    int autominorder = 0;
    int hvmode = SRA_HVMODE_AUTO;
    int sts;

    get_date( spec_run_time );

    /* Default is not to use multithreaded matrix ops */

    blt_set_number_of_threads(1);

    printf( "snapspec %s: Tests adjustments against relative accuracy specifications\n"
            "            and calculates orders of coordinates\n",PROGRAM_VERSION);


    while( argc >= 3 && argv[1][0] == '-' )
    {
        int narg = 1;
        int narg2 = 2;
        char *arg2=argv[2];
        if( argv[1][2] )
        {
            narg2 = 1;
            arg2 = argv[1]+2;
        }

        switch( argv[1][1] )
        {
        case 'o':
        case 'O':
            min_order_str = arg2;
            narg=narg2;
            break;

        case 'f': /* For backwards compatibility */
        case 'F':
        case 's':
        case 'S':
            splitcrdfile = arg2;
            narg=narg2;
            break;

        case 'c':
        case 'C':
            outputcsvname=arg2;
            narg=narg2;
            break;

        case 'u':
        case 'U':
            updatecrdfile = arg2;
            narg=narg2;
            break;

        case 'm':
        case 'M':
            modestr = arg2;
            narg=narg2;
            break;

        case 'a':
        case 'A':
            autominorder = 1;
            break;

        case 'v':
        case 'V':
            use_cache=1;
            break;

        case 't':
        case 'T': {
            int nthread;
            if( _stricmp(arg2,"auto") == 0 )
            {
                nthread=BLT_DEFAULT_NTHREAD;
            }
            else if( sscanf(arg2,"%d",&nthread) != 1 || nthread <= 0 )
            {
                printf("snapspec: Invalid value %s for number of threads",arg2);
                return 0;
            }
            blt_set_number_of_threads(nthread);
            }
            narg=narg2;
            break;

        default:
            printf("snapspec: Invalid option %s\n",argv[1]);
            return 0;
        }
        argv+=narg;
        argc-=narg;
    }

    if( modestr )
    {
        switch( modestr[0] )
        {
        case 'a':
        case 'A':
            hvmode = SRA_HVMODE_AUTO; break;
        case 'h':
        case 'H':
        case '2':
            hvmode = SRA_HVMODE_HOR; break;
        case 'v':
        case 'V':
        case '1':
            hvmode = SRA_HVMODE_VRT; break;
        case '3':
            hvmode = SRA_HVMODE_3D; break;
        default:
            printf("snapspec: Invalid mode string %s\n",modestr);
        }
    }

    if( argc != 3 && argc != 4 )
    {
        printf("\nSyntax: snapspec [options]] binary_file_name [config_file_name] listing_file_name\n");
        printf("\nOptions are:\n");
        printf("   -o order      Specifies the highest order to test\n");
        printf("   -a            Base the highest order on the control station orders\n");
        printf("   -m mode       The mode for testing, 3d, horizontal, vertical, or auto (default)\n");
        printf("   -u filename   Updates the orders in the named coordinate file\n");
        printf("   -s filename   Base name for seperate coordinate files for each order (no extension)\n");
        printf("   -c filename   Output results in a csv file\n");
        printf("   -v            Use covariance cache (.ssc file) if calculating covariances\n");
        printf("   -t #|auto     Specifies the number of threads to use\n\n");
        printf("Use filename \"-\" to use default filenames for outputs\n\n");
        return 0;
    }

    bfn = argv[1];

    if( argc == 3 )
    {
        basecfn = default_cfg_name;
        ofn = argv[2];
    }
    else
    {
        basecfn = argv[2];
        ofn = argv[3];
    }

    out = fopen( ofn, "w" );
    if( !out )
    {
        printf("Cannot open output file %s\n",ofn );
        return 0;
    }
    set_error_file( out );

    init_snap_globals();
    install_default_projections();
    install_default_crdsys_file();
    cfn = copy_string(find_file( basecfn, ".cfg", bfn, FF_TRYALL, "snapspec" ));

    fprintf(out,"snapspec version %s: Calculation of station orders\n",PROGRAM_VERSION);
    fprintf(out,"Run at %s\n",spec_run_time);
    fprintf(out,"SNAP binary file: %s\n",bfn);
    fprintf(out,"Spec configuration file: %s\n",cfn);

    if( cfn ) { cfg = open_config_file( cfn, '!' );}
    if( !cfn || !cfg )
    {
        fprintf(out,"Cannot open configuration file %s\n",basecfn);
        printf("snapspec: Cannot open configuration file %s\n",basecfn);
        return 0;
    }

    b = open_binary_file( bfn, BINFILE_SIGNATURE );

    if( !b ||
            reload_snap_globals( b ) != OK ||
            reload_stations( b ) != OK )
    {

        fprintf(out,"Cannot reload data from binary file %s\n", bfn);
        printf( "snapspec: Cannot reload data from binary file %s\n", bfn);
        return 0;
    }

    ra = create_relacc();

    hsdc = create_test( MAX_ORDER );
    hsdc->nmark = ra->nstn;
    hsdc->options = 0;
    hsdc->env = ra;
    ra->hsdc = hsdc;

    ra->logfile = out;
    printf("\nUsing configuration file %s\n",cfn);

    nerr = read_configuration( cfg, hsdc );
    close_config_file( cfg );

    if( nerr > 0 )
    {
        printf("\n%d errors in configuration file %s\nTest aborted\n",nerr,cfn);
        fprintf(out,"\n%d errors in configuration file %s\nTest aborted\n",nerr,cfn);
        exit(1);
    }

    if( setup_hv_mode( hvmode, hsdc, ra ) != OK )
    {
        exit(1);
    }

    if(  reload_covariances( b, ra ) != OK ||
            (reload_choleski_decomposition(b, ra) != OK &&
             (reload_relative_covariances( b, ra ) != OK ||
              check_relacc_complete( ra ) != OK )))
    {

        fprintf(out,"Cannot reload covariance data from binary file %s\n"
                "Make sure that the SNAP command file includes \"output all_relative_covariances\"\n"
                "or \"output decomposition\"\n",
                bfn );
        printf( "snapspec: Cannot reload covariance data from binary file %s\n"
                "Make sure the SNAP command file includes\n"
                "   output all_relative_covariances or\n"
                "   output decomposition\n", bfn);
        return 0;
    }

    cvrcachefile=cache_covariance_filename( bfn );
    try_reload_cached_covariance( ra, cvrcachefile );
    close_binary_file(b);

    if( ra->bltdec )
    {
        hsdc->options|= SDC_OPT_TWOPASS_CVR;
    }

    set_test_confidence( hsdc );

    if( min_order_str )
    {
        char *orderstr = min_order_str;
        int iorder;
        for( iorder = 0; iorder < hsdc->norder; iorder++ )
        {
            if( _stricmp(orderstr, hsdc->tests[iorder].scOrder ) == 0 )
            {
                min_order = iorder;
                orderstr = NULL;
                break;
            }
        }
        if( orderstr )
        {
            printf("snapspec: Invalid order parameter %s supplied to program\n",min_order_str);
            fprintf(out,"Invalid order parameter %s supplied to program\n",min_order_str);
            exit(1);
        }
    }

    if( autominorder || ra->autominorder )
    {
        int max_control_order = get_max_control_order( hsdc, ra, &max_control_str );
        if( max_control_order < -1 )
        {
            printf("snapspec: Unable to determine order of control stations\n");
            fprintf(out, "Unable to determine order of control stations\n");
            return 0;
        }

        if( max_control_order >= min_order )
        {
            min_order = max_control_order + 1;
            if( min_order < hsdc->norder )
            {
                min_order_str = hsdc->tests[min_order].scOrder;
            }
            else
            {
                // TODO: Check this is the right default order ...
                min_order = hsdc->norder;
                min_order_str = dfltOrder;
            }
        }
    }

    fprintf(out,"SNAP run time: %s\n",run_time);

    if( max_control_str )
    {
        fprintf(out,"Lowest order of control stations: %s\n",max_control_str);
    }
    if( min_order_str )
    {
        fprintf(out,"Best order permitted: %s\n",min_order_str);
    }
    if(test_apriori )
    {
        fprintf(out,"Running apriori tests\n");
        varmult = 1.0;
    }
    else
    {
        fprintf(out,"Running aposteriori tests with SEU = %.4lf\n", seu);
        varmult = seu*seu;
    }
    fprintf(out,"\n");

    if( hsdc->loglevel > 1 ) write_station_index( hsdc, ra );

    sts = run_tests( hsdc );

    copy_unused_roles_to_orders( ra );

    if( sts == STS_OK )
    {
        update_station_orders(hsdc,ra);
        if( outputcsvname )
        {
            char *csvfile=output_filename(outputcsvname,bfn,SNAPSPEC_CSV_EXT);
            write_output_csv( csvfile, ra );
            fprintf(out,"\nResults written to CSV file %s\n",csvfile);
            printf("Results written to CSV file %s\n",csvfile);
            check_free(csvfile);
        }
        else
        {
            write_results( hsdc, ra );
        }

        if( updatecrdfile )
        {
            char *crdfile=output_filename(updatecrdfile,bfn,SNAPSPEC_CRD_EXT);
            update_crdfile( crdfile );
            fprintf(out,"\nUpdated coordinate file written to %s\n",crdfile);
            printf("Updated coordinate file written to %s\n",crdfile);
            check_free(crdfile);
        }

        if( splitcrdfile ) 
        {
            char *crdfile=output_filename(updatecrdfile,bfn,"");
            write_coord_files( hsdc, ra, crdfile );
            fprintf(out,"\nSplit coordinate files written to %s...\n",crdfile);
            printf("Split coordinate files written to %s...\n",crdfile);
        }

    }
    else
    {
        fprintf(out,"\nsnapspec aborted with errors\n");
    }


    ra->logfile = NULL;
    fclose(out);

    if( use_cache && ra->bltupdated ) cache_covariance_matrix( ra, cvrcachefile );

    delete_test(hsdc);
    delete_relacc( ra );

    return 0;
}
