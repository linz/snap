#include "snapconfig.h"
/*
   $Log: relerror.c,v $
   Revision 1.8  2003/10/23 01:29:04  ccrook
   Updated to support absolute accuracy tests

   Revision 1.7  2003/05/16 01:20:41  ccrook
   Added option to store all relative covariances in binary file.

   Revision 1.6  2003/05/15 21:44:25  ccrook
   Fixing problem with sqrt of negative number resulting from rounding errors.

   Revision 1.5  1999/05/24 07:24:39  ccrook
   Fixed error in testing vertical tolerances.

   Revision 1.4  1999/05/20 12:19:40  ccrook
   Fixed up handling of aposteriori tests

   Revision 1.3  1999/05/20 10:42:28  ccrook
   Added function for testing relative accuracies of stations against
   specifications

   Revision 1.2  1998/05/21 04:02:00  ccrook
   Added support for deformation model to be applied in the adjustment.

   Revision 1.1  1996/01/03 22:04:44  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>

#include "util/binfile.h"
#include "util/leastsqu.h"
#include "util/lsobseq.h"
#include "network/network.h"
#include "stnobseq.h"
#include "reorder.h"
#include "snapdata/gpscvr.h"
#include "snap/snapglob.h"
#include "output.h"
#include "relerror.h"
#include "util/progress.h"

static char rcsid[]="$Id: relerror.c,v 1.8 2003/10/23 01:29:04 ccrook Exp $";

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
        set_station_obseq( st1, dst1[axis], hA, irow, UNKNOWN_DATE );
        set_station_obseq( st2, dst2[axis], hA, irow, UNKNOWN_DATE );
        oe_value( hA, irow+axis, dif[axis] );
        oe_covar( hA, irow, irow, 1.0 );
    }
}


static void station_vertical( int id, double vrt[3] )
{
    rot_vertical( & (stnptr(id)->rTopo), vrt );
}

static void *hA;
static VRTFUNC oldvrt;
static XFMFUNC oldxfm;
static int oldfixed;

static void init_calc_covar( void )
{
    oldvrt = gps_vrtfunc();
    oldxfm = gps_xfmfunc();
    oldfixed = gps_vertical_fixed();
    set_gps_vertical_fixed(0);
    init_gps_covariances( station_vertical, (XFMFUNC) 0 );
    hA = create_oe( nprm );
}

static void term_calc_covar( void )
{
    delete_oe( hA );
    set_gps_vertical_fixed( oldfixed );
    init_gps_covariances( oldvrt, oldxfm );
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


void dump_relative_covariances( BINARY_FILE *b, int dumpall )
{
    double relcvr[6];
    int istn, maxstn;

    init_calc_covar();
    create_section( b, "STATION_RELATIVE_COVARIANCES" );

    maxstn=number_of_stations(net);

    if( ! dumpall )
    {
        init_progress_meter( maxstn );
        for( istn=0; istn++ < maxstn; )
        {
            int iconn;
            int nconn = get_connection_count( istn );
            update_progress_meter( istn );
            if( !nconn ) continue;
            for( iconn = 0; iconn < nconn; iconn++ )
            {
                int jstn = get_connection( istn, iconn );
                if( jstn <= istn ) continue;

                calc_covar( istn, jstn, relcvr );

                fwrite( &istn, sizeof(istn), 1, b->f );
                fwrite( &jstn, sizeof(jstn), 1, b->f );
                fwrite( relcvr,sizeof(relcvr), 1, b->f );
            }
        }
        end_progress_meter();
    }
    else
    {
        long nelt = maxstn;
        long ielt = 0;
        int jstn;
        nelt = nelt * ((maxstn-1)) / 2;
        init_progress_meter( nelt );
        for( istn=1; istn++ < maxstn; )
        {
            for( jstn = 1; jstn < istn; jstn++ )
            {
                ielt++;
                if( ielt % 20 == 0 ) update_progress_meter( ielt );

                calc_covar( istn, jstn, relcvr );

                fwrite( &istn, sizeof(istn), 1, b->f );
                fwrite( &jstn, sizeof(jstn), 1, b->f );
                fwrite( relcvr,sizeof(relcvr), 1, b->f );
            }
        }
        end_progress_meter();
    }

    /* Mark the end with a station code -1 */
    istn = -1;
    fwrite( &istn, sizeof(istn), 1, b->f );

    end_section( b );


    /* Restore the gps processing options */
    term_calc_covar();
}


