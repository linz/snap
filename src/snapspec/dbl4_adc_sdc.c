/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
** $Id: dbl4_adc_sdc.c,v 1.1 2003/05/28 01:40:45 ccrook Exp $
**//**
** \file
**      Code for applying the SDC test algorithm to calculate coordinate
**      orders.
**
*************************************************************************
*/

#include "dbl4_common.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

#include "dbl4_adc_sdc.h"
#include "dbl4_utl_yield.h"
#include "dbl4_utl_progress.h"
#include "dbl4_utl_error.h"
#include "dbl4_utl_alloc.h"

#define SDC_STS_UNKNOWN    1  /* Mark to be assigned at current order */
#define SDC_STS_FAIL       2  /* Mark has failed at current order */
#define SDC_STS_PASS       3  /* Mark has passed at current order */
#define SDC_STS_PASSED     4  /* Marks has passed at lower order */
#define SDC_STS_SKIP       5  /* Mark is to be skipped in current test */
#define SDC_STS_NEED_CVR   6  /* Status pending passing horizontal accuracy */

/* Objects defined to implement the SDC test */

/* SDCStation is the definition of the accuracy test information about
   a station */

typedef struct
{
    char role;       /**< Role of the station in the test */
    char status;     /**< Status of the station (one of SDC_STS macro values) */
    int nrelrow;     /**< Row number used for relative accuracy testing */
    int nreltest;    /**< Number of relative accuracy tests applying */
    int nrelbad;     /**< Number of relative accuracy tests failed */
    int nrelfail;    /**< Number of rel.acc. tests to passed nodes that failed */
    int passtest;    /**< The test in which then station passed, used to reset for twopass */
    float error2;    /**< Square of semi-major axis of error ellipse */
    float verror2;   /**< Square of the vertical error */
    float ctldist2;  /**< Square of distance to nearest control (fixed stn) */
} SDCStation, *hSDCStation;

/* SDCLine is the information required for relative accuracy tests */

typedef struct
{
    float distance;  /**< Square of the length of the line */
    float error;     /**< Square of semi-major of rel err ellipse -
                        or SDC_COVAR_UNAVAILABLE if not yet computed */
} SDCLine, *hSDCLine;

/* SDCTestImp carries all the information for the relative accuracy test */

typedef struct RABlock_s
{
    int size;             /**< Size of block */
    int alloc;            /**< Allocated from block */
    unsigned char *data;  /**< Block memory */
    struct RABlock_s *next;
} RABlock, *hRABlock;

typedef struct
{
    hSDCTest sdc;      /**< The definition of the tests to apply */
    hSDCStation stns;  /**< The list of stations to apply the tests to */
    unsigned char **relstatus; /**< Array of arrays of status (forming lower triangle array) */
    int *relcol0;              /**< Array of first used column of status array */
    RABlock *relalloc;        /**< An linked list of relative accuracy array allocations */
    RABlock *curalloc;        /**< Current being used relative accuracy array allocation */
    int allocsize;             /**< Size used for each new block allocation */
    int *lookup;       /**< Lookup from relative accuracy order to station order */
    float maxctldist2; /**< The maximum distance to nearest control squared */
    float maxerror2;   /**< The maximum squared semi-major axis of the error ellipse */
    float maxverror2;  /**< The maximum squared semi-major axis of the error ellipse */
    int nreltest;      /**< Number of relative accuracy marks */
    int testhor;       /**< True if testing horizontal errors */
    int testvrt;       /**< True if testing vertical errors */
    int loglevel;      /**< The log level to use */
    int order;         /**< The current order being run */
    int phase;         /**< The current phase of adjustment for a two phase implementation */
    int phase1;        /**< The first phase to run */
    int phase2;        /**< The last phase to run */
    int twopass;       /**< Non zero if using two pass calculation */
    int needphase2;    /**< Set to true if not all required data found in phase1*/
    char logbuffer[1024]; /**< Buffer used for writing log messages */
    clock_t reftime;   /**< Reference time for timestamps */
    clock_t lasttime;  /**< Time of last timestamp */
} SDCTestImp, *hSDCTestImp;

#define SDCI_PHASE_TRIAL 1
#define SDCI_PHASE_CALC  2

/* Relative accuracy information is stored in a lower triangle format for
   all marks for which it is required.  The leading diagonal is omitted.
   Status information for the tests is SDC_STS_PASS, SDC_STS_FAIL,
   SDC_STS_SKIP, or STS_NEED_CVR.  Mapping between rows in relative accuracy array and
   input station ids is defined by SDCStation.nrelrow and SDCTestImp.lookup.
   */

/* ABORT_FREQUENCY controls the granularity of yields to the system */
/* The frequency is actually inversely proportional to this number */

#define ABORT_FREQUENCY 1000
#define DEFAULT_RAMEM_SIZE (1024*1024*8)

#define SDC_TEST_DCTL "DCTL" /*     Distance to control */
#define SDC_TEST_HAA  "HAA"  /*     Horizontal absolute accuracy (F) */
#define SDC_TEST_HDAA "HDAA" /*     Horizontal distance dependent accuracy (F) */
#define SDC_TEST_HRAA "HRAA" /*     Horizontal rel acc from absolute (P - may need vertical pass also) */
#define SDC_TEST_HRAC "HRAC" /*     Horizontal relative accuracy pass/fail by calculated value (P/F) */
#define SDC_TEST_HRAD "HRAD" /*     Horizontal relative accuracy between stations passed by max distance test (P) */
#define SDC_TEST_HRAS "HRAS" /*     Horizontal relative accuracy pass/fail by approx calcs using abs accuracy (P/F) */
#define SDC_TEST_HRAP "HRAP" /*     Horizontal relative accuracy failed to passed station (F) */
#define SDC_TEST_RA   "RA"   /*     Actual relative accuracy tests (P/F v1=no RA tests, v2=no RA fails) */
#define SDC_TEST_RAA  "RAA"  /*     Relative accuracy passed by absolute (P) */
#define SDC_TEST_RAC  "RAC"  /*     Too few RA tests (F) */
#define SDC_TEST_RAIS "RAIS" /*     Relative accuracy test initial status (s2=status) */
#define SDC_TEST_RAS  "RAS"  /*     Selected to fail RA tests (F, v1=maxfailratio,v2=maxfailerror) */
#define SDC_TEST_REQC "REQC" /*     Relative covariance requested */
#define SDC_TEST_VAA  "VAA"  /*     Vertical absolute accuracy (F) */
#define SDC_TEST_VDAA "VDAA" /*     Vertical distance dependent absolute accuracy (F) */
#define SDC_TEST_VRAA "VRAA" /*     Vertical rel acc from absolute (P - may need horizontal pass also) */
#define SDC_TEST_VRAC "VRAC" /*     Vertical relative accuracy pass/fail by calculated value (P/F) */
#define SDC_TEST_VRAD "VRAD" /*     Vertical relative accuracy between stations passed by max distance test (P) */
#define SDC_TEST_VRAS "VRAS" /*     Vertical relative accuracy pass/fail by approx calcs using abs accuracy (P/F) */
#define SDC_TEST_VRAP "VRAP" /*     Vertical relative accuracy failed to passed station (F) */

static void sdcInitTestImp( hSDCTestImp sdci, hSDCTest sdc );
static void sdcReleaseTestImp( hSDCTestImp sdci );
static StatusType sdcLoadSDCStations( hSDCTestImp sdci );
static StatusType sdcFindStationsForTest( hSDCTestImp sdci, int ntest, int *nleft );
static StatusType sdcFindNearestControl( hSDCTestImp sdci );
static StatusType sdcApplyAbsAccuracy( hSDCTestImp sdci, hSDCOrderTest test,
                                       int *nleft );
static StatusType sdcCreateRelTest( hSDCTestImp sdci );
static void sdcRAInitAllocRow( hSDCTestImp sdci );
static unsigned char *sdcRAAllocRow( hSDCTestImp sdci, int row, int col0 );
static StatusType sdcSetupRelAccuracyStatus( hSDCTestImp sdci, hSDCOrderTest test );
static StatusType sdcApplyRelTest( hSDCTestImp sdci, hSDCOrderTest test );
static StatusType sdcApplyRelTestPass( hSDCTestImp sdci, int *pnpass );
static StatusType sdcApplyRelTestFail( hSDCTestImp sdci, hSDCOrderTest test,
                                       int *pnfailacc, int *pnfailcount );
static StatusType sdcSeekRelTestFail( hSDCTestImp sdci, hSDCOrderTest test, int *pfailed );
static void sdcSetRelTestStatus( hSDCTestImp sdci, int istn, char status );
static StatusType sdcUpdateOrders( hSDCTestImp sdci, int itest, int apply );
static StatusType sdcApplyDefaultOrder( hSDCTestImp sdci );
static void sdcWriteLog( hSDCTestImp sdci, int level, const char *fmt, ... );
static void sdcWriteCompactLogHeader( hSDCTestImp sdci );
static void sdcWriteCompactLog( hSDCTestImp sdci, long stn1, long stn2,
                                const char *test, const char *status, double v1, double v2, const char *comment );
static long sdcStationId( hSDCTestImp sdci, int istn );
static void sdcTimeStamp( hSDCTestImp sdci, const char *status );

/*************************************************************************
** Function name: sdcCreateSDCTest
**//**
**    Routine allocates memory for an SDC test object
**
**  \param maxorder            The maximum number of orders in the
**                             test.
**
**  \return                    Pointer to the object created.
**
**************************************************************************
*/

hSDCTest sdcCreateSDCTest( int maxorder )
{
    hSDCOrderTest tests, test;
    hSDCTest sdc;
    int i;

    sdc = (hSDCTest) utlAlloc( sizeof(SDCTest) );
    tests = (hSDCOrderTest) utlAlloc( maxorder * sizeof(SDCOrderTest) );

    sdc->env = NULL;
    sdc->nmark = 0;
    sdc->norder = 0;
    sdc->maxorder = maxorder;
    sdc->loglevel = 0;
    sdc->options = 0;
    sdc->tests = tests;
    sdc->idFailOrder = 0;
    sdc->dblErrFactor = 3.0;
    sdc->scFailOrder[0] = 0;

    sdc->pfStationId = NULL;
    sdc->pfStationRole = NULL;
    sdc->pfStationPriority = NULL;
    sdc->pfDistance2 = NULL;
    sdc->pfError2 = NULL;
    sdc->pfVrtError2 = NULL;
    sdc->pfRequestCovar = NULL;
    sdc->pfCalcRequested = NULL;
    sdc->pfSetOrder = NULL;
    sdc->pfWriteLog = NULL;
    sdc->pfWriteCompact = NULL;

    for( i = 0; i < maxorder; i++ )
    {
        test = &(tests[i]);
        test->blnTestHor = BLN_TRUE;
        test->blnTestVrt = BLN_FALSE;
        test->dblMaxCtlDistFactor = 0.0;
        test->nCtlForDistance =0;
        test->dblCtlDistFactor=0.0;
        test->nHigherForDistance=0;
        test->dblHigherDistFactor=0.0;
        test->dblRange = 0.0;
        test->iMinRelAcc = 0;
        test->dblAbsTestAbsMax = 1000.0;
        test->dblAbsTestDDMax  = 1000.0;
        test->dblAbsTestDFMax  = 1000.0;
        test->dblRelTestAbsMin = 0.0;
        test->dblRelTestDFMax  = 1000.0;
        test->dblRelTestDDMax  = 0.0;
        test->dblAbsTestAbsMaxV = 1000.0;
        test->dblAbsTestDDMaxV  = 1000.0;
        test->dblAbsTestDFMaxV  = 1000.0;
        test->dblRelTestAbsMinV = 0.0;
        test->dblRelTestDFMaxV  = 1000.0;
        test->dblRelTestDDMaxV  = 0.0;
    }

    return sdc;
}

