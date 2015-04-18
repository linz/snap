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
#include "util/dateutil.h"
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

/* Parameter short names and descriptive names for each
   translation parameter */

#define MAXPRMNAMELEN 22

static const char *geoPrmNames[] =
{
    " X shift (m)",
    " Y shift (m)",
    " Z shift (m)",
    " scale (ppm)",
    " rotn X (sec)",
    " rotn Y (sec)",
    " rotn Z (sec)",
    " X shift rate (m/yr)",
    " Y shift rate (m/yr)",
    " Z shift rate (m/yr)",
    " scale rate (ppm/yr)",
    " rotn X rate (sec/yr)",
    " rotn Y rate (sec/yr)",
    " rotn Z rate (sec/yr)"
};

static const char *IERSPrmNames[] =
{
    " X shift (mm)",
    " Y shift (mm)",
    " Z shift (mm)",
    " scale (ppb)",
    " rotn X (mas)",
    " rotn Y (mas)",
    " rotn Z (mas)",
    " X shift rate (mm/yr)",
    " Y shift rate (mm/yr)",
    " Z shift rate (mm/yr)",
    " scale rate (ppb/yr)",
    " rotn X rate (mas/yr)",
    " rotn Y rate (mas/yr)",
    " rotn Z rate (mas/yr)"
};

static const char *topoPrmNames[] =
{
    " E shift (m)",
    " N shift (m)",
    " U shift (m)",
    " scale (ppm)",
    " rotn E (sec)",
    " rotn N (sec)",
    " rotn U (sec)",
    " E shift rate (m)",
    " N shift rate (m)",
    " U shift rate (m)",
    " scale rate (ppm)",
    " rotn E rate (sec)",
    " rotn N rate (sec)",
    " rotn U rate (sec)"
};

static const char *grownames[] =
{
    "X translation (m)",
    "Y translation (m)",
    "Z translation (m)",
    "Scale factor (ppm)",
    "X rotation (arc sec)",
    "Y rotation (arc sec)",
    "Z rotation (arc sec)",
    "X translation rate (m/year)",
    "Y translation rate (m/year)",
    "Z translation rate (m/year)",
    "Scale factor rate (ppm/year)",
    "X rotation rate (arc sec/year)",
    "Y rotation rate (arc sec/year)",
    "Z rotation rate (arc sec/year)"
};

static const char *irownames[] =
{
    "X translation (mm)",
    "Y translation (mm)",
    "Z translation (mm)",
    "Scale factor (ppb)",
    "X rotation (mas)",
    "Y rotation (mas)",
    "Z rotation (mas)",
    "X translation rate (mm/year)",
    "Y translation rate (mm/year)",
    "Z translation rate (mm/year)",
    "Scale factor rate (ppb/year)",
    "X rotation rate (mas/year)",
    "Y rotation rate (mas/year)",
    "Z rotation rate (mas/year)"
};

static const char *trownames[] =
{
    "E translation (m)",
    "N translation (m)",
    "U translation (m)",
    "Scale factor (ppm)",
    "N-S tilt (arc sec)",
    "W-E tilt (arc sec)",
    "Vertical rotation (arc sec)",
    "E translation rate (m/year)",
    "N translation rate (m/year)",
    "U translation rate (m/year)",
    "Scale factor rate (ppm/year)",
    "N-S tilt rate (arc sec/year)",
    "W-E tilt rate (arc sec/year)",
    "Vertical rotation rate (arc sec/year)"
};

static double iers_mult[14] =
{ 
    1000.0,
    1000.0,
    1000.0,
    1000.0,
    -1000.0,
    -1000.0,
    -1000.0,
    1000.0,
    1000.0,
    1000.0,
    1000.0,
    -1000.0,
    -1000.0,
    -1000.0
};


static const char *valformat[] = 
{ 
    "  %10.4lf    ", 
    "  %10.4lf    ", 
    "  %10.4lf    ", 
    "  %13.7lf ",
    "  %13.7lf ",
    "  %13.7lf ",
    "  %13.7lf ",
    "  %11.5lf   ", 
    "  %11.5lf   ", 
    "  %11.5lf   ", 
    "  %14.8lf",
    "  %14.8lf",
    "  %14.8lf",
    "  %14.8lf"
};

static const char *missingstr = "      -        ";


static void init_rftrans_prms( rfTransformation *rf )
{
    char prmname[REFFRAMELEN + MAXPRMNAMELEN];
    char *prmtype;
    const char **prmNames;
    double origin[3];
    int i;

    /* Define the parameters of the reference frame */

    strncpy( prmname, rf->name, REFFRAMELEN );
    prmname[REFFRAMELEN] = 0;
    prmtype = prmname + strlen(prmname);

    prmNames = rf->istopo ? topoPrmNames : geoPrmNames;

    setup_rftrans_calcs( rf );

    for( i = 0; i < 14; i++ )
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

    /* Set up an origin unless forcing otherwise */
 
    origin[0] = origin[1] = origin[2] = 0.0;
    if( rf->localorigin )
    {
        get_network_topocentre_xyz( net, origin );
    }
    set_rftrans_origin( rf, origin );
}

