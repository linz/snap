#include "snapconfig.h"
/*

  $Log: testspec.c,v $
  Revision 1.8  2004/03/05 02:27:41  ccrook
  Updated calculation of tests of specifications - probability distributions were
  not being calculated correctly!  How was this never spotted?

  Revision 1.7  2003/11/23 23:05:19  ccrook
  Updated iteration output to display number of stations exceeding adjustment
  tolerance

  Revision 1.6  2003/11/23 22:46:25  ccrook
  Fixed boundary condition on specification testing.

  Revision 1.5  2003/11/23 22:01:27  ccrook
  Updated specification testing to avoid testing rejected stations.

  Revision 1.4  2003/10/23 01:29:04  ccrook
  Updated to support absolute accuracy tests

  Revision 1.3  2001/05/14 18:26:17  ccrook
  Minor updates

  Revision 1.2  1999/05/20 12:20:08  ccrook
  Fixing up handling of aposteriori tests

  Revision 1.1  1999/05/20 10:40:59  ccrook
  Initial revision


*/

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "testspec.h"
#include "relerror.h"
#include "snap/stnadj.h"
#include "stnobseq.h"
#include "snap/snapglob.h"
#include "util/probfunc.h"
#include "util/chkalloc.h"
#include "output.h"
#include "util/progress.h"
#include "util/errdef.h"

static SpecDef *spechead = NULL;
static int *stn_testids = NULL;
static int ntestid = 0;

static int spec_apriori = 1;
static int listopts = SPEC_LIST_NONE;

int do_accuracy_tests = 0;

static void test_absolute_accuracy_specs( SpecDef *spec, int apriori, int *stn_testids, int listopts );


void set_spec_apriori( int isapriori )
{
    spec_apriori = isapriori;
}

void set_spec_listoption( int option )
{
    listopts = option;
}


int define_spec( char *name, double conf,
                 int goth, double habs, double hppm, double hmax,
                 int gotv, double vabs, double vppm, double vmax )
{
    SpecDef *spec;
    SpecDef **nextloc = &(spechead);

    for( spec = spechead; spec; spec = spec->next )
    {
        if( _stricmp(name,spec->name) == 0 )
        {
            return INCONSISTENT_DATA;
        }
        nextloc = &(spec->next);
    }


    spec = (SpecDef *) check_malloc( sizeof(SpecDef) + strlen(name) + 1 );
    spec->next = NULL;
    spec->name =  ((char *)(spec)) + sizeof(SpecDef);
    strcpy(spec->name,name);
    _strupr(spec->name);
    spec->confidence = conf;
    spec->htolabs = habs;
    spec->htolppm = hppm;
    spec->htolmax = hmax;
    spec->gothtol = goth;
    spec->vtolabs = vabs;
    spec->vtolppm = vppm;
    spec->vtolmax = vmax;
    spec->gotvtol = gotv;
    spec->htolfactor = 0.0;
    spec->vtolfactor = 0.0;
    spec->testid = 0;

    (*nextloc) = spec;

    return OK;
}

int get_spec_testid( char *name, int *testid )
{
    SpecDef *spec;

    for( spec = spechead; spec; spec = spec->next )
    {
        if( _stricmp(name,spec->name) == 0 ) break;
    }

    if( ! spec ) return INVALID_DATA;

    if( ! spec->testid )
    {
        if( ntestid >= sizeof(int) ) return TOO_MUCH_DATA;
        spec->testid = 1 << ntestid;
        ntestid++;
    }

    (*testid) = spec->testid;

    do_accuracy_tests = 1;

    return OK;
}

int set_station_spec_testid( int stnid, int testid, int add )
{
    int nstns;
    int istn;
    nstns = number_of_stations(net);
    if( ! stn_testids )
    {
        stn_testids = (int *) check_malloc( sizeof(int) * (nstns+1) );
        for( istn = 0; istn <= nstns; istn++ )
        {
            stn_testids[istn] = 0;
        }
    }
    if( stnid < 1 || stnid > nstns ) return INVALID_DATA;
    if( add )
    {
        stn_testids[stnid] |= testid;
    }
    else
    {
        stn_testids[stnid] &= ~testid;
    }
    return OK;
}