/*************************************************************************
** Function name: sdcDropSDCTest
**//**
**    Release the resources alloced to the SDCTest object
**
**  \param sdc                 The definition of the test to drop
**
**  \return
**
**************************************************************************
*/

void sdcDropSDCTest( hSDCTest sdc )
{
    if( sdc )
    {
        if( sdc->tests ) utlFree( sdc->tests );
        utlFree( sdc );
    }
}

/*************************************************************************
** Function name: sdcCalcSDCOrders
**//**
**    Routine calculates orders of stations using the "SDC algorithm",
**    which tries orders sequentially to find an order that matches the
**    absolute and relative accuracy requirements.
**
**  \param sdc                 The definition of the test to apply
**
**  \return
**
**************************************************************************
*/

StatusType sdcCalcSDCOrders( hSDCTest sdc )
{
    return sdcCalcSDCOrders2( sdc, 0 );
}

StatusType sdcCalcSDCOrders2( hSDCTest sdc, int minorder)
{
    SDCTestImp sdci;
    int order;
    int phaseminorder;
    StatusType sts;

    sts = STS_OK;

    /*> Check that input data is valid */

    if ( sdc->norder <= 0 ) THROW_EXCEPTION(("SDC test called with less than 1 order to test"));
    if ( sdc->nmark <= 0 ) THROW_EXCEPTION(("SDC test called with less than 1 mark to test"));

    /*> Initiallize the SDCTestImp object */

    sdcInitTestImp( &sdci, sdc );
    sdci.sdc = sdc;
    sdci.reftime = sdci.lasttime = clock();

    sdcWriteLog( &sdci, SDC_LOG_STEPS, "Initiallizing SDC tests\n" );
    sdcTimeStamp(&sdci,"Initiallizing SDC tests");
    sdcWriteCompactLogHeader( &sdci );

    /*> Initiallize the list of sdc stations */

    sdcWriteLog( &sdci, SDC_LOG_STEPS, "Loading marks for SDC tests\n" );
    sts = sdcLoadSDCStations( &sdci );
    sdcTimeStamp(&sdci,"Stations loaded");

    /*> Find the nearest control to each mark */

    if( sts == STS_OK )
    {
        sdcWriteLog( &sdci, SDC_LOG_STEPS, "Finding nearest control for each mark\n" );
        sts = sdcFindNearestControl( &sdci );
        sdcTimeStamp(&sdci,"Identified nearest control marks");
    }

    phaseminorder=minorder;
    for( sdci.phase=sdci.phase1; sts == STS_OK && sdci.phase <= sdci.phase2; sdci.phase++ )
    {

        if( sdci.twopass )
        {
            if( sdci.phase==SDCI_PHASE_TRIAL )
            {
                sdcWriteLog( &sdci, SDC_LOG_CALCS | SDC_LOG_CALCS2, "\nPass 1: Using existing covariance information\n");
            }
            else if( sdci.phase==SDCI_PHASE_CALC )
            {
                sdcTimeStamp(&sdci,"Calculating missing covariances");
                if( ! sdc->pfCalcRequested( sdc->env ) ) sts=STS_INVALID_DATA;
                sdcTimeStamp(&sdci,"Calculation of missing covariances complete");
                if( sts != STS_OK )
                {
                    sdcWriteLog( &sdci, SDC_LOG_STEPS, "Calculation of missing covariances failed" );
                }
                sdcWriteLog( &sdci, SDC_LOG_CALCS | SDC_LOG_CALCS2, "\nPass 2: Calculating with extra covariance information\n");
            }

            /* Reset status for stations which passed in tests which are being redone */
            {
                hSDCStation stns = sdci.stns;
                for( int i = 0; sts == STS_OK && i < sdc->nmark; i++ )
                {
                    hSDCStation s = & (stns[i]);
                    if( s->status == SDC_STS_PASSED && s->passtest >= phaseminorder )
                    {
                        s->status = SDC_STS_SKIP;
                        s->passtest = -1;
                    }
                    else if ( s->status == SDC_STS_UNKNOWN )
                    {
                        s->status = SDC_STS_SKIP;
                    }
                }
            }
        }

        /*> For each order in turn... */

        sdci.needphase2=0;
        for( order = phaseminorder; sts == STS_OK && order < sdc->norder; order++ )
        {
            int nunknown;
            hSDCOrderTest test = &(sdc->tests[order]);
            if( ! test->blnTestHor && ! test->blnTestVrt ) continue;
            sdci.order=order;

            {
                char buf[80];
                sprintf(buf,"SDC tests for order %.4s",test->scOrder);
                sts = utlShowProgress( buf, PROG_NO_BAR );
                if( sts != STS_OK ) break;
                sprintf(buf,"Commencing tests for order %.4s",test->scOrder);
                sdcTimeStamp(&sdci,buf);
            }

            sdcWriteLog( &sdci, SDC_LOG_STEPS,
                         "=============================================================="
                         "\nRunning test for order %.4s\n", test->scOrder );
            if( test->blnTestHor )
            {
                if( test->blnTestVrt )
                {
                    sdcWriteLog(&sdci,SDC_LOG_STEPS,
                                " Horizontal accuracy tests\n");
                }
                sdcWriteLog( &sdci, SDC_LOG_STEPS,
                             "   Absolute accuracy maximum = %.3lf m\n", test->dblAbsTestAbsMax );
                sdcWriteLog( &sdci, SDC_LOG_STEPS,
                             "   Absolute accuracy maximum to nearest control = %.3lf m +/- %.4lf m/100m\n",
                             test->dblAbsTestDFMax, test->dblAbsTestDDMax );
                sdcWriteLog( &sdci, SDC_LOG_STEPS,
                             "   Relative accuracy by abs accuracy = %.3lf m\n",
                             test->dblRelTestAbsMin );
                sdcWriteLog( &sdci, SDC_LOG_STEPS,
                             "   Maximum range for relative accuracy test = %.1lf m\n",
                             test->dblRange);
                if( test->dblMaxCtlDistFactor > 0  )
                    sdcWriteLog( &sdci, SDC_LOG_STEPS, "   Max times max dist to control to test : %.1lf",
                                 test->dblMaxCtlDistFactor );
                sdcWriteLog( &sdci, SDC_LOG_STEPS,
                             "   Relative accuracy maximum = %.3lf m +/- %.4lf m/100m\n",
                             test->dblRelTestDFMax, test->dblRelTestDDMax );
            }
            if( test->blnTestVrt )
            {
                sdcWriteLog(&sdci,SDC_LOG_STEPS,
                            " Vertical accuracy tests\n");
                sdcWriteLog( &sdci, SDC_LOG_STEPS,
                             "   Absolute accuracy maximum = %.3lf m\n", test->dblAbsTestAbsMaxV );
                sdcWriteLog( &sdci, SDC_LOG_STEPS,
                             "   Absolute accuracy maximum to nearest control = %.3lf m +/- %.4lf m/100m\n",
                             test->dblAbsTestDFMax, test->dblAbsTestDDMaxV );
                sdcWriteLog( &sdci, SDC_LOG_STEPS,
                             "   Relative accuracy by abs accuracy = %.3lf m\n",
                             test->dblRelTestAbsMinV );
                sdcWriteLog( &sdci, SDC_LOG_STEPS,
                             "   Maximum range for relative accuracy test = %.1lf m\n",
                             test->dblRange);
                if( test->dblMaxCtlDistFactor > 0  )
                    sdcWriteLog( &sdci, SDC_LOG_STEPS, "   Max times max dist to control to test : %.1lf",
                                 test->dblMaxCtlDistFactor );
                sdcWriteLog( &sdci, SDC_LOG_STEPS,
                             "   Relative accuracy maximum = %.3lf m +/- %.4lf m/100m\n",
                             test->dblRelTestDFMaxV, test->dblRelTestDDMaxV );
            }

            /*>> Set the status of any stations for which this test becomes
                 valid to SDC_STS_UNKNOWN */

            sts = sdcFindStationsForTest( &sdci, order, &nunknown  );
            if( sts != STS_OK ) break;
            if( nunknown == 0 ) continue;

            /*>> Apply the absolute tests and relative by absolute test */

            sdcWriteLog( &sdci, SDC_LOG_STEPS, "Applying absolute accuracy test\n");
            sts = utlShowProgress("Applying absolute accuracy tests",PROG_TEMP_MSG);
            if( sts != STS_OK ) break;
            sts = sdcApplyAbsAccuracy( &sdci, test, &nunknown );
            sdcTimeStamp(&sdci,"Absolute accuracy test complete");
            if( sts != STS_OK ) break;

            /*>> If there are any marks with status still unknown ... */

            if( nunknown > 0 )
            {
                /*>>> If the relative accuracy test array is not yet constructed,
                      then build it */
                if( ! sdci.relstatus )
                {
                    sdcWriteLog( &sdci, SDC_LOG_STEPS,
                                 "Setting up array for relative accuracy tests\n" );
                    sts = sdcCreateRelTest( &sdci );
                    sdcTimeStamp(&sdci,"Relative accuracy tests initiated");
                    if( sts != STS_OK ) break;
                }

                /*>>> Apply the relative accuracy tests */

                sdcWriteLog( &sdci, SDC_LOG_STEPS, "Applying relative accuracy test\n");
                sts = utlShowProgress( "Applying relative accuracy tests",PROG_TEMP_MSG);
                sts = sdcApplyRelTest( &sdci, test );
                if( sts == STS_MISSING_DATA && sdci.phase == SDCI_PHASE_TRIAL )
                {
                    sdci.needphase2 = 1;
                    sts = STS_OK;
                }
                sdcTimeStamp(&sdci,"Relative accuracy tests completed");
                if( sts != STS_OK ) break;
            }

            /*>> Apply the order to passed nodes */

            if( ! sdci.needphase2 )
            {
                phaseminorder=order+1;
                sdcWriteLog( &sdci, SDC_LOG_STEPS, "Setting node orders\n");
            }
            sts = sdcUpdateOrders( &sdci, order, ! sdci.needphase2 );
            sdcTimeStamp(&sdci,"Orders updated");
            if( sts != STS_OK ) break;

            /*>> Yield to the system and check for user abort */

            sts = utlCheckAbort();
        }

        /*>> If don't need another pass then exit */
        if( ! sdci.needphase2 ) break;
    }

    sdcWriteLog( &sdci, SDC_LOG_STEPS,
                 "=============================================================="
                 "\n" );

    /*> Apply the default order */

    if( sts == STS_OK )
    {
        int nunknown;
        sts = sdcFindStationsForTest( &sdci, sdc->norder, &nunknown  );
        if( nunknown > 0 )
        {
            sdcWriteLog( &sdci, SDC_LOG_STEPS, "Applying default order to remaining nodes\n");
            sts = sdcApplyDefaultOrder( &sdci );
            sdcTimeStamp(&sdci,"Default order applied to remaining nodes");
            sdcWriteLog( &sdci, SDC_LOG_STEPS,
                         "=============================================================="
                         "\n" );
        }

    }

    /*> Release the list of SDC stations */

    sdcReleaseTestImp( &sdci );

    return sts;
}