void test_relative_accuracy_specs( SpecDef *spec, int apriori, int *stn_testids, int listopts )
{
    int istn;
    int jstn;
    int nstns;
    int testid;
    int gothtol;
    int gotvtol;
    double covar[6];
    double emax;
    double emin;
    double azemax;
    double verr;
    double vtol;
    double htol;
    int headed;
    double veclen;
    long ntest;
    long nfailh;
    long nfailv;
    double maxhratio;
    int istnmaxh;
    int jstnmaxh;
    double maxvratio;
    int istnmaxv;
    int jstnmaxv;
    int printall = listopts & SPEC_LIST_ALL;
    int printfail = listopts & SPEC_LIST_FAIL;
    long nstntest;
    double hratio;
    double vratio;

    gothtol = spec->gothtol && (spec->htolabs > 0.0 || spec->htolppm > 0.0);
    gotvtol = spec->gotvtol && (spec->vtolabs > 0.0 || spec->vtolppm > 0.0);
    if( ! gothtol && ! gotvtol ) return;

    init_calc_covar();

    fprintf(lst, "\n------------------------\nRelative accuracy tests\n");

    nstns = number_of_stations( net );

    headed = 0;

    ntest = 0;
    nfailh = 0;
    nfailv = 0;

    istnmaxh = jstnmaxh = 0;
    maxhratio = 0.0;

    istnmaxv = jstnmaxv = 0;
    maxvratio = 0.0;

    nstntest = 0;
    testid = spec->testid;
    for( istn = 1; istn <= nstns; istn++ )
    {
        if( stn_testids[istn] & testid ) nstntest++;
    }

    init_progress_meter( (nstntest*(nstntest+1))/2 );

    for( istn = 1; istn < nstns; istn++ )
    {
        if( ! (stn_testids[istn] & testid) ) continue;
        for( jstn = istn+1; jstn <= nstns; jstn++ )
        {
            int failed = 0;
            if( ! (stn_testids[jstn] & testid) ) continue;
            ntest++;
            update_progress_meter( ntest );
            calc_covar( istn, jstn, covar );
            veclen = calc_distance( stnptr(istn), 0.0, stnptr(jstn), 0.0, NULL, NULL );
            hratio = vratio = 0.0;
            if( gothtol )
            {
                calc_error_ellipse( covar, &emax, &emin, &azemax );
                emax *= spec->htolfactor;
                if( ! apriori ) emax *= seu;
                htol = _hypot( spec->htolabs, (spec->htolppm)*veclen*1.0e-6);
                if( htol > 0.0 )
                {
                    hratio = emax/htol;
                    if( hratio > 1.0 )
                    {
                        nfailh++;
                        failed = 1;
                    }
                    if( hratio > maxhratio )
                    {
                        maxhratio = hratio;
                        istnmaxh = istn;
                        jstnmaxh = jstn;
                    }
                }
            }

            if( gotvtol )
            {
                verr = sqrt( fabs( covar[5] ));
                verr *= spec->vtolfactor;
                if( ! apriori ) verr *= seu;
                vtol = _hypot( spec->vtolabs, (spec->vtolppm)*veclen*1.0e-6);
                if( vtol > 0.0 )
                {
                    vratio = verr/vtol;
                    if( vratio > 1.0 )
                    {
                        nfailv++;
                        failed = 1;
                    }
                    if( vratio > maxvratio )
                    {
                        maxvratio = vratio;
                        istnmaxv = istn;
                        jstnmaxv = jstn;
                    }
                }
            }
            if( (failed && printfail) || printall )
            {
                if( ! headed )
                {
                    int width;
                    int w;
                    width = (stn_name_width+2)*2+9;
                    if( gothtol ) width += 12;
                    if( gotvtol ) width += 12;
                    fprintf(lst,"\nRatio of calculated error to tolerance\n\n     ");
                    for( w = 0; w < width; w++ ) fprintf(lst,"=");
                    fprintf(lst,"\n     %-*s  %-*s   Length  ",stn_name_width,"From",
                            stn_name_width,"To");
                    if( gothtol ) fprintf(lst,"  Horizontal");
                    if( gotvtol ) fprintf(lst,"   Vertical ");
                    fprintf(lst,"\n     ");
                    for( w = 0; w < width; w++ ) fprintf(lst,"=");
                    fprintf(lst,"\n\n");
                    headed = 1;
                }

                fprintf(lst,"     %-*s  %-*s  %7.0lf ",
                        stn_name_width, stnptr(istn)->Code,
                        stn_name_width, stnptr(jstn)->Code, veclen );
                if( gothtol ) fprintf(lst,"    %8.2lf",hratio);
                if( gotvtol ) fprintf(lst,"    %8.2lf",vratio);
                fprintf(lst,"\n");
            }
        }
    }

    end_progress_meter();

    /* Summarize tests for this specification */

    if( gothtol )
    {
        fprintf(lst, "\nHorizontal tolerance:\n");
        fprintf(lst, "    Stations tested:             %10ld\n",nstntest);
        fprintf(lst, "    Vectors tested:              %10ld\n",ntest);
        fprintf(lst, "    Vectors exceeding tolerance: %10ld\n",nfailh);
        if( istnmaxh )
            fprintf(lst, "    Largest error/tolerance:     %10.2lf (%s to %s)\n",
                    maxhratio, stnptr(istnmaxh)->Code, stnptr(jstnmaxh)->Code );
    }

    if( gotvtol )
    {
        fprintf(lst, "\nVertical tolerance:\n");
        fprintf(lst, "    Stations tested:             %10ld\n",nstntest);
        fprintf(lst, "    Vectors tested:              %10ld\n",ntest);
        fprintf(lst, "    Vectors exceeding tolerance: %10ld\n",nfailv);
        if( istnmaxv )
            fprintf(lst, "    Largest error/tolerance:     %10.2lf (%s to %s)\n",
                    maxvratio, stnptr(istnmaxv)->Code, stnptr(jstnmaxv)->Code );
    }

    term_calc_covar();
}