static void update_rftrans_prms( rfTransformation *rf, int get_covariance )
{
    int i, j;
    double cvr[28];
    double dummy[14];
    int rowidx[14];
    int rowno[14];
    int nprm;
    double *s, *d;

    nprm = 0;
    for( i = 0; i < 14; i++ )
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
    for( i = 0; i < 14; i++ ) for( j = 0; j <= i; j++, d++ )
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
    double fullcvr[14][14];
    double fullxfm[14][14];
    double temp[14][14];
    double vtemp[14];
    int i, j, k, ij;

    /* Make a full covariance and rotation matrix */

    for( i = 0, ij=0; i < 14; i++ ) for( j = 0; j <= i; j++, ij++ )
    {
        fullcvr[i][j] = fullcvr[j][i] = cvr[ij];
        fullxfm[i][j] = fullxfm[j][i] = 0.0;
    }
    for( i = 0; i < 3; i++ ) for ( j = 0; j < 3; j++ )
    {
            fullxfm[i+rfTx][j] = xform[i][j];
            fullxfm[i+rfRotx][j+rfRotx] = xform[i][j];
            fullxfm[i+rfTxRate][j] = xform[i][j];
            fullxfm[i+rfRotxRate][j+rfRotx] = xform[i][j];
    }
    fullxfm[rfScale][rfScale]=1.0;

    /* Premultiply by xform */

    for( i=0; i<14; i++ ) for( j=0; j<14; j++ )
    {
        double sum = 0;
        for( k = 0; k < 14; k++ ) sum += fullxfm[i][k]*fullcvr[k][j];
        temp[i][j] = sum;
    }

    /* Postmultiply and store back into cvr */

    for( i=0, ij=0; i<14; i++ ) for( j=0; j<=i; j++, ij++ )
    {
        double sum = 0;
        for( k = 0; k < 14; k++ ) sum += temp[i][k]*fullxfm[j][k];
        cvr[ij] = sum;
    }

    /* Apply the transformation to the transformation values */

    for( i=0; i<14; i++ ) vtemp[i] = val[i];
    for( i=0; i<14; i++ )
    {
        double sum = 0;
        for( k = 0; k < 14; k++ ) sum += fullxfm[i][k] * vtemp[k];
        val[i] = sum;
    }
}

static void print_rftrans_def( const char *rownames[], int *row, int *identical,
                               double *val, double *cvr, double *vmult, double semult, int *display, 
                               FILE *out )
{
    int i, j, ii, ij, j0, j1;
    double se[14];
    int nval;
    int gotcvr=0;

    for( i = 0, ii = 0, ij = 0; i < 14; i++, ij = ii+1, ii += i+1 )
    {
        se[i] = cvr[ii];
        if( se[i] > 0.0 ) se[i] = sqrt(se[i]);
        for( j = 0; j < i; j++, ij++ )
        {
            if( se[j] > 0.0 && se[i] > 0.0 ) cvr[ij] /= (se[i]*se[j]);
        }
        cvr[ii] = 1.0;
    }
    fprintf(out,"\n      %-30s  %15s  %15s\n","Parameter","Value    ","Error    ");
    nval=0;
    gotcvr=0;
    for( i = 0; i < 14; i++ )
    {
        double factor = vmult ? vmult[i] : 1.0;
        if( ! display[i]) continue;
        fprintf(out,"      %-30s",rownames[i] );
        fprintf(out,valformat[i],val[i]*factor);
        if( row[i] ) 
        {
            fprintf(out,valformat[i],se[i]*semult*fabs(factor));
            gotcvr++;
        }
        else { fprintf(out,"%s",missingstr);}
        if( identical[i] ) fprintf( out, "  (same as %s)",param_name(identical[i]));
        fprintf(out,"\n");
        nval++;
    }
    nval = nval > 8 ? 0 : 7;
    if( gotcvr > 1)
    {
        fprintf(out,"\n      Correlation matrix:\n");
        for( j0 = 0; j0 < 14; j0 += 7+nval  )
        {
            j1=j0+7+nval;
            if( j0 ) fprintf(out,"\n");
            ij=(j0*(j0+1))/2;
            for( i = j0; i < 14; i++ )
            {
                int used=0;
                if( display[i]) fprintf(out,"      ");
                for( j = 0; j <= i; j++, ij++ )
                {
                    if( ! display[i] || ! display[j] ) continue;
                    if( j < j0 || j >= j1 ) continue;
                    if( ! used ) fprintf(out,"      ");
                    fprintf(out, "  %8.4lf", cvr[ij] );
                    used=1;
                }
                if( used ) fprintf(out,"\n" );
            }
        }
    }
}