static void print_spec( FILE *out, SpecDef *spec )
{
    fprintf( out,"\nTesting order specifications: %s\n",spec->name);
    fprintf( out,"\nBased on %.2lf %s confidence limits\n",spec->confidence,
             spec_apriori ? "apriori" : "aposteriori" );
    if( spec->gothtol )
    {
        fprintf( out, "Horizontal accuracy: (error multiplier: %6.2lf)\n",
                 spec->htolfactor );
        if( spec->htolmax > 0 )
        {
            fprintf( out, "           Absolute: %5.1lf mm\n",
                     spec->htolmax*1000 );
        }
        if( spec->htolabs > 0 || spec->htolppm > 0 )
        {
            fprintf( out, "           Relative: %5.1lf mm  %7.3lf ppm\n",
                     spec->htolabs*1000, spec->htolppm );
        }
    }
    if( spec->gotvtol )
    {
        fprintf( out, "Vertical accuracy:   (error multiplier: %6.2lf)\n",
                 spec->vtolfactor );
        if( spec->vtolmax > 0 )
        {
            fprintf( out, "           Absolute: %5.1lf mm\n",
                     spec->vtolmax*1000 );
        }
        if( spec->vtolabs > 0 || spec->vtolppm > 0 )
        {
            fprintf( out, "           Relative: %5.1lf mm  %7.3lf ppm\n",
                     spec->vtolabs*1000, spec->vtolppm );
        }
    }
}

void test_specifications( void )
{
    int i;
    double prob;
    int nstns;
    int nrej;
    int istn;

    if( ! ntestid ) return;
    if( ! stn_testids ) return;

    print_section_header( lst, "ACCURACY SPECIFICATION TESTS" );

    /* Ensure rejected/ignored stations are not tested ... */

    nrej = 0;
    nstns = number_of_stations(net);
    for( istn = 1; istn <= nstns; istn++ )
    {
        stn_adjustment *sa = stnadj( stnptr( istn ));
        if( sa->flag.rejected || sa->flag.ignored )
        {
            stn_testids[istn] = 0;
            nrej++;
        }
    }

    if( nrej > 0 )
    {
        fprintf(lst,"\nNote: %d rejected stations not used in specification tests\n",
                (int) nrej );
    }

    for( i = 0; i<ntestid; i++ )
    {
        int testid = 1 << i;
        SpecDef *spec;
        for( spec = spechead; spec; spec = spec->next )
        {
            if( spec->testid == testid ) break;
        }
        if( ! spec ) continue;

        prob = 1-spec->confidence/100.0;

        /* Fix up specification scale factors for confidence and
           degrees of freedom */

        if( spec_apriori )
        {
            spec->vtolfactor = sqrt(fabs(inv_chi2_distn( prob, 1 )));
            spec->htolfactor = sqrt(fabs(inv_chi2_distn( prob, 2 )));
        }
        else
        {
            spec->vtolfactor = sqrt(fabs(inv_f_distn( prob, 1, dof )));
            spec->htolfactor = sqrt(fabs(inv_f_distn( prob, 2, dof )*2));
        }

        /* Head up output log for this specification */

        if( i ) fprintf( lst, "\n\n========================================"
                             "========================================\n");

        print_spec( lst, spec );

        /* Test the absolute accuracy specs for all applicable stations */

        if( spec->htolmax > 0.0 || spec->vtolmax > 0.0 )
        {
            test_absolute_accuracy_specs( spec, spec_apriori, stn_testids, listopts );
        }

        /* Test the relative accuracy specs for all applicable vectors */

        if( spec->htolabs > 0.0 || spec->htolppm > 0.0 ||
                spec->vtolabs > 0.0 || spec->vtolppm > 0.0 )
        {
            test_relative_accuracy_specs( spec, spec_apriori, stn_testids, listopts );
        }
    }
    print_section_footer( lst );

}



