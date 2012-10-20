#include "snapconfig.h"

/* Code for managing a list of reference frame transformations */

/*
   $Log: rftrnadj.c,v $
   Revision 1.1  1996/01/03 22:08:33  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "rftrnadj.h"
#include "snap/rftrans.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/geodetic.h"
#include "util/dms.h"
#include "util/pi.h"

#include "snap/snapglob.h"
#include "snap/genparam.h"
#include "adjparam.h"
#include "util/geodetic.h"
#include "snap/stnadj.h"
#include "util/leastsqu.h"
#include "output.h"

static char rcsid[]="$Id: rftrnadj.c,v 1.1 1996/01/03 22:08:33 CHRIS Exp $";

/* Parameter short names and descriptive names for each
   translation parameter */

static char *geoPrmNames[7] =
{
    " X shift (m)",
    " Y shift (m)",
    " Z shift (m)",
    " rotn X (sec)",
    " rotn Y (sec)",
    " rotn Z (sec)",
    " scale (ppm)"
};
static char *topoPrmNames[7] =
{
    " E shift (m)",
    " N shift (m)",
    " U shift (m)",
    " rotn E (sec)",
    " rotn N (sec)",
    " rotn U (sec)",
    " scale (ppm)"
};

static char *grownames[] =
{
    "X translation (m)",
    "Y translation (m)",
    "Z translation (m)",
    "X rotation (arc sec)",
    "Y rotation (arc sec)",
    "Z rotation (arc sec)",
    "Scale factor (ppm)"
};
static char *trownames[] =
{
    "E translation (m)",
    "N translation (m)",
    "U translation (m)",
    "N-S tilt (arc sec)",
    "W-E tilt (arc sec)",
    "Vertical rotation (arc sec)",
    "Scale factor (ppm)"
};

static void init_rftrans_prms( rfTransformation *rf )
{
    char prmname[REFFRAMELEN + 20];
    char *prmtype;
    char **prmNames;
    int i;

    /* Define the parameters of the reference frame */

    strncpy( prmname, rf->name, REFFRAMELEN );
    prmname[REFFRAMELEN] = 0;
    prmtype = prmname + strlen(prmname);

    prmNames = rf->istopo ? topoPrmNames : geoPrmNames;

    for( i = 0; i < 7; i++ )
    {
        if( ! rf->calcPrm[i] && ! rf->prmId[i] ) continue;
        if( !rf->prmId[i] )
        {
            strcpy( prmtype, prmNames[i] );
            rf->prmId[i] = define_param( prmname, 0.0, 0 );
            flag_param_listed(rf->prmId[i]);
        }
        define_param_value( rf->prmId[i], rf->prm[i], rf->calcPrm[i] );
        if( rf->prmUsed[i] ) flag_param_used( rf->prmId[i] );
    }

    rf->calctrans = (rf->calcPrm[rfTx] || rf->calcPrm[rfTy] || rf->calcPrm[rfTx]) ? 1 : 0;
    rf->calcrot = (rf->calcPrm[rfRotx] || rf->calcPrm[rfRoty] || rf->calcPrm[rfRotx]) ? 1 : 0;
    rf->calcscale = rf->calcPrm[rfScale];

    /* Set up an origin if the scale is being calculated */

    rf->isorigin = (rf->calctrans && (rf->calcrot || rf->calcscale)) ? 1 : 0;
    if( rf->isorigin )
    {
        double origin[3];
        get_network_topocentre_xyz( net, origin );
        set_rftrans_origin( rf, origin );
    }

}

static void update_rftrans_prms( rfTransformation *rf, int get_covariance )
{
    int i, j;
    double cvr[28];
    double dummy[7];
    int rowidx[7];
    int rowno[7];
    int nprm;
    double *s, *d;

    nprm = 0;
    for( i = 0; i < 7; i++ )
    {
        rowidx[i] = 0;
        if( rf->prmId[i] )
        {
            int rn;
            rf->prm[i] = param_value( rf->prmId[i] );
            rn = param_rowno( rf->prmId[i] );
            if( !rn ) continue;
            rowidx[i] = 1;
            rowno[nprm] = rn-1;
            nprm++;
        }
    }

    /* If any rows are calculated then get the covariance matrix */

    if( nprm && get_covariance ) lsq_get_params( rowno, nprm, dummy, cvr );

    /* Copy into the reference frame covariance */

    s = cvr; d = rf->prmCvr;
    for( i = 0; i < 7; i++ ) for( j = 0; j <= i; j++, d++ )
        {
            if( get_covariance && rowidx[i] && rowidx[j] )
            {
                *d = *s++;
            }
            else
            {
                *d = 0.0;
            }
        }
}

void init_rftrans_prms_list( void )
{
    int nrf;

    for( nrf = 1; nrf <= rftrans_count(); nrf++ )
    {
        init_rftrans_prms( rftrans_from_id(nrf) );
    }
}