/*************************************************************************
** Function name: sdcInitTestImp
**//**
**    Initiallizes the SDCTestImp structure - simply associates the
**    test definition with it and sets the pointers to NULL.
**
**  \param sdci                The implementation object
**  \param sdc                 The test definition object
**
**  \return
**
**************************************************************************
*/

static void sdcInitTestImp( hSDCTestImp sdci, hSDCTest sdc)
{
    int i;

    sdci->sdc = sdc;
    sdci->stns = NULL;
    sdci->relstatus = NULL;
    sdci->relcol0 = NULL;
    sdci->relalloc = NULL;
    sdci->allocsize = DEFAULT_RAMEM_SIZE;
    sdci->lookup = NULL;
    sdci->maxctldist2 = 0.0;
    sdci->maxerror2 = 0.0;
    sdci->maxverror2 = 0.0;
    sdci->loglevel = sdc->loglevel;
    if( ! sdc->pfWriteLog ) sdci->loglevel &= SDC_LOG_COMPACT;
    if( ! sdc->pfWriteCompact ) sdci->loglevel &= ~SDC_LOG_COMPACT;
    if( sdc->options & SDC_OPT_TWOPASS_CVR && sdc->pfRequestCovar && sdc->pfCalcRequested )
    {
        sdci->twopass=1;
        sdci->phase1=SDCI_PHASE_TRIAL;
    }
    else
    {
        sdci->twopass=0;
        sdci->phase1=SDCI_PHASE_CALC;
    }
    sdci->phase2=SDCI_PHASE_CALC;
    sdci->phase=0;
    sdci->order=0;
    sdci->needphase2=0;

    sdci->testhor = 0;
    sdci->testvrt = 0;
    for(i = 0; i < sdc->norder; i++ )
    {
        if( sdc->tests[i].blnTestHor )
        {
            sdci->testhor = 1;
        }
        if( sdc->tests[i].blnTestVrt )
        {
            sdci->testvrt = 1;;
        }
    }
}



/*************************************************************************
** Function name: sdcReleaseTestImp
**//**
**    Releases the resources assigned to the SDCTestImp object.
**
**  \param sdci                The object from which to release
**                             resources.
**
**  \return
**
**************************************************************************
*/

static void sdcReleaseTestImp( hSDCTestImp sdci)
{
    /*> Release the memory allocated to the stations */
    if( sdci->stns )
    {
        utlFree( sdci->stns );
        sdci->stns = NULL;
    }
    if( sdci->relstatus )
    {
        utlFree( sdci->relstatus );
        sdci->relstatus = NULL;
    }
    if( sdci->relcol0 )
    {
        utlFree( sdci->relcol0 );
        sdci->relstatus = NULL;
    }
    while( sdci->relalloc )
    {
        hRABlock alloc=sdci->relalloc;
        sdci->relalloc=alloc->next;
        utlFree(alloc->data);
        utlFree(alloc);
    }
    sdci->curalloc=0;
    if( sdci->lookup )
    {
        utlFree( sdci->lookup );
        sdci->lookup = NULL;
    }
}

/*************************************************************************
** Function name: sdcRAInitAllocRow
**//**
**    Releases the resources assigned to the SDCTestImp object.
**
**  \param sdci                The object from which to release
**                             resources.
**
**  \return
**
**************************************************************************
*/

static void sdcRAInitAllocRow( hSDCTestImp sdci )
{
    int nrow=sdci->nreltest;
    if( nrow < 2 ) return;
    if( ! sdci->relstatus )
    {
        sdci->relstatus=(unsigned char **) utlAlloc(nrow*sizeof(unsigned char *));
    }
    if( ! sdci->relcol0 )
    {
        sdci->relcol0=(int *) utlAlloc(nrow*sizeof(int));
    }
    for( int i=0; i<nrow; i++ )
    {
        sdci->relstatus[i]=0;
        sdci->relcol0[i]=0;
    }
    if( sdci->relalloc )
    {
        for( hRABlock alloc=sdci->relalloc; alloc; alloc=alloc->next )
        {
            alloc->alloc=0;
        }
    }
    sdci->curalloc=sdci->relalloc;
}

/*************************************************************************
** Function name: sdcRAAllocRow
**//**
**    Releases the resources assigned to the SDCTestImp object.
**
**  \param sdci                The SDCI object
**  \param row                 The row to allocate
**  \param col0                The first column to allocate for the row
**
**  \return                    Pointer to the status column for the row, or
**                             null if error encountered
**
**************************************************************************
*/

static unsigned char *sdcRAAllocRow( hSDCTestImp sdci, int row, int col0 )
{
    unsigned char *rowstatus;
    int nalloc;
    hRABlock alloc;
    hRABlock newalloc;

    if( sdci->relstatus[row] ) return 0;
    nalloc=row-col0+1;

    alloc=sdci->curalloc;
    while( 1 )
    {
        if( alloc )
        {
            if( alloc->alloc+nalloc <= alloc->size ) break;
            if( alloc->next ) {
                alloc=alloc->next;
                continue;
            }
        }
        if( nalloc*64 > sdci->allocsize ) sdci->allocsize=nalloc*64;
        newalloc=(hRABlock) utlAlloc( sizeof(RABlock) );
        newalloc->data=(unsigned char *) utlAlloc(sdci->allocsize);
        newalloc->size=sdci->allocsize;
        newalloc->alloc=0;
        newalloc->next=0;
        if( alloc ) {
            alloc->next=newalloc;
        }
        else {
            sdci->relalloc=newalloc;
        }
        alloc=newalloc;
        break;
    }
    sdci->curalloc=alloc;
    rowstatus=alloc->data+alloc->alloc;
    alloc->alloc += nalloc;
    sdci->relstatus[row]=rowstatus;
    sdci->relcol0[row]=col0;
    return rowstatus;
}


/*************************************************************************
** Function name: sdcLoadSDCStations
**//**
**    Creates and initiallizes an array of SDCStation objects in the
**    test implementation SDCTestImp
**
**  \param sdci                The test implementation
**
**  \return                    Used for abort status
**
**************************************************************************
*/

static StatusType sdcLoadSDCStations( hSDCTestImp sdci)
{
    hSDCTest sdc = sdci->sdc;
    StatusType sts;
    int i;

    /*> Allocate an array of SDCStation objects */
    sdci->stns = (hSDCStation) utlAlloc( sdc->nmark * sizeof(SDCStation) );

    /*> Initiallize each with the role, status, and max error ellipse */
    /*> Only calculate the ellipse semi-major if it will be required for
        testing */

    sts = STS_OK;
    for( i = 0; sts == STS_OK && i < sdc->nmark; i++ )
    {
        hSDCStation s = & (sdci->stns[i]);
        s->role = (sdc->pfStationRole)( sdc->env, i );
        s->error2 = 0.0;
        s->verror2 = 0.0;
        if( s->role == SDC_IGNORE_MARK )
        {
            s->status = SDC_STS_SKIP;
            sdcWriteLog( sdci, SDC_LOG_CALCS,
                         "    Station %ld is ignored\n",sdcStationId(sdci,i));
        }
        else if ( s->role == SDC_CONTROL_MARK )
        {
            s->status = SDC_STS_PASSED;
            sdcWriteLog( sdci, SDC_LOG_CALCS,
                         "    Station %ld is a control mark\n",sdcStationId(sdci,i));
        }
        else
        {
            s->status = SDC_STS_SKIP;
            if( sdci->testhor )
            {
                s->error2 = (sdc->pfError2)( sdc->env, i, i );
                if( s->error2 > sdci->maxerror2 ) sdci->maxerror2 = s->error2;
            }
            if( sdci->testvrt )
            {
                s->verror2 = (sdc->pfVrtError2)( sdc->env, i, i );
                if( s->verror2 > sdci->maxverror2 ) sdci->maxverror2 = s->verror2;
            }
            sdcWriteLog( sdci, SDC_LOG_CALCS,
                         "    Station %ld is to be tested. Error H/V = %.8lf/%.8lf m2\n",
                         sdcStationId(sdci,i),(double) (s->error2), (double) (s->verror2) );
        }
        s->nrelrow = 0;
        s->nreltest = 0;
        s->nrelbad = 0;
        s->nrelfail = 0;
        s->passtest = -1;

        /*> Yield and check for user abort */

        if( i % ABORT_FREQUENCY == 0 ) sts = utlCheckAbort();
    }

    return sts;
}


/*************************************************************************
** Function name: sdcFindStationsForTest
**//**
**    Adds stations to the test which apply to the current order, but
**    which have been skipped in previous orders.
**
**  \param sdci                The test implementation
**  \param ntest               The order being tested
**  \param nleft               The number of stations to be tested
**
**  \return                    The abort status
**
**************************************************************************
*/

static StatusType sdcFindStationsForTest( hSDCTestImp sdci, int ntest, int *nleft )
{
    hSDCTest sdc = sdci->sdc;
    hSDCStation stns = sdci->stns;
    StatusType sts = STS_OK;
    int i;
    int nunknown = 0;

    /*> Seek all stations for which the status is SDC_STS_SKIP, and if their
        role is set to the test number (ie the apply for this test onwards,
        then set the status to SDC_STS_UNKNOWN */


    for( i = 0; sts == STS_OK && i < sdc->nmark; i++ )
    {
        hSDCStation s = & (stns[i]);
        if( i % ABORT_FREQUENCY == 0 ) sts = utlCheckAbort();
        if( s->role == SDC_IGNORE_MARK || s->role == SDC_CONTROL_MARK ) continue;
        if( s->status == SDC_STS_SKIP && s->role <= ntest )
        {
            s->status = SDC_STS_UNKNOWN;
        }
        if( s->status == SDC_STS_UNKNOWN ) nunknown++;
    }

    if( nleft ) *nleft = nunknown;

    return sts;
}


/*************************************************************************
** Function name: sdcFindNearestControl
**//**
**    Finds the nearest fixed point (ie control) to each station being
**    tested.
**
**  \param sdci                The test implementation
**
**  \return                    returns abort status
**
**************************************************************************
*/