void test_absolute_accuracy_specs( SpecDef *spec, int apriori, int *stn_testids, int listopts )
{
    int istn;
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
    long ntest;
    long nfailh;
    long nfailv;
    double maxhratio;
    int istnmaxh;
    double maxvratio;
    int istnmaxv;
    int printall = listopts & SPEC_LIST_ALL;
    int printfail = listopts & SPEC_LIST_FAIL;
    long nstntest;
    double hratio;
    double vratio;

    gothtol = spec->gothtol && (spec->htolmax > 0.0);
    gotvtol = spec->gotvtol && (spec->vtolmax > 0.0);
    if( ! gothtol && ! gotvtol ) return;

    fprintf(lst, "\n------------------------\nAbsolute (network) accuracy tests\n");

    nstns = number_of_stations( net );

    headed = 0;

    ntest = 0;
    nfailh = 0;
    nfailv = 0;

    istnmaxh = 0;
    maxhratio = 0.0;

    istnmaxv = 0;
    maxvratio = 0.0;

    nstntest = 0;
    testid = spec->testid;
    for( istn = 1; istn <= nstns; istn++ )
    {
        if( stn_testids[istn] & testid ) nstntest++;
    }

    init_progress_meter( nstntest );

    for( istn = 1; istn <= nstns; istn++ )
    {
        int failed = 0;
        if( ! (stn_testids[istn] & testid) ) continue;
        ntest++;
        update_progress_meter( ntest );
        get_station_covariance( stnptr(istn), covar );
        hratio = vratio = 0.0;
        if( gothtol )
        {
            calc_error_ellipse( covar, &emax, &emin, &azemax );
            emax *= spec->htolfactor;
            if( ! apriori ) emax *= seu;
            htol = spec->htolmax;
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
                }
            }
        }

        if( gotvtol )
        {
            verr = sqrt( fabs( covar[5] ));
            verr *= spec->vtolfactor;
            if( ! apriori ) verr *= seu;
            vtol = spec->vtolmax;
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
                }
            }
        }
        if( (failed && printfail) || printall )
        {
            if( ! headed )
            {
                int width;
                int w;
                width = stn_name_width+2+2;
                if( gothtol ) width += 12;
                if( gotvtol ) width += 12;
                fprintf(lst,"\nRatio of calculated error to tolerance\n\n     ");
                for( w = 0; w < width; w++ ) fprintf(lst,"=");
                fprintf(lst,"\n     %-*s    ",stn_name_width,"Station");
                if( gothtol ) fprintf(lst,"  Horizontal");
                if( gotvtol ) fprintf(lst,"   Vertical ");
                fprintf(lst,"\n     ");
                for( w = 0; w < width; w++ ) fprintf(lst,"=");
                fprintf(lst,"\n\n");
                headed = 1;
            }

            fprintf(lst,"     %-*s  ",
                    stn_name_width, stnptr(istn)->Code );
            if( gothtol ) fprintf(lst,"    %8.2lf",hratio);
            if( gotvtol ) fprintf(lst,"    %8.2lf",vratio);
            fprintf(lst,"\n");
        }
    }

    end_progress_meter();

    /* Summarize tests for this specification */

    if( gothtol )
    {
        fprintf(lst, "\nHorizontal tolerance:\n");
        fprintf(lst, "    Stations tested:              %10ld\n",nstntest);
        fprintf(lst, "    Stations exceeding tolerance: %10ld\n",nfailh);
        if( istnmaxh )
            fprintf(lst, "     Largest error/tolerance:     %10.2lf (%s)\n",
                    maxhratio, stnptr(istnmaxh)->Code );
    }

    if( gotvtol )
    {
        fprintf(lst, "\nVertical tolerance:\n");
        fprintf(lst, "    Stations tested:              %10ld\n",nstntest);
        fprintf(lst, "    Stations exceeding tolerance: %10ld\n",nfailv);
        if( istnmaxv )
            fprintf(lst, "     Largest error/tolerance:     %10.2lf (%s)\n",
                    maxvratio, stnptr(istnmaxv)->Code );
    }

}