void update_rftrans_prms_list( int get_covariance )
{
    int nrf;

    for( nrf = 1; nrf <= rftrans_count(); nrf++ )
    {
        rfTransformation *rf;
        rf = rftrans_from_id(nrf);
        update_rftrans_prms( rf, get_covariance );
        setup_rftrans( rf);
    }
}



/* Transform the covariance matrix C to TCT' */

static void transform_rftrans( double xform[3][3], double *val, double *cvr )
{
    double fullcvr[7][7];
    double fullxfm[7][7];
    double temp[7][7];
    double vtemp[7];
    int i, j, k, ij;

    /* Make a full covariance and rotation matrix */

    for( i = 0, ij=0; i < 7; i++ ) for( j = 0; j <= i; j++, ij++ )
        {
            fullcvr[i][j] = fullcvr[j][i] = cvr[ij];
            fullxfm[i][j] = fullxfm[j][i] = 0.0;
        }
    for( i = 0; i < 3; i++ ) for ( j = 0; j < 3; j++ )
        {
            fullxfm[i][j] = xform[i][j];
            fullxfm[i+3][j+3] = xform[i][j];
        }
    fullxfm[6][6]=1.0;

    /* Premultiply by xform */

    for( i=0; i<7; i++ ) for( j=0; j<7; j++ )
        {
            double sum = 0;
            for( k = 0; k < 7; k++ ) sum += fullxfm[i][k]*fullcvr[k][j];
            temp[i][j] = sum;
        }

    /* Postmultiply and store back into cvr */

    for( i=0, ij=0; i<7; i++ ) for( j=0; j<=i; j++, ij++ )
        {
            double sum = 0;
            for( k = 0; k < 7; k++ ) sum += temp[i][k]*fullxfm[j][k];
            cvr[ij] = sum;
        }

    /* Apply the transformation to the transformation values */

    for( i=0; i<7; i++ ) vtemp[i] = val[i];
    for( i=0; i<7; i++ )
    {
        double sum = 0;
        for( k = 0; k < 7; k++ ) sum += fullxfm[i][k] * vtemp[k];
        val[i] = sum;
    }
}

static void print_rftrans_def( char *rownames[], int *row, int *identical,
                               double *val, double *cvr, double semult, int *display, FILE *out )
{
    int i, j, ii, ij;
    double se[7];

    for( i = 0, ii = 0, ij = 0; i < 7; i++, ij = ii+1, ii += i+1 )
    {
        se[i] = cvr[ii];
        if( se[i] > 0.0 ) se[i] = sqrt(se[i]);
        for( j = 0; j < i; j++, ij++ )
        {
            if( se[j] > 0.0 && se[i] > 0.0 ) cvr[ij] /= (se[i]*se[j]);
        }
        cvr[ii] = 1.0;
    }
    fprintf(out,"\n      %-30s  %10s  %10s\n","Parameter","Value  ","Error  ");
    for( i = 0; i < 7; i++ )
    {
        if( ! display[i]) continue;
        fprintf(out,"      %-30s  %10.4lf",rownames[i],val[i]);
        if( row[i] ) fprintf(out,"  %10.4lf",se[i]*semult);
        else { fprintf(out,"  %10s","-    "); }
        if( identical[i] ) fprintf( out, "  (same as %s)",param_name(identical[i]));
        fprintf(out,"\n");
    }
    fprintf(out,"\n      Correlation matrix:\n");
    for( i = 0, ij = 0; i < 7; i++ )
    {
        if( display[i]) fprintf(out,"      ");
        for( j = 0; j <= i; j++, ij++ )
        {
            if( ! display[i] || ! display[j] ) continue;
            fprintf(out, "  %8.4lf", cvr[ij] );
        }
        if( display[i]) fprintf(out,"\n" );
    }
}