static StatusType sdcFindNearestControl( hSDCTestImp sdci)
{
    hSDCTest sdc = sdci->sdc;
    hSDCStation stns = sdci->stns;
    int ictl;
    int istn;
    int istnc;
    int first;
    StatusType sts = STS_OK;

    /*> Compute the distance from each control mark to each tested
        mark, and store the minimum of these distances */

    /*  This uses a very crude algorithm, simply computing the
        distance from each control mark to each tested mark and
        storing the distance if it is less than the currently
        stored value.  Could perhaps do more efficient things
        with a sorted list of nodes if we wanted to. */

    first = 1;
    for( ictl = 0; sts == STS_OK && ictl < sdc->nmark; ictl++ )
    {
        if( ictl % ABORT_FREQUENCY  == 0 ) sts = utlCheckAbort();
        if( stns[ictl].role != SDC_CONTROL_MARK ) continue;
        for( istn = 0; sts == STS_OK && istn < sdc->nmark; istn++ )
        {
            if( istn % ABORT_FREQUENCY  == 0 ) sts = utlCheckAbort();
            if( stns[istn].role != SDC_IGNORE_MARK )
            {
                float dist = (float) (sdc->pfDistance2)(sdc->env,ictl, istn );
                if( first || dist < stns[istn].ctldist2 )
                {
                    stns[istn].ctldist2 = dist;
                }
            }
        }
        first = 0;
    }

    /*> Calculate the maximum distance between the nearest control mark */

    sdci->maxctldist2 = 0.0;
    istnc=-1;

    for( istn = 0; sts == STS_OK && istn < sdc->nmark; istn++ )
    {
        hSDCStation stn = &(stns[istn]);

        if( istn % ABORT_FREQUENCY == 0 ) sts = utlCheckAbort();

        if( stn->role == SDC_CONTROL_MARK || stn->role == SDC_IGNORE_MARK )
            continue;

        if( stn->ctldist2 > sdci->maxctldist2 )
        {
            sdci->maxctldist2 = stn->ctldist2;
            istnc=istn;
        }

        if( sdci->loglevel & SDC_LOG_CALCS )
        {
            sdcWriteLog( sdci, SDC_LOG_CALCS,
                         "    Station %ld is %.2lf m2 from control\n",
                         sdcStationId(sdci,istn), (double)(stn->ctldist2) );
        }
        if( sdci->loglevel & SDC_LOG_COMPACT )
        {
            sdcWriteCompactLog( sdci,sdcStationId(sdci,istn),-1,SDC_TEST_DCTL,"",stn->ctldist2,-1,"" );
        }
    }
    sdcWriteLog(sdci, SDC_LOG_STEPS | SDC_LOG_CALCS | SDC_LOG_CALCS2,
                "Maximum distance to nearest control %.2lfm from %ld\n",sqrt(sdci->maxctldist2),sdcStationId(sdci,istnc));

    return sts;
}


/*************************************************************************
** Function name: sdcApplyAbsAccuracy
**//**
**    Applies the absolute accuracy test and also the relative accuracy
**    by absolute accuracy tests.
**
**  \param sdci                The test implementation
**  \param test                Definition of the current order
**                             being tested.
**  \param nleft               Returns the number of marks still
**                             to assign a status
**
**  \return                    returns the abort status
**
**************************************************************************
*/

static StatusType sdcApplyAbsAccuracy( hSDCTestImp sdci, hSDCOrderTest test,
                                       int *nleft )
{
    hSDCTest sdc = sdci->sdc;
    hSDCStation stns = sdci->stns;
    int i;
    int nunknown;
    double tolpass=0.0;
    double tolfail=0.0;
    double ftol=0.0;
    double dtol=0.0;
    double tolpassv=0.0;
    double tolfailv=0.0;
    double ftolv=0.0;
    double dtolv=0.0;
    char testhor = 0;
    char testvrt = 0;
    int nhfail = 0;
    int nhdfail = 0;
    int nvfail = 0;
    int nvdfail = 0;
    int npass = 0;
    StatusType sts = STS_OK;

    if( test->blnTestHor )
    {
        testhor = 1;

        /*> Calculate the tolerances for failing the max abs accuracy test, and
            for passing the relative accuracy by abs accuracy test */

        tolfail = test->dblAbsTestAbsMax / sdc->dblErrFactor;
        tolfail *= tolfail;

        tolpass = test->dblRelTestAbsMin / sdc->dblErrFactor;
        tolpass *= tolpass;

        /*> Calculate the fixed and distance dependent tolerances (applying the
            error scale factor and squaring to be comparable with error
            calculated */

        ftol = test->dblAbsTestDFMax / sdc->dblErrFactor;
        ftol *= ftol;
        dtol = test->dblAbsTestDDMax / (100.0 * sdc->dblErrFactor);
        dtol *= dtol;

    }

    if( test->blnTestVrt )
    {
        testvrt = 1;

        /*> If testing vertical accuracy, calculate the equivalent
            vertical components.
            */

        tolfailv = test->dblAbsTestAbsMaxV / sdc->dblErrFactor;
        tolfailv *= tolfailv;

        tolpassv = test->dblRelTestAbsMinV / sdc->dblErrFactor;
        tolpassv *= tolpassv;

        ftolv = test->dblAbsTestDFMaxV / sdc->dblErrFactor;
        ftolv *= ftolv;
        dtolv = test->dblAbsTestDDMaxV / (100.0 * sdc->dblErrFactor);
        dtolv *= dtolv;

    }

    /*> For each station with unknown status set the status to fail if
        it is greater than the Max Absolute Accuracy test limit or
        than Max distance dependent absolute accuracy test, and to
        pass if it is less than the Relative Accuracy by Absolute Accuracy
        limit.  */

    nunknown=0;

    for( i = 0; sts == STS_OK && i < sdc->nmark; i++ )
    {
        hSDCStation s = & stns[i];
        long sdcstni=sdcStationId(sdci,i);
        if( i % ABORT_FREQUENCY == 0 ) sts = utlCheckAbort();
        if( s->status == SDC_STS_UNKNOWN )
        {
            int hpass=1;
            int vpass=1;
            if( testhor )
            {
                hpass=0;
                if( s->error2 > tolfail )
                {
                    sdcWriteLog( sdci, SDC_LOG_TESTS,
                                 "  Station %ld fails abs accuracy (%.8lf > %.8lf)\n",
                                 sdcstni, s->error2, tolfail );
                    sdcWriteCompactLog( sdci, sdcstni,-1,SDC_TEST_HAA,"F",s->error2,tolfail,"");
                    s->status = SDC_STS_FAIL;
                    nhfail++;
                }
                else if ( s->error2 > ftol+dtol*s->ctldist2 )
                {
                    sdcWriteLog( sdci, SDC_LOG_TESTS,
                                 "  Station %ld fails dist dependent abs accuracy (%.8lf > %.8lf)\n",
                                 sdcstni, s->error2, ftol+dtol*s->ctldist2 );
                    sdcWriteCompactLog( sdci, sdcstni,-1,SDC_TEST_HDAA,"F", s->error2,ftol+dtol*s->ctldist2,"");
                    s->status = SDC_STS_FAIL;
                    nhdfail++;
                }
                else if ( s->error2 < tolpass )
                {
                    sdcWriteLog( sdci, SDC_LOG_TESTS,
                                 "  Station %ld passes rel acc from abs accuracy (%.8lf < %.8lf)\n",
                                 sdcstni,s->error2, tolpass );
                    sdcWriteCompactLog( sdci, sdcstni, -1,SDC_TEST_HRAA,"P",s->error2,tolpass,"");
                    hpass=1;
                }
            }

            if( testvrt && s->status != SDC_STS_FAIL )
            {
                vpass=0;
                if( s->verror2 > tolfailv )
                {
                    sdcWriteLog( sdci, SDC_LOG_TESTS,
                                 "  Station %ld fails vrt abs accuracy (%.8lf > %.8lf)\n",
                                 sdcstni, s->verror2, tolfailv );
                    sdcWriteCompactLog( sdci, sdcstni,-1,SDC_TEST_VAA,"F",s->verror2,tolfailv,"");
                    s->status = SDC_STS_FAIL;
                }
                else if ( s->verror2 > ftolv+dtolv*s->ctldist2 )
                {
                    sdcWriteLog( sdci, SDC_LOG_TESTS,
                                 "  Station %ld fails vrt dist dependent abs accuracy (%.8lf > %.8lf)\n",
                                 sdcstni, s->verror2, ftolv+dtolv*s->ctldist2 );
                    sdcWriteCompactLog( sdci, sdcstni,1,SDC_TEST_VDAA,"F",
                                        s->verror2,ftolv+dtolv*s->ctldist2,"");
                    s->status = SDC_STS_FAIL;
                }
                else if ( s->verror2 < tolpassv )
                {
                    sdcWriteLog( sdci, SDC_LOG_TESTS,
                                 "  Station %ld passes vrt rel acc from abs accuracy (%.8lf < %.8lf)\n",
                                 sdcstni, s->verror2, tolpassv );
                    sdcWriteCompactLog( sdci, sdcstni, -1,SDC_TEST_VRAA,"P",s->verror2,tolpassv,"");
                    vpass=1;
                }
            }

            if( hpass && vpass )
            {
                sdcWriteCompactLog( sdci, sdcstni, -1,SDC_TEST_RAA,"P",-1,-1,"");
                s->status = SDC_STS_PASS;
                npass++;
            }

            if( s->status == SDC_STS_UNKNOWN )
            {
                nunknown++;
            }
        }
    }

    if( nhfail )
    {
        sdcWriteLog( sdci, SDC_LOG_STEPS,
                     "  %d stations failed on absolute hor accuracy\n", nhfail );
    }

    if( nhdfail )
    {
        sdcWriteLog( sdci, SDC_LOG_STEPS,
                     "  %d stations failed on relative hor accuracy to control\n", nhdfail );
    }

    if( nvfail )
    {
        sdcWriteLog( sdci, SDC_LOG_STEPS,
                     "  %d stations failed on absolute vrt accuracy control\n", nvfail );
    }

    if( nvdfail )
    {
        sdcWriteLog( sdci, SDC_LOG_STEPS,
                     "  %d stations failed on relative vrt accuracy to control\n", nvdfail );
    }

    if( npass )
    {
        sdcWriteLog( sdci, SDC_LOG_STEPS,
                     "  %d stations passed relative accuracy by absolute accuracy\n", npass );
    }

    if( nunknown )
    {
        sdcWriteLog( sdci, SDC_LOG_STEPS,
                     "  %d stations unassigned\n", nunknown );
    }


    *nleft = nunknown;
    return sts;
}


/*************************************************************************
** Function name: sdcCreateRelTest
**//**
**    Creates the matrices required to hold the relative accuracy test
**    information, and initiallizes the array of distances between marks,
**    as this will always be required (either to apply a test, or to
**    determine that a test need not be applied).
**
**  \param sdci                The test implementation
**
**  \return                    Returns the abort status
**
**************************************************************************
*/

static StatusType sdcCreateRelTest( hSDCTestImp sdci)
{
    hSDCTest sdc = sdci->sdc;
    hSDCStation stns = sdci->stns;
    int nrow;
    int i;

    StatusType sts = STS_OK;

    /*> Count the rows for which relative tests are required */

    nrow = 0;
    for( i = 0; i < sdc->nmark; i++ )
    {
        hSDCStation s = & (stns[i]);
        s->nrelrow = -1;
        if( s->role == SDC_IGNORE_MARK ) continue;
        s->nrelrow = nrow++;
    }
    if( nrow <= 2 ) return sts;

    /*> Allocate the tables for vector information, status information,
        and reverse lookup of row numbers */

    sdci->lookup = (int *) utlAlloc( sizeof(int) * nrow );
    sdci->nreltest = nrow;

    /*> Initiallize the reverse lookup from rel test code to station code */

    for( i = 0; i < sdc->nmark; i++ )
    {
        int nr = stns[i].nrelrow;
        if( nr >= 0 ) sdci->lookup[nr] = i;
    }

    if( sts == STS_OK )
    {
        sts = utlShowProgress("Preparing relative accuracy test", 100);
    }

    sdcRAInitAllocRow( sdci );

    return sts;
}