static void print_rftrans( rfTransformation *rf, double semult, FILE *out )
{
    double *tval, *tcvr, *gval, *gcvr;
    int trow[14], tidentical[14], grow[14], gidentical[14], display[14];
    double sval[14], scvr[105], dval[14], dcvr[105], oshift[3];
    int *srow, *sidentical, *drow, *didentical;
    double c1, c2, c3, cmin, cmax, azmax;
    int i, k;
    int userates=rf->userates;;
    int calced;
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

    calced=0;
    for( i = 0; i < 14; i++ )
    {
        int pn;
        dval[i] = sval[i] = rf->prm[i];
        pn = rf->prmId[i];
        srow[i] = 0;
        sidentical[i] = 0;
        if( pn ) { srow[i] = param_rowno(pn); calced=1; }
        if( srow[i] ) sidentical[i] = identical_param( pn );
        didentical[i] = 0;
        drow[i] = srow[i] ? -1 : 0;
    }

    for( k = 0; k < 105; k++ )
    {
        dcvr[k] = scvr[k] = rf->prmCvr[k];
    }

    /* Calculate the destination system */

    transform_rftrans( *trot, dval, dcvr );

    /* Lets find out some interesting things - like the maximum tilt error */

    /* Shift at origin */
    oshift[0] = oshift[1] = oshift[2] = 0.0;
    if( rf->usetrans && rf->localorigin )
    {
        oshift[0] = rf->origin[0];
        oshift[1] = rf->origin[1];
        oshift[2] = rf->origin[2];
        rftrans_correct_vector( rf->id, oshift, year_as_snapdate( rf->refepoch) );
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
    display[rfTx] = display[rfTy] = display[rfTz] = rf->usetrans;

    display[rfRotxRate] = display[rfRotyRate] = display[rfRotzRate] = userates;
    display[rfScaleRate] = userates;
    display[rfTxRate] = display[rfTyRate] = display[rfTzRate] = rf->usetrans && userates;

    /* OK - now all we need to do is to print out the results... */

    fprintf(out,"\nReference frame: %s\n",rf->name );
    fprintf(out,"\n   %s as a %s reference frame\n",
            calced ? "Calculated" : "Defined",
            rf->istopo ? "topocentric" : "geocentric" );
    if( rf->localorigin )
    {
        fprintf(out,"\n   Reference point for rotation and scale (%12.3lf %12.3lf %12.3lf)\n",
                rf->origin[0], rf->origin[1], rf->origin[2] );
    }
    if( rf->userates )
    {
        double epoch_date;
        int y,m,d;
        epoch_date=year_as_snapdate(rf->refepoch);
        date_as_ymd( epoch_date, &y, &m, &d );
        fprintf(out,"\n   Reference epoch of frame %02d-%02d-%04d\n",d,m,y);
    }

    if( output_reffrm_iers )
    {
        fprintf(out,"\n   IERS definition\n");
        print_rftrans_def( irownames, grow, gidentical, gval, gcvr, iers_mult, semult, display, out );
    }

    if( output_reffrm_geo )
    {
        fprintf(out,"\n   Geocentric definition\n");
        print_rftrans_def( grownames, grow, gidentical, gval, gcvr, 0, semult, display,  out );
    }

    if( output_reffrm_topo )
    {
        fprintf(out,"\n   Topocentric definition\n");
        print_rftrans_def( trownames, trow, tidentical, tval, tcvr, 0, semult, display,  out );
    }

    if( rf->localorigin && rf->usetrans )
    {
        fprintf(out,"\n   Translation at origin (%.4lf, %.4lf, %.4lf)\n",oshift[0],oshift[1],oshift[2]);
    }
    if( ! rf->usetrans )
    {
        fprintf(out,"\n   Reference frame translations are not used in this adjustment\n");
    }

    if( output_reffrm_topo )
    {
        fprintf(out,"\n   Minimum error of horizontal tilt is %.5lf arc sec\n",
                cmin * semult);
        fprintf(out,"   Maximum error of horizontal tilt is %.5lf arc sec\n",
                cmax * semult );
        fprintf(out,"   Rotation axis of maximum tilt error has azimuth %.0lf degrees\n",
                azmax * RTOD);
    }
}


#define DS (double *)


void print_rftrans_list( FILE *out )
{
    int nrf;
    double semult;
    double topolat, topolon;
    void *latfmt, *lonfmt;

    if( rftrans_count() <= 0 ) return;
    if( ! output_reffrm_topo && ! output_reffrm_geo && ! output_reffrm_iers ) return;

    print_section_heading( out, "REFERENCE FRAME PARAMETERS");

    fprintf(lst,"\nThe errors listed for calculated parameters are %s errors\n",
            apriori ? "apriori" : "aposteriori" );

    if( output_reffrm_topo )
    {
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
    }

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

void vd_rftrans_corr_vector( int nrf, double vd[3], double date,
                             double dst1[3][3], double dst2[3][3],
                             void *hA, int irow )
{

    rfTransformation *rf;
    double dvdprm[3];
    double dmult;
    int i, axis;
    
    rf = rftrans_from_id( nrf );

    dmult=0.0;
    if( date != UNDEFINED_DATE && rf->userates )
    {
        dmult=date_as_year( date ) - rf->refepoch;
    }

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

        if( rf->userates && dmult != 0.0 )
        {

            /* Scale factor rate */

            if( rf->calcscalerate )
            {
                double scl;
                premult3( (double *) rf->tmat, vd, dvdprm, 1 );
                scl = 1.0e-6 / ( 1 + dmult * rf->prm[rfScaleRate] * 1.0e-6 );

                for( i = 0; i < 3; i++ )
                    set_param_obseq( rf->prmId[rfScaleRate], hA, irow+i, dmult*dvdprm[i]*scl );
            }

            /* Now the rotation rates */

            if( rf->calcrotrate )
            {
                for( axis = 0; axis < 3; axis++ )
                {
                    premult3( (double *) rf->dtmatdrotrate[axis], vd, dvdprm, 1 );
                    for( i = 0; i < 3; i++ )
                    {
                        set_param_obseq( rf->prmId[rfRotxRate+axis], hA, irow+i, dmult*dvdprm[i] );
                    }
                }
            }

        }
    }

    /* Now modify the vector and the differentials wrt lat, long, hgt */

    if( rf->userates )
    {
        double vr[3];
        premult3( DS rf->tmatrate, vd, vr, 1 );
        premult3( DS rf->tmat, vd, vd, 1 );
        vecadd2( vd, 1, vr, dmult, vd );
    }
    else
    {
        premult3( DS rf->tmat, vd, vd, 1 );
    }
    premult3( DS rf->tmat, DS dst1, DS dst1, 3 );
    premult3( DS rf->tmat, DS dst2, DS dst2, 3 );

}


/* Apply the reference frame rotation to an xyz point position in the local
   system to get an equivalent difference in the reference frame.  Also
   apply the same correction to the differential with respect to the
   endpoint stations (dst1, dst2), and put the differentials with respect
   to the station rows into the observation equations hA, irow (irow is
   the first of the three rows for the vector difference observation) */

void vd_rftrans_corr_point( int nrf, double vd[3], double date,
                            double dst1[3][3], void *hA, int irow )
{

    rfTransformation *rf;
    double dvdprm[3], scl;
    double dmult;
    int i, axis;

    rf = rftrans_from_id( nrf );

    dmult=0.0;
    if( date != UNDEFINED_DATE && rf->userates )
    {
        dmult=date_as_year( date ) - rf->refepoch;
    }

    /* Apply the origin shift */

    vecdif( vd, rf->origin, vd );

    /* Apply the translation */

    vecdif( vd, rf->trans, vd );
    if( dmult ) 
    {
        vecadd2( vd, 1, rf->transrate, -1.0*dmult, vd );
    }

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

        if( rf->userates && dmult != 0.0 )
        {
            /* Scale factor */

            if( rf->calcscalerate )
            {
                premult3( (double *) rf->tmat, vd, dvdprm, 1 );
                scl = 1.0e-6 / ( 1 + dmult*rf->prm[rfScale] * 1.0e-6 );

                for( i = 0; i < 3; i++ )
                    set_param_obseq( rf->prmId[rfScale], hA, irow+i, dmult*dvdprm[i]*scl );
            }

            /* Each translation component */

            if( rf->calctransrate )
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
                        set_param_obseq( rf->prmId[rfTxRate+axis], hA, irow+i, dmult*dvdprm[i] );
                    }

                }
            }

            /* Now the rotations */

            if( rf->calcrotrate )
            {
                for( axis = 0; axis < 3; axis++ )
                {
                    premult3( (double *) rf->dtmatdrotrate[axis], vd, dvdprm, 1 );
                    for( i = 0; i < 3; i++ )
                    {
                        set_param_obseq( rf->prmId[rfRotxRate+axis], hA, irow+i, dmult*dvdprm[i] );
                    }
                }
            }
        }
    }

    /* Now modify the vector and the differentials wrt lat, long, hgt */

    premult3( DS rf->tmat, vd, vd, 1 );
    if( rf->userates )
    {
        double vr[3];
        premult3( DS rf->tmatrate, vd, vr, 1 );
        vecadd2( vd, 1, vr, dmult, vd );
    }

    premult3( DS rf->tmat, DS dst1, DS dst1, 3 );

    vecadd( vd, rf->origin, vd );
}