static void print_rftrans( rfTransformation *rf, double semult, FILE *out )
{
    double *tval, *tcvr, *gval, *gcvr;
    int trow[7], tidentical[7], grow[7], gidentical[7], display[7];
    double sval[7], scvr[28], dval[7], dcvr[28], oshift[3];
    int *srow, *sidentical, *drow, *didentical;
    double c1, c2, c3, cmin, cmax, azmax;
    int i, k;
    tmatrix *trot;

    /* s is source system, d is destination, t = topocentric, g = geocentric */
    /* Set up the mapping between these systems */

    if( rf->istopo )
    {
        tval = sval; tcvr = scvr;
        gval = dval; gcvr = dcvr;
        srow = trow; sidentical = tidentical;
        drow = grow; didentical = gidentical;
        trot = &rf->invtoporot;
    }
    else
    {
        gval = sval; gcvr = scvr;
        tval = dval; tcvr = dcvr;
        srow = grow; sidentical = gidentical;
        drow = trow; didentical = tidentical;
        trot = &rf->toporot;
    }

    for( i = 0; i < 7; i++ )
    {
        int pn;
        dval[i] = sval[i] = rf->prm[i];
        pn = rf->prmId[i];
        srow[i] = 0;
        sidentical[i] = 0;
        if( pn ) srow[i] = param_rowno(pn);
        if( srow[i] ) sidentical[i] = identical_param( pn );
        didentical[i] = 0;
        drow[i] = srow[i] ? -1 : 0;
    }

    for( k = 0; k < 28; k++ )
    {
        dcvr[k] = scvr[k] = rf->prmCvr[k];
    }

    /* Calculate the destination system */

    transform_rftrans( *trot, dval, dcvr );

    /* Lets find out some interesting things - like the maximum tilt error */

    /* Shift at origin */
    oshift[0] = oshift[1] = oshift[2] = 0.0;
    if( rf->istrans && rf->isorigin )
    {
        oshift[0] = rf->origin[0];
        oshift[1] = rf->origin[1];
        oshift[2] = rf->origin[2];
        rftrans_correct_vector( rf->id, oshift );
        oshift[0] = gval[0] + (rf->origin[0]-oshift[0]);
        oshift[1] = gval[1] + (rf->origin[1]-oshift[1]);
        oshift[2] = gval[2] + (rf->origin[2]-oshift[2]);
    }

    /* Maximum tilt error */
    c1 = (tcvr[9] + tcvr[14])/2;
    c2 = (tcvr[9] - tcvr[14])/2;
    c3 = tcvr[13];
    cmax = c2*c2 + c3*c3;
    if( cmax > 0.0 )
    {
        azmax = 0.5 * atan2( c3, -c2 );
        while( azmax > PI ) azmax -= PI;
        while( azmax < 0.0 ) azmax += PI;
        cmax = sqrt(cmax);
        cmin = c1-cmax;
        cmax += c1;
        if( cmax > 0.0 ) cmax = sqrt(cmax);
        if( cmin > 0.0 ) cmin = sqrt(cmin);
    }
    else
    {
        cmax = c1;
        if( cmax > 0.0 ) cmax = sqrt(cmax);
        cmin = cmax;
        azmax = 0.0;
    }

    display[rfRotx] = display[rfRoty] = display[rfRotz] = 1;
    display[rfScale] = 1;
    display[rfTx] = display[rfTy] = display[rfTz] = rf->istrans;

    /* OK - now all we need to do is to print out the results... */

    fprintf(out,"\nReference frame: %s\n",rf->name );
    fprintf(out,"\n   Calculated as a %s reference frame\n",
            rf->istopo ? "topocentric" : "geocentric" );
    if( rf->isorigin )
    {
        fprintf(out,"\n   Reference point for rotation and scale (%12.3lf %12.3lf %12.3lf)\n",
                rf->origin[0], rf->origin[1], rf->origin[2] );
    }
    fprintf(out,"\n   Geocentric definition\n");
    print_rftrans_def( grownames, grow, gidentical, gval, gcvr, semult, display, out );
    fprintf(out,"\n   Topocentric definition\n");
    print_rftrans_def( trownames, trow, tidentical, tval, tcvr, semult, display, out );

    if( rf->isorigin && rf->istrans )
    {
        fprintf(out,"\n   Translation at origin (%.4lf, %.4lf, %.4lf)\n",oshift[0],oshift[1],oshift[2]);
    }

    fprintf(out,"\n   Minimum error of horizontal tilt is %.5lf arc sec\n",
            cmin * semult);
    fprintf(out,"   Maximum error of horizontal tilt is %.5lf arc sec\n",
            cmax * semult );
    fprintf(out,"   Rotation axis of maximum tilt error has azimuth %.0lf degrees\n",
            azmax * RTOD);
}


#define DS (double *)


void print_rftrans_list( FILE *out )
{
    int nrf;
    double semult;
    double topolat, topolon;
    void *latfmt, *lonfmt;

    if( rftrans_count() <= 0 ) return;

    print_section_heading( out, "REFERENCE FRAME PARAMETERS");

    fprintf(lst,"\nThe errors listed for calculated parameters are %s errors\n",
            apriori ? "apriori" : "aposteriori" );

    latfmt = create_dms_format( 3, 5, 0, NULL, NULL, NULL, "N", "S" );
    lonfmt = create_dms_format( 3, 5, 0, NULL, NULL, NULL, "E", "W" );
    get_network_topocentre( net, &topolat, &topolon );
    fprintf(out,"\nTopocentric axes are east, north, up directions at\n   ");
    fputs( dms_string( topolat* RTOD, latfmt, NULL ), out );
    fputs( "    ", out );
    fputs( dms_string( topolon* RTOD, lonfmt, NULL ), out );
    fputs( "\n", out );
    check_free( latfmt );
    check_free( lonfmt );

    semult = apriori ? 1.0 : seu;

    for( nrf = 1; nrf <= rftrans_count(); nrf++ )
    {
        print_rftrans( rftrans_from_id(nrf), semult, out );
    }
}