/*************************************************************************
** Function name: sdcApplyRelTest
**//**
**    Applies the relative accuracy test .. a relative complex algorithm!
**
**  \param sdci                The test implementation
**  \param test                Definition of the current order
**                             being tested.
**
**  \return                    Returns the abort status
**
**************************************************************************
*/

static StatusType sdcApplyRelTest( hSDCTestImp sdci, hSDCOrderTest test)
{
    int npass=0;
    int nfailacc;
    int nfailcnt;
    int found;
    int ntotalpass = 0;
    int ntotalfailacc = 0;
    int ntotalfailcnt = 0;
    int ntotalfailsel = 0;
    StatusType sts = STS_OK;

    /* Log the initial state of the stations prior to the relative accuracy tests */

    if( sdci->loglevel & SDC_LOG_COMPACT )
    {
        hSDCStation stns = sdci->stns;
        for( int i = 1; i < sdci->nreltest; i++ )
        {
            int istni = sdci->lookup[i];
            long sdcstni=sdcStationId(sdci,istni);
            hSDCStation stni = &(stns[istni]);
            sdcWriteCompactLog( sdci, sdcstni,stni->status,SDC_TEST_RAIS,"",-1,-1,"");
        }
    }

    /*> Set up the relative accuracy status matrix */

    sts = sdcSetupRelAccuracyStatus( sdci, test );

    /*> Loop ... */
    found = 0;
    while( sts == STS_OK )
    {

        /*>> Apply the test for failed nodes */
        sts = sdcApplyRelTestFail( sdci, test, &nfailacc, &nfailcnt );
        ntotalfailacc += nfailacc;
        ntotalfailcnt += nfailcnt;

        /*>> Then the test for passed nodes */
        if( sts==STS_OK ) sts = sdcApplyRelTestPass( sdci, &npass );
        ntotalpass += npass;

        /*>> Then seek another node to fail, and exit if none is found .. */
        if( sts == STS_OK ) sts = sdcSeekRelTestFail( sdci, test, &found );
        if( ! found ) break;
        ntotalfailsel++;
    }

    if( ntotalpass )
    {
        sdcWriteLog( sdci, SDC_LOG_STEPS,
                     "  %d stations passed relative accuracy tests\n", ntotalpass );
    }
    if( ntotalfailacc )
    {
        sdcWriteLog( sdci, SDC_LOG_STEPS,
                     "  %d stations failed on relative accuracy\n", ntotalfailacc );
    }

    if( ntotalfailcnt )
    {
        sdcWriteLog( sdci, SDC_LOG_STEPS,
                     "  %d stations failed with too few relative tests remaining\n", ntotalfailcnt );
    }

    if( ntotalfailsel )
    {
        sdcWriteLog( sdci, SDC_LOG_STEPS,
                     "  %d stations failed by selecting a station to fail\n",
                     ntotalfailsel );
    }


    return sts;
}



/*************************************************************************
** Function name: sdcApplyRelTestPass
**//**
**    Seeks all marks that pass the relative accuracy test because all
**    test vectors from them pass.
**
**  \param sdci                The test implementation
**  \param pnpass              The number of passed nodes
**
**  \return                    The abort status
**
**************************************************************************
*/

static StatusType sdcApplyRelTestPass( hSDCTestImp sdci, int *pnpass)
{
    hSDCStation stns = sdci->stns;
    hSDCTest sdc = sdci->sdc;
    int nmark = sdc -> nmark;
    int npass = 0;
    int istn;
    StatusType sts = STS_OK;

    /*> For each station */

    for( istn = 0; sts == STS_OK && istn < nmark; istn++ )
    {
        hSDCStation stni = &(stns[istn]);
        long sdcstni = sdcStationId(sdci,istn);
        if( istn % ABORT_FREQUENCY == 0 ) sts = utlCheckAbort();

        /*>> If the status is unknown, and all tests pass, then
             set the station status to SDC_STS_PASS */

        if( stni->status == SDC_STS_UNKNOWN &&
                /* stni->nreltest > 0 && */
                stni->nrelbad <= 0 )
        {
            sdcWriteLog( sdci, SDC_LOG_TESTS, "  Station %ld passes rel accuracy tests (%d)\n",
                         sdcstni, stni->nreltest );
            sdcSetRelTestStatus( sdci, istn, SDC_STS_PASS );
            sdcWriteCompactLog( sdci, sdcstni,-1,SDC_TEST_RA,"P",stni->nreltest,-1,"");
            npass++;
        }
    }

    (*pnpass) =  npass;
    return sts;
}


/*************************************************************************
** Function name: sdcApplyRelTestFail
**//**
**    Seeks all marks that fail the relative accuracy test because one
**    or more vectors from them to an already passed node fails.
**
**  \param sdci                The test implementation
**  \param test                The current test being applied
**  \param pnfailacc           Returns the number of nodes failed
**  \param pnfailcount         Returns the number of nodes failed
**
**  \return                    Returns the abort status
**
**************************************************************************
*/

static StatusType sdcApplyRelTestFail( hSDCTestImp sdci, hSDCOrderTest test,
                                       int *pnfailacc, int *pnfailcount )
{
    hSDCStation stns = sdci->stns;
    int nmark = sdci->sdc->nmark;
    int nfailacc = 0;
    int nfailcnt = 0;
    int istn;
    StatusType sts = STS_OK;

    /*> Reject any stations for which there a failed vectors to passed
        stations */

    for( istn = 0; sts == STS_OK && istn < nmark; istn++ )
    {
        hSDCStation stni = &(stns[istn]);
        long sdcstni = sdcStationId( sdci, istn );
        if( istn % ABORT_FREQUENCY == 0 ) sts = utlCheckAbort();
        if( stni->status == SDC_STS_UNKNOWN && stni->nrelfail )
        {
            sdcWriteLog( sdci, SDC_LOG_TESTS, "  Station %ld fails rel accuracy tests (%d/%d)\n",
                         sdcstni, stni->nrelfail, stni->nreltest );
            sdcSetRelTestStatus( sdci, istn, SDC_STS_FAIL );
            sdcWriteCompactLog( sdci, sdcstni,-1,SDC_TEST_RA,"F", stni->nreltest, stni->nrelfail,"");
            nfailacc++;
        }
    }

    /*> Reject any stations for too few relative vectors remain */

    if( test->iMinRelAcc > 0 )
    {
        for( istn = 0; sts == STS_OK && istn < nmark; istn++ )
        {
            hSDCStation stni = &(stns[istn]);
            long sdcstni = sdcStationId( sdci, istn );
            if( istn % ABORT_FREQUENCY == 0 ) sts = utlCheckAbort();
            if( stni->status == SDC_STS_UNKNOWN && stni->nreltest < test->iMinRelAcc )
            {
                sdcWriteLog( sdci, SDC_LOG_TESTS, "  Station %ld fails with too few relative accuracy tests\n",
                             sdcstni );
                sdcWriteCompactLog( sdci,sdcstni,-1,SDC_TEST_RAC,"F",-1,-1,"");
                sdcSetRelTestStatus( sdci, istn, SDC_STS_FAIL );
                nfailcnt++;
            }
        }
    }

    if( pnfailacc) *pnfailacc = nfailacc;
    if( pnfailcount) *pnfailcount = nfailcnt;
    return sts;
}


/*************************************************************************
** Function name: sdcSeekRelTestFail
**//**
**    Seeks a candidate node to fail when there is no automatic choice.
**    Seaches for the unassigned (ie unknown status) station with the
**    lowest priortity, or worst test order, or highest percentage of failed vectors.
**    If more than one match, then pick the node with the highest absolute error.
**
**  \param sdci                The test implementation
**  \param test                The order being tested
**  \param pfailed             True if a node is failed
**
**  \return                    Returns the abort status
**
**************************************************************************
*/

static StatusType sdcSeekRelTestFail( hSDCTestImp sdci, hSDCOrderTest test, int *pfailed )
{
    hSDCTest sdc = sdci->sdc;
    hSDCStation stns = sdci->stns;
    int nmark = sdci->sdc->nmark;
    int istnfail = -1;
    int priority = 0;
    int testorder = 0;
    int maxorder = 0;
    int maxpriority = 0;
    float maxfailratio = 0.0;
    float maxfailerror = 0.0;
    float failratio;
    float failerror;
    /* float verror; */
    int istn;
    StatusType sts = STS_OK;
    float herrmult = test->blnTestHor ? 1.0 : 0.0;
    /* float verrmult = test->blnTestVrt ? test->dblVertHorRatio*test->dblVertHorRatio : 0.0; */

    /*> Seek the station with the maximum percentage of failed tests,
        (resolve tiebreakers using the absolute error of the station */

    for( istn = 0; sts == STS_OK && istn < nmark; istn++ )
    {
        hSDCStation stni = &(stns[istn]);
        if( istn % ABORT_FREQUENCY == 0 ) sts = utlCheckAbort();

        if( stni->status != SDC_STS_UNKNOWN ) continue;
        if( stni->nreltest == 0 ) continue;

        if( sdc->pfStationPriority ) priority = (sdc->pfStationPriority)( sdc->env, istn );
        if( sdc->pfStationRole ) testorder = (sdc->pfStationRole)( sdc->env, istn );

        if( priority != SDC_NO_PRIORITY )
        {
            if( maxpriority == SDC_NO_PRIORITY ) continue;
            if( priority < maxpriority ) continue;
        }
        if( priority != maxpriority ) maxorder=testorder;
        else if( testorder < maxorder ) continue;

        failratio = ((float)(stni->nrelbad))/stni->nreltest;
        if( failratio < maxfailratio ) continue;

        failerror = stni->error2 * herrmult;
        /* verror = stni->verror2 * verrmult; */

        if( priority != maxpriority ||
                testorder != maxorder ||
                failratio > maxfailratio ||
                (failratio == maxfailratio && failerror > maxfailerror) )
        {
            istnfail = istn;
            maxpriority = priority;
            maxorder = testorder;
            maxfailratio = failratio;
            maxfailerror = failerror;
        }
    }

    /*> If a mark is found, set the status of the mark to fail */

    if( istnfail >= 0 )
    {
        sdcWriteLog( sdci, SDC_LOG_TESTS,
                     "  Station %ld selected to fail rel acc tests (%.3lf,%.8lf)\n",
                     sdcStationId(sdci,istnfail),
                     (double) maxfailratio, (double) maxfailerror );
        sdcWriteCompactLog( sdci, sdcStationId(sdci,istnfail),-1,SDC_TEST_RAS,"F",maxfailratio,maxfailerror,"");
        sdcSetRelTestStatus( sdci, istnfail, SDC_STS_FAIL );
    }

    *pfailed =  (istnfail >= 0) ? 1 : 0;
    return sts;
}