/* Apply the reference frame rotation to a vector difference in the local
   system to get an equivalent difference in the reference frame.  Also
   apply the same correction to the differential with respect to the
   endpoint stations (dst1, dst2), and put the differentials with respect
   to the station rows into the observation equations hA, irow (irow is
   the first of the three rows for the vector difference observation) */

void vd_rftrans_corr_vector( int nrf, double vd[3],
                             double dst1[3][3], double dst2[3][3],
                             void *hA, int irow )
{

    rfTransformation *rf;
    double dvdprm[3];
    int i, axis;

    rf = rftrans_from_id( nrf );

    /* Before we modify the vector difference, calculate the differential
       effects of rotation for summing into the normal equations.. */

    if( hA )
    {

        /* Scale factor */

        if( rf->calcscale )
        {
            double scl;
            premult3( (double *) rf->tmat, vd, dvdprm, 1 );
            scl = 1.0e-6 / ( 1 + rf->prm[rfScale] * 1.0e-6 );

            for( i = 0; i < 3; i++ )
                set_param_obseq( rf->prmId[rfScale], hA, irow+i, dvdprm[i]*scl );
        }

        /* Now the rotations */

        if( rf->calcrot )
        {
            for( axis = 0; axis < 3; axis++ )
            {
                premult3( (double *) rf->dtmatdrot[axis], vd, dvdprm, 1 );
                for( i = 0; i < 3; i++ )
                {
                    set_param_obseq( rf->prmId[rfRotx+axis], hA, irow+i, dvdprm[i] );
                }
            }
        }
    }

    /* Now modify the vector and the differentials wrt lat, long, hgt */

    premult3( (double *) rf->tmat, vd, vd, 1 );
    premult3( (double *) rf->tmat, DS dst1, DS dst1, 3 );
    premult3( (double *) rf->tmat, DS dst2, DS dst2, 3 );

}


/* Apply the reference frame rotation to an xyz point position in the local
   system to get an equivalent difference in the reference frame.  Also
   apply the same correction to the differential with respect to the
   endpoint stations (dst1, dst2), and put the differentials with respect
   to the station rows into the observation equations hA, irow (irow is
   the first of the three rows for the vector difference observation) */

void vd_rftrans_corr_point( int nrf, double vd[3],
                            double dst1[3][3], void *hA, int irow )
{

    rfTransformation *rf;
    double dvdprm[3], scl;
    int i, axis;

    rf = rftrans_from_id( nrf );

    /* Apply the origin shift */

    vecdif( vd, rf->origin, vd );

    /* Apply the translation */

    vecdif( vd, rf->trans, vd );

    /* Before we modify the position, calculate the differential
       effects of rotation for summing into the normal equations.. */

    if( hA )
    {

        /* Scale factor */

        if( rf->calcscale )
        {
            premult3( (double *) rf->tmat, vd, dvdprm, 1 );
            scl = 1.0e-6 / ( 1 + rf->prm[rfScale] * 1.0e-6 );

            for( i = 0; i < 3; i++ )
                set_param_obseq( rf->prmId[rfScale], hA, irow+i, dvdprm[i]*scl );
        }

        /* Each translation component */

        if( rf->calctrans )
        {
            for( axis = 0; axis < 3; axis++ )
            {
                dvdprm[0] = dvdprm[1] = dvdprm[2] = 0.0;
                dvdprm[axis] = -1.0;
                if( rf->istopo )
                {
                    premult3( (double *) rf->invtoporot, dvdprm, dvdprm, 1 );
                }
                premult3( (double *) rf->tmat, dvdprm, dvdprm, 1 );
                for( i = 0; i < 3; i++ )
                {
                    set_param_obseq( rf->prmId[rfTx+axis], hA, irow+i, dvdprm[i] );
                }

            }
        }

        /* Now the rotations */

        if( rf->calcrot )
        {
            for( axis = 0; axis < 3; axis++ )
            {
                premult3( (double *) rf->dtmatdrot[axis], vd, dvdprm, 1 );
                for( i = 0; i < 3; i++ )
                {
                    set_param_obseq( rf->prmId[rfRotx+axis], hA, irow+i, dvdprm[i] );
                }
            }
        }
    }

    /* Now modify the vector and the differentials wrt lat, long, hgt */

    premult3( (double *) rf->tmat, vd, vd, 1 );
    premult3( (double *) rf->tmat, DS dst1, DS dst1, 3 );

    vecadd( vd, rf->origin, vd );
}