/*************************************************************************
** Function name: sdcSetRelTestStatus
**//**
**    Set the status of a node to pass or fail, and updated the relative
**    accuracy results to reflect this.  If the node is passed, then
**    increment the count of failed tests for any node to which it is
**    connected by a failed vector.  If the node is rejected, then remove
**    the counts of any vectors to it from the nodes to which they are
**    connected.
**
**  \param sdci                The test implementation
**  \param istn                The station for which the status is
**                             to be set
**  \param status              The status to set it to.
**
**  \return
**
**************************************************************************
*/

static void sdcSetRelTestStatus( hSDCTestImp sdci, int istn, char status)
{
    hSDCStation stns = sdci->stns;
    unsigned char *relstatus;
    int nreltest = sdci->nreltest;
    int col0;
    int nrelrow = stns[istn].nrelrow;
    int i;

    /*> Set the station status */

    stns[istn].status = status;

    /*> For each other station for which a relative test is defined .. */

    relstatus=sdci->relstatus[nrelrow];
    col0=sdci->relcol0[nrelrow];
    if( ! relstatus ) col0=nrelrow+1;

    for( i = col0; i < nreltest; i++ )
    {
        hSDCStation stn;
        unsigned char stsij=SDC_STS_SKIP;

        if( i <= nrelrow )
        {
            stsij=relstatus[i-col0];
        }
        else
        {
            relstatus=sdci->relstatus[i];
            col0=sdci->relcol0[i];
            if( ! relstatus || col0 > nrelrow ) continue;
            stsij=relstatus[nrelrow-col0];
        }

        /*>> If the station has passed then .. */

        stn = &(stns[sdci->lookup[i]]);

        if( status == SDC_STS_PASS )
        {

            /*>>> If the test fails, then increment the count of failed
                 tests for the corresponding node */

            if( stsij == SDC_STS_FAIL)
            {
                stn->nrelfail++;
            }
        }

        /*>> If the station has failed or is untestable then ... */

        else
        {

            /*>>> Remove the test from the any attached node */
            /*>>> If the test passed then decrement the pass count of the node */

            if( stsij == SDC_STS_PASS )
            {
                stn->nreltest--;
            }
            else if( stsij == SDC_STS_FAIL)
            {
                stn->nreltest--;
                stn->nrelbad--;
            }
        }

    }
}


/*************************************************************************
** Function name: sdcSetupRelAccuracyStatus
**//**
**    Sets up the relative accuracy status matrix for a given order.  This
**    finds all vectors which are within the test range for the order,
**    calculates the error to see if they pass or fail, and for each node
**    counts the number of tests, the number of passed tests.  This will
**    also fail nodes which are connected to passed nodes by failed vectors.
**
**  \param sdci                The test implementation
**  \param test                Definition of the current order
**                             being tested.
**
**  \return                    The abort status
**
**************************************************************************
*/

static StatusType sdcSetupRelAccuracyStatus( hSDCTestImp sdci, hSDCOrderTest test)
{
    hSDCTest sdc = sdci->sdc;
    hSDCStation stns = sdci->stns;
    double range2;
    char userange;
    double ftol;
    double dtol;
    double ftolv;
    double dtolv;
    double maxtestdist = 0.0;
    double maxtestdistv = 0.0;
    int nmark = sdc->nmark;
    int options = sdc->options;
    int i;
    int j;
    char shortcircuit = ! (options & SDC_OPT_NO_SHORTCIRCUIT_CVR);
    char strictcovar = options & SDC_OPT_STRICT_SHORTCIRCUIT_CVR;
    char needcovariances = 0;
    char usemaxdistance = 0;
    char usemaxdistancev = 0;
    char logcalcs = (sdci->loglevel & (SDC_LOG_CALCS | SDC_LOG_CALCS2)) ? 1 : 0;
    char logcalcs2 = (sdci->loglevel & SDC_LOG_CALCS2) ? 1 : 0;
    char logcompact = (sdci->loglevel & SDC_LOG_COMPACT) ? 1 : 0;
    char testhor = test->blnTestHor;
    char testvrt = test->blnTestVrt;

    StatusType sts = STS_OK;

    sdcTimeStamp(sdci,"Setting up relative accuracy status");

    /*> Initiallize the counters for each of the tests */

    for( i = 0; i < nmark; i++ )
    {
        hSDCStation s = &(stns[i]);
        s->nreltest = 0;
        s->nrelbad  = 0;
        s->nrelfail = 0;
    }

    /*> Initiallized the memory allocator for the relative
     * accuracy calcs */

    sdcRAInitAllocRow( sdci );

    /*> Set up the array of tests status information - basically we are
        interested in codes with status of unknown, and their connections
        to marks with status passed.  Also may apply a maximum distance
        test. */

    range2 = test->dblRange;
    range2 *= range2;

    if( test->dblMaxCtlDistFactor > 0.0 )
    {
        double crange2 = sdci->maxctldist2*test->dblMaxCtlDistFactor*test->dblMaxCtlDistFactor;
        if( crange2 < range2 ) range2 = crange2;
        sdcWriteLog( sdci, SDC_LOG_STEPS | SDC_LOG_CALCS | SDC_LOG_CALCS2,
                     "  Relative accuracy range calculated as %.2f m2\n",sqrt(range2) );
    }
    userange = range2 > 0.01;

    ftol = test->dblRelTestDFMax / sdc->dblErrFactor;
    ftol *= ftol;
    dtol = test->dblRelTestDDMax / (100.0 * sdc->dblErrFactor);
    dtol *= dtol;

    ftolv = test->dblRelTestDFMaxV / sdc->dblErrFactor;
    ftolv *= ftolv;
    dtolv = test->dblRelTestDDMaxV / (100.0 * sdc->dblErrFactor);
    dtolv *= dtolv;

    if( shortcircuit && testhor && dtol > 0.0 )
    {
        double maxerror = sdci->maxerror2;
        maxerror *= strictcovar ? 2 : sqrt(2.0);
        maxtestdist = (maxerror - ftol) / dtol;
        if( maxtestdist < 0.0 ) maxtestdist = 0.0;
        usemaxdistance = 1;
        sdcWriteLog( sdci, SDC_LOG_STEPS | SDC_LOG_CALCS | SDC_LOG_CALCS2,
                     "  Relative hor accuracy assumed achieved for lines longer than %.2lf m\n",sqrt(maxtestdist));
    }
    if( shortcircuit && testvrt && dtolv > 0.0 )
    {
        double maxerror = sdci->maxverror2;
        double factor = 2.0;
        if( strictcovar ) factor=sqrt(factor);
        maxerror *= factor;
        maxtestdistv = (maxerror - ftolv) / dtolv;
        usemaxdistancev = 1;
        sdcWriteLog( sdci, SDC_LOG_STEPS | SDC_LOG_CALCS | SDC_LOG_CALCS2,
                     "  Relative vrt accuracy assumed achieved for lines longer than %.2lf m\n",sqrt(maxtestdistv));
    }


    /*> First pass for each line - attempt to determine pass or fail status for each vector between
        stations with status unknown or passed ... */

    sdcWriteLog( sdci, SDC_LOG_CALCS | SDC_LOG_CALCS2, "  Calculating status for each vector\n");


    for( i = 1; i < sdci->nreltest; i++ )
    {
        unsigned char *relstatus = 0;

        int istni = sdci->lookup[i];
        long sdcstni=sdcStationId(sdci,istni);
        hSDCStation stni = &(stns[istni]);
        char stsi = stni->status;
        char passi = (stsi == SDC_STS_PASS || stsi == SDC_STS_PASSED);


        if( i % ABORT_FREQUENCY == 0 )
        {
            sts = utlCheckAbort();
            if( sts != STS_OK ) return sts;
        }

        if( ! passi && stsi != SDC_STS_UNKNOWN) continue;

        for( j = 0; j < i; j++ )
        {
            int istnj = sdci->lookup[j];
            long sdcstnj=sdcStationId(sdci,istnj);
            hSDCStation stnj = &(stns[istnj]);
            char stsj = stnj->status;
            char passj = (stsj == SDC_STS_PASS || stsj == SDC_STS_PASSED);
            double dist;
            unsigned char stsij = SDC_STS_UNKNOWN;

            if( relstatus )
            {
                relstatus++;
                *relstatus = SDC_STS_SKIP;
            }

            if( j % ABORT_FREQUENCY == 0 )
            {
                sts = utlCheckAbort();
                if( sts != STS_OK ) return sts;
            }

            /*>> If neither station needs status calculated then continue */

            if( ! passj && stsj != SDC_STS_UNKNOWN) continue;
            if( passi && passj ) continue;

            /*>> If the stations are more than maximum distance apart then continue */

            dist = (sdc->pfDistance2)(sdc->env, istni, istnj );

            if( userange && dist >= range2 ) continue;

            /*>> Calculate the acceptable relative error for the line.
                 Test length against the maximum length of line that can fail
                 given the maximum station variance.  If this doesn't pass then
                 test against the endpoint coordinate accuracies to see
            	 whether we can pass or fail based on these.  If not, then
            	 use actual relative accuracy and pass or fail using that.
                 The actual relative error (squared) of the line is retrieved from the
            	 the array of relative accuracy values.  If it is not yet stored there
            	 (value of SDC_COVAR_UNAVAILABLE) then calculate it and store it.

                 Do this for horizontal and vertical as required by test. */

            if( testhor )
            {
                double error = 0.0;
                double tolij = ftol + dtol * dist;

                /* Test against maximum length of line that can fail */

                if( usemaxdistance && dist > maxtestdist )
                {
                    stsij = SDC_STS_PASS;
                    if( logcalcs2 ) sdcWriteLog(sdci,SDC_LOG_CALCS2,
                                                    "   Vector %ld to %ld relative accuracy pass by max distance test\n",
                                                    sdcstni,sdcstnj);
                    if( logcompact ) sdcWriteCompactLog( sdci, sdcstni,sdcstnj,SDC_TEST_HRAD,"P",-1,-1,"");
                }

                else
                {

                    /* See if we can pass or fail based on the endpoint coordinate accuracies.
                       Note: adding errors like this is not strictly correct - assumes that they
                       are positively or 0 correlated.  Using strictcovar means this works even
                       if errors could be negatively correlated. */

                    if( shortcircuit )
                    {
                        error = stni->error2 + stnj->error2;

                        if( strictcovar ) error += 2*sqrt(stni->error2*stnj->error2);
                        if( error < tolij )
                        {
                            stsij = SDC_STS_PASS;
                            if( logcalcs2 ) sdcWriteLog(sdci,SDC_LOG_CALCS2,
                                                            "   Vector %ld to %ld relative accuracy pass by absolute (%.8lf < %.8lf)\n",
                                                            sdcstni,sdcstnj,error,tolij);
                            if( logcompact ) sdcWriteCompactLog( sdci, sdcstni,sdcstnj,SDC_TEST_HRAS,"P",error,tolij,"");
                        }
                        else if(
                            (error=(stni->error2 + stnj->error2 - 2*sqrt(stni->error2*stnj->error2))) > tolij )
                        {
                            stsij = SDC_STS_FAIL;
                            if( logcalcs2 ) sdcWriteLog(sdci,SDC_LOG_CALCS2,
                                                            "   Vector %ld to %ld relative accuracy fail by absolute (%.8lf > %.8lf)\n",
                                                            sdcstni,sdcstnj,error,tolij);
                            if( logcompact ) sdcWriteCompactLog( sdci, sdcstni,sdcstnj,SDC_TEST_HRAS,"F",error,tolij,"");
                        }
                    }
                    if( stsij == SDC_STS_UNKNOWN )
                    {
                        error = (sdc->pfError2)(sdc->env,istni,istnj);
                        if( error != SDC_COVAR_UNAVAILABLE )
                        {
                            if( error < tolij ) stsij = SDC_STS_PASS;
                            else stsij = SDC_STS_FAIL;
                            if( logcalcs2 ) sdcWriteLog(sdci,SDC_LOG_CALCS2,
                                                            "   Vector %ld to %ld true relative accuracy %s (%.8lf %s %.8lf)\n",
                                                            sdcstni,sdcstnj, stsij==SDC_STS_PASS ? "pass" : "fail",
                                                            error, stsij==SDC_STS_PASS ? "<" : ">", tolij);
                            if( logcompact ) sdcWriteCompactLog( sdci, sdcstni, sdcstnj, SDC_TEST_HRAC,
                                                                     (stsij==SDC_STS_PASS ? "P" :"F"),error,tolij,"");
                        }
                    }
                }

                /*>> If the covariance could not be found, then record the missing covariance info.
                     It may turn out to be unnecessary, if we can fail one of the nodes in subsequent
                     tests.  */

                if( stsij == SDC_STS_UNKNOWN )
                {
                    stsij = SDC_STS_NEED_CVR;
                    needcovariances=1;
                }
                /*>> If the vector has failed then if one or other node is already passed
                     we can fail the other.   */

                /* If sdci->needphase2 is set then a previous order has incomplete tests so
                 * can't be sure which stations would have passed or failed in those tests.
                 * So in that case we can't fail anything.
                 */

                else if ( ! sdci->needphase2 )
                {
                    if (passi && stsij == SDC_STS_FAIL && ! sdci->needphase2)
                    {
                        stnj->status = SDC_STS_FAIL;
                        sdcWriteLog( sdci, SDC_LOG_TESTS,
                                     "   Station %ld fails on rel accuracy to passed station %ld (%.8lf > %.8lf)\n",
                                     sdcstnj, sdcstni, error, tolij);
                        if( logcompact ) sdcWriteCompactLog( sdci, sdcstnj,sdcstni,SDC_TEST_HRAP,"F",error,tolij,"");
                    }
                    else if(passj && stsij == SDC_STS_FAIL)
                    {
                        stni->status = SDC_STS_FAIL;
                        sdcWriteLog( sdci, SDC_LOG_CALCS | SDC_LOG_CALCS2,
                                     "   Station %ld fails on rel accuracy to passed station %ld (%.8lf > %.8lf)\n",
                                     sdcstni,sdcstnj, error, tolij);
                        if( logcompact ) sdcWriteCompactLog( sdci, sdcstni,sdcstnj,SDC_TEST_HRAP,"F",error,tolij,"");
                        /* Station i has failed so don't need to look at this any more */
                        break;
                    }
                }
            }

            /* --------------------------------------------------------------- */
            /* If not already failed on the horizontal test, then try vertical */

            if( testvrt && stsij != SDC_STS_FAIL )
            {
                double tolij = ftolv + dtolv * dist;
                double error = 0.0;
                char needcvr = stsij == SDC_STS_NEED_CVR;

                stsij = SDC_STS_UNKNOWN;

                /* Test against maximum length of line that can fail */

                if( usemaxdistancev && dist > maxtestdistv )
                {
                    stsij = SDC_STS_PASS;
                    if( logcalcs2 ) sdcWriteLog(sdci,SDC_LOG_CALCS2,
                                                    "   Vector %ld to %ld vrt accuracy pass by max distance test\n",
                                                    sdcstni,sdcstnj);
                    if( logcompact ) sdcWriteCompactLog( sdci, sdcstni,sdcstnj,SDC_TEST_VRAD,"P",-1,-1,"");
                }

                else
                {

                    /* See if we can pass or fail based on the endpoint coordinate accuracies.
                       Note: adding errors like this is not strictly correct - assumes that they
                       are positively or 0 correlated.  Should add on 2*sqrt(stni->error2*stnj->error2)
                       if we think errors could be negatively correlated. */

                    if( shortcircuit )
                    {
                        error = stni->verror2 + stnj->verror2;

                        if( strictcovar ) error += 2*sqrt(stni->verror2*stnj->verror2);
                        if( error < tolij )
                        {
                            stsij = SDC_STS_PASS;
                            if( logcalcs2 ) sdcWriteLog(sdci,SDC_LOG_CALCS2,
                                                            "   Vector %ld to %ld relative vrt accuracy pass by absolute (%.8lf < %.8lf)\n",
                                                            sdcstni,sdcstnj,error,tolij);
                            if( logcompact ) sdcWriteCompactLog( sdci, sdcstni,sdcstnj,SDC_TEST_VRAS,"P",error,tolij,"");
                        }
                        else if(
                            (error=(stni->verror2 + stnj->verror2 - 2*sqrt(stni->verror2*stnj->verror2))) > tolij )
                        {
                            stsij = SDC_STS_FAIL;
                            if( logcalcs2 ) sdcWriteLog(sdci,SDC_LOG_CALCS2,
                                                            "   Vector %ld to %ld relative vrt accuracy fail by absolute (%.8lf > %.8lf)\n",
                                                            sdcstni,sdcstnj,error,tolij);
                            if( logcompact ) sdcWriteCompactLog( sdci, sdcstni,sdcstnj,SDC_TEST_VRAS,"F",error,tolij,"");
                        }
                    }
                    if( stsij == SDC_STS_UNKNOWN )
                    {
                        error = (sdc->pfVrtError2)(sdc->env,istni,istnj);
                        if( error != SDC_COVAR_UNAVAILABLE )
                        {
                            if( error < tolij ) stsij = SDC_STS_PASS;
                            else stsij = SDC_STS_FAIL;
                            if( logcalcs2 ) sdcWriteLog(sdci,SDC_LOG_CALCS2,
                                                            "   Vector %ld to %ld true vrt relative accuracy %s (%.8lf %s %.8lf)\n",
                                                            sdcstni,sdcstnj, stsij==SDC_STS_PASS ? "pass" : "fail",
                                                            error, stsij==SDC_STS_PASS ? "<" : ">", tolij);
                            if( logcompact ) sdcWriteCompactLog( sdci, sdcstni, sdcstnj, SDC_TEST_VRAC,
                                                                     stsij==SDC_STS_PASS ? "P" :"F",error,tolij,"");
                        }
                    }
                }

                /*   If the covariance could not be found, then record this and continue.
                     It may turn out to be unnecessary if one or other node is subsequently
                     failed. */

                if( stsij == SDC_STS_UNKNOWN )
                {
                    stsij = SDC_STS_NEED_CVR;
                    needcovariances=1;
                }
                /*   If the vector has failed then if one or other node is already passed
                     we can fail the other.   */
                else if ( ! sdci->needphase2 )
                {
                    if (passi && stsij == SDC_STS_FAIL)
                    {
                        stnj->status = SDC_STS_FAIL;
                        sdcWriteLog( sdci, SDC_LOG_TESTS,
                                     "   Station %ld fails on rel vrt accuracy to passed station %ld (%.8lf > %.8lf)\n",
                                     sdcstnj, sdcstni, error, tolij);
                        if( logcompact ) sdcWriteCompactLog( sdci, sdcstnj,sdcstni,SDC_TEST_VRAP,"F",error,tolij,"");
                    }
                    else if(passj && stsij == SDC_STS_FAIL)
                    {
                        stni->status = SDC_STS_FAIL;
                        sdcWriteLog( sdci, SDC_LOG_CALCS | SDC_LOG_CALCS2,
                                     "   Station %ld fails on rel vrt accuracy to passed station %ld (%.8lf > %.8lf)\n",
                                     sdcstni, sdcstnj, error, tolij);
                        if( logcompact ) sdcWriteCompactLog( sdci, sdcstni,sdcstnj,SDC_TEST_VRAP,"F",error,tolij,"");
                        /* Station i has failed so don't need to look at this any more */
                        break;
                    }
                }

                if( needcvr ) stsij = SDC_STS_NEED_CVR;
            }

            if( stsij != SDC_STS_SKIP )
            {
                if( ! relstatus )
                {
                    relstatus=sdcRAAllocRow( sdci, i, j );
                }
                *relstatus = stsij;
            }
        }
    }

    sdcTimeStamp(sdci,"Completed first pass using known covariances");

    /*> Now check for missing covariance information */
    /* This is done after all covariances have been tested as covariances that were
     * not found will not be needed if both stations have failed or passed
     */

    if( needcovariances )
    {
        long nmissing=0;
        char status[80];
        sdcWriteLog( sdci, SDC_LOG_CALCS | SDC_LOG_CALCS2, "  Checking missing covariance information\n");

        /* Record each missing value */

        for( i = 1; i < sdci->nreltest; i++ )
        {
            unsigned char *relstatus = sdci->relstatus[i];
            int col0=sdci->relcol0[i];
            int istni = sdci->lookup[i];
            long sdcstni=sdcStationId(sdci,istni);
            hSDCStation stni = &(stns[istni]);
            char stsi = stni->status;
            char passi = (stsi == SDC_STS_PASS || stsi == SDC_STS_PASSED);

            if( i % ABORT_FREQUENCY == 0 )
            {
                sts = utlCheckAbort();
                if( sts != STS_OK ) return sts;
            }

            /* If not passed or potential to pass then not interested */
            if( ! relstatus ) continue;
            if( ! passi && stsi != SDC_STS_UNKNOWN) continue;

            for( j = col0; j < i; j++, relstatus++ )
            {
                int istnj = sdci->lookup[j];
                long sdcstnj=sdcStationId(sdci,istnj);
                hSDCStation stnj = &(stns[istnj]);
                char stsj = stnj->status;
                char passj = (stsj == SDC_STS_PASS || stsj == SDC_STS_PASSED);

                if( j % ABORT_FREQUENCY == 0 )
                {
                    sts = utlCheckAbort();
                    if( sts != STS_OK ) return sts;
                }

                /* If not passed or potential to pass then not interested */
                if( ! passj && stsj != SDC_STS_UNKNOWN) continue;
                if( *relstatus != SDC_STS_NEED_CVR ) continue;

                /*>> If trial phase of two pass calculation then record missing covariance,
                     else abort relative accuracy test */

                if( sdci->phase != SDCI_PHASE_TRIAL )
                {
                    sdcWriteLog( sdci, 0, "Aborting SDC tests due to missing covariance information %ld (%d) to %ld (%d)",
                                 sdcstni,(int) stsi,sdcstnj,(int) stsj);
                    return STS_MISSING_DATA;
                }

                if( logcalcs2 ) sdcWriteLog( sdci, SDC_LOG_CALCS2,
                                                 "    Requesting covariance %ld to %ld\n",
                                                 sdcstni, sdcstnj);
                if( logcompact ) sdcWriteCompactLog( sdci, sdcstni,sdcstnj,SDC_TEST_REQC,"",-1,-1,"");

                (sdc->pfRequestCovar)(sdc->env,istni,istnj);
                sdci->needphase2 = 1;
                nmissing++;
            }
        }

        sprintf(status,"Completed check for missing covariances (%ld required)",nmissing);
        sdcTimeStamp(sdci,status);
    }

    /*> If need phase2 (either from this order or a previous order, then don't process further */

    if( sdci->needphase2 )
    {
        sdcWriteLog( sdci, SDC_LOG_CALCS | SDC_LOG_CALCS2, "  Relative accuracy test postponed pending covariance calcs\n");
        return STS_MISSING_DATA;
    }

    /* -----------------------------------------------------------------------------------*/
    /* Count the number of tests and failed tests for each station               */

    /*> Finally count the number of tests and failed tests for each station */

    if( sdci->twopass )
    {
        sdcWriteLog( sdci, SDC_LOG_CALCS | SDC_LOG_CALCS2, "  Summarizing relative accuracy data\n");
    }

    for( i = 1; i < sdci->nreltest; i++ )
    {
        unsigned char *relstatus = sdci->relstatus[i];
        int col0=sdci->relcol0[i];

        int istni = sdci->lookup[i];
        hSDCStation stni = &(stns[istni]);
        char stsi = stni->status;
        char passi = (stsi == SDC_STS_PASS || stsi == SDC_STS_PASSED);

        if( i % ABORT_FREQUENCY == 0 )
        {
            sts = utlCheckAbort();
            if( sts != STS_OK ) return sts;
        }

        if( ! relstatus ) continue;
        if( ! passi && stsi != SDC_STS_UNKNOWN) continue;

        for( j = col0; j < i; relstatus++, j++ )
        {
            int istnj = sdci->lookup[j];
            hSDCStation stnj = &(stns[istnj]);
            char stsj = stnj->status;
            char passj = (stsj == SDC_STS_PASS || stsj == SDC_STS_PASSED);

            if( j % ABORT_FREQUENCY == 0 )
            {
                sts = utlCheckAbort();
                if( sts != STS_OK ) return sts;
            }
            if( *relstatus == SDC_STS_SKIP ) continue;

            if( ! passj && stsj != SDC_STS_UNKNOWN) continue;
            if( passi && passj ) continue;

            /*>> Increment the count of tests for the nodes */
            stni->nreltest++;
            stnj->nreltest++;

            if( *relstatus == SDC_STS_FAIL )
            {
                /*>> If it fails, then increment count of fails for nodes
                     and if one or other node is already passed, the count of fails
                     connecting to an already passed node */
                stni->nrelbad++;
                stnj->nrelbad++;
            }

        }
    }

    sdcTimeStamp(sdci,"Completed summarizing relative covariance statuses");

    if( sts == STS_OK && sdci->loglevel & SDC_LOG_CALCS )
    {
        if( logcalcs ) sdcWriteLog( sdci, SDC_LOG_CALCS | SDC_LOG_CALCS2, "  Status of marks\n");
        for( i = 0; sts == STS_OK && i < nmark; i++ )
        {
            hSDCStation s = &(stns[i]);
            if( i % ABORT_FREQUENCY == 0 ) sts = utlCheckAbort();
            if( s->status != SDC_STS_UNKNOWN ) continue;
            if( logcalcs ) sdcWriteLog( sdci, SDC_LOG_CALCS | SDC_LOG_CALCS2,
                                            "    Station %ld: %d tests: %d bad\n",
                                            sdcStationId(sdci,i), s->nreltest, s->nrelbad );
        }
    }

    return sts;
}



/*************************************************************************
** Function name: sdcUpdateOrders
**//**
**    Updates the orders of stations which pass a given order test, and
**    initiallizes their status for the next order to be tested.
**
**  \param sdci                The test implementation
**  \param itest               The number of the order being
**                             tested.
**  \param apply               If false then updates are not applied
**
**  \return                    The abort status
**
**************************************************************************
*/

static StatusType sdcUpdateOrders( hSDCTestImp sdci, int itest, int apply )
{
    hSDCTest sdc = sdci->sdc;
    hSDCStation stns = sdci->stns;
    int nmark = sdc->nmark;
    int istn;
    StatusType sts = STS_OK;

    /*> For each station */

    for( istn = 0; sts == STS_OK && istn < nmark; istn++ )
    {
        hSDCStation stni = &(stns[istn]);
        if( istn % ABORT_FREQUENCY == 0 ) sts = utlCheckAbort();

        /*>> If it has passed, then set its order to that for the test
             and set the status to SDC_STS_PASSED */

        if( stni->status == SDC_STS_PASS )
        {
            stni->status = SDC_STS_PASSED;
            stni->passtest = itest;
            if( apply )
            {
                (sdc->pfSetOrder)( sdc->env, istn, itest );
                sdcWriteLog(sdci,SDC_LOG_TESTS,"  Setting order of node %ld to %s\n",
                            sdcStationId(sdci,istn), sdc->tests[itest].scOrder );
            }
        }

        /*>> If it has failed then set the status back to SDC_STS_UNKNOWN
             ready for the next order of test */

        else if ( stni->status == SDC_STS_FAIL )
        {
            stni->status = SDC_STS_UNKNOWN;
        }
    }
    return sts;
}


/*************************************************************************
** Function name: sdcApplyDefaultOrder
**//**
**    Applies a default order for all stations which have not been assigned
**    an order from any of the order tests.
**
**  \param sdci                The test implementation
**
**  \return                    The abort status
**
**************************************************************************
*/

static StatusType sdcApplyDefaultOrder( hSDCTestImp sdci)
{
    hSDCTest sdc = sdci->sdc;
    hSDCStation stns = sdci->stns;
    int nmark = sdc->nmark;
    int istn;
    StatusType sts = STS_OK;

    /*> For each station */

    for( istn = 0; sts == STS_OK && istn < nmark; istn++ )
    {
        hSDCStation stni = &(stns[istn]);
        if( istn % ABORT_FREQUENCY == 0 ) sts = utlCheckAbort();

        /*>> If the status is still unknown then set it to failed */

        if( stni->status == SDC_STS_UNKNOWN )
        {
            (sdc->pfSetOrder)( sdc->env, istn, SDC_DEFAULT );
            sdcWriteLog(sdci,SDC_LOG_TESTS,"  Setting order of node %ld to default %s\n",
                        sdcStationId(sdci,istn), sdc->scFailOrder );
        }
    }
    return sts;
}



/*************************************************************************
** Function name: sdcWriteLog
**//**
**    Writes log information to the log file if required by the log level
**
**  \param sdci                The test implementation
**  \param level               The log level for the output
**  \param fmt                 The format string
**                             The format parameters
**
**  \return
**
**************************************************************************
*/

static void sdcWriteLog( hSDCTestImp sdci, int level, const char *fmt, ... )
{
    va_list ap;

    /*> Check that the log level is appropriate */

    if( level && ! (sdci->loglevel & level) ) return;

    /*> Format the string using the supplied parameters using vsprintf */

    va_start(ap, fmt);
    vsprintf(sdci->logbuffer, fmt, ap);
    va_end(ap);

    /*> Use the function pointer in the sdc object to write the log */

    (sdci->sdc->pfWriteLog)( sdci->sdc->env, sdci->logbuffer );
}

/*************************************************************************
** Function name: sdcWriteCompactLogHeader
**//**
**    Writes the compact log format header
**
**  \param sdci                The test implementation
**
**  \return
**
**************************************************************************
*/

static void sdcWriteCompactLogHeader( hSDCTestImp sdci )
{
    if( ! (sdci->sdc->pfWriteCompact ) ) return;
    if( ! (sdci->loglevel & SDC_LOG_COMPACT) ) return;
    (sdci->sdc->pfWriteCompact)( sdci->sdc->env, "phase,order,stn1,stn2,test,status,v1,v2,comment\n");
}

/*************************************************************************
** Function name: sdcWriteCompactLog
**//**
**    Writes log information to the log file in a compact format for analysis
**
**  \param sdci                The test implementation
**  \param stn1                Id of first station or -1
**  \param stn2                Id of second station or -1
**  \param test                Id of test/information being output
**  \param status              Pass/fail status
**  \param v1                  Test value 1
**  \param v2                  Test value 2
**                             The format parameters
**
**  \return
**
**************************************************************************
*/

static void sdcWriteCompactLog( hSDCTestImp sdci, long stn1, long stn2,
                                const char *test, const char *status, double v1, double v2, const char *comment )
{
    char cstn1[20],cstn2[20],cv1[40],cv2[40];

    /*> Check that writing  log level is appropriate */

    if( ! (sdci->sdc->pfWriteCompact ) ) return;
    if( ! (sdci->loglevel & SDC_LOG_COMPACT) ) return;

    /*> Format the string using the supplied parameters using vsprintf */

    cstn1[0]=cstn2[0]=cv1[0]=cv2[0]=0;
    if( stn1 >= 0 ) sprintf(cstn1,"%ld",stn1);
    if( stn2 >= 0 ) sprintf(cstn2,"%ld",stn2);
    if( v1 >= 0 ) sprintf(cv1,"%.8lf",v1);
    if( v2 >= 0 ) sprintf(cv2,"%.8lf",v2);

    sprintf(sdci->logbuffer,"%d,%d,%s,%s,%s,%s,%s,%s,%s\n",
            sdci->phase,sdci->order,cstn1,cstn2,test,status,cv1,cv2,comment);

    /*> Use the function pointer in the sdc object to write the log */

    (sdci->sdc->pfWriteCompact)( sdci->sdc->env, sdci->logbuffer );
}



/*************************************************************************
** Function name: sdcStationId
**//**
**    Returns the external id of a specified station
**
**  \param sdci                The test implementation
**  \param istn                The station for which the id is required
**
**  \return                    The external id of the station.
**
**************************************************************************
*/

static long sdcStationId( hSDCTestImp sdci, int istn )
{
    long id;

    id = istn;
    if( sdci->sdc->pfStationId )
    {
        id = (sdci->sdc->pfStationId)( sdci->sdc->env, istn );
    }

    return id;
}


/*************************************************************************
** Function name: sdcTimeStamp
**//**
**    Write a timestamp message
**
**  \param sdci                The test implementation
**  \param status              The completed phase
**
**  \return                    The external id of the station.
**
**************************************************************************
*/

static void sdcTimeStamp( hSDCTestImp sdci, const char *status )
{
    double ttotal;
    double tlast;
    clock_t now;

    if( ! (sdci->loglevel & SDC_LOG_TIMESTAMP) ) return;
    now = clock();
    ttotal = (double)(now-sdci->reftime) / CLOCKS_PER_SEC;
    tlast = (double)(now-sdci->lasttime) / CLOCKS_PER_SEC;
    sdci->lasttime = now;

    sprintf(sdci->logbuffer,"   .. %s took %.2lf seconds (total %.2lf seconds)\n",status,tlast,ttotal);
    (sdci->sdc->pfWriteLog)( sdci->sdc->env, sdci->logbuffer );
}
