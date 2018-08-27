#ifndef DBL4_ADC_SDC_H
#define DBL4_ADC_SDC_H
/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
**  Description:
**      Header for code applying SDC order algorithm
**
** $Id: dbl4_adc_sdc.h,v 1.1 2003/05/28 01:40:45 ccrook Exp $
**
**************************************************************************
*/

#include "dbl4_types.h"

typedef struct
{
    IdType  idOrder;       /**< Order of the nodes passing the test */
    SysCodeType scOrder;   /**< Order display code */
    Boolean blnAutoRange;  /**< Range is calculated based on nearest control */

    Boolean blnTestHor;    /**< Test horizontal accuracy */
    double  dblRange;      /**< Range used in rel accuracy test - <=0 for no limit */
    double  dblAbsTestAbsMax;  /**< Absolute test fail limit */
    double  dblAbsTestDDMax;   /**< Relative to control dist dep m/100m */
    double  dblAbsTestDFMax;   /**< Relative to control fixed component */
    double  dblRelTestAbsMin;  /**< Rel Acc by absolute accuracy limit */
    double  dblRelTestDDMax;   /**< Rel Acc dist dependent m/100m */
    double  dblRelTestDFMax;   /**< Rel Accuracy fixed component */

    Boolean blnTestVrt;   /**< Test vertical accuracy */
    double  dblAbsTestAbsMaxV;  /**< Absolute test fail limit */
    double  dblAbsTestDDMaxV;   /**< Relative to control dist dep m/100m */
    double  dblAbsTestDFMaxV;   /**< Relative to control fixed component */
    double  dblRelTestAbsMinV;  /**< Rel Acc by absolute accuracy limit */
    double  dblRelTestDDMaxV;   /**< Rel Acc dist dependent m/100m */
    double  dblRelTestDFMaxV;   /**< Rel Accuracy fixed component */
    double  dblVertHorRatio;    /**< Ratio of vert/horizontal accuracies when
                                  determining station with maximum error
                                  to reject */

} SDCOrderTest, *hSDCOrderTest;

#define SDC_IGNORE_MARK    -1
#define SDC_CONTROL_MARK   -2

#define SDC_LOG_STEPS 1
#define SDC_LOG_TESTS 2
#define SDC_LOG_CALCS 4
#define SDC_LOG_DISTS 8
#define SDC_LOG_CALCS2 16
#define SDC_LOG_DEBUG (SDC_LOG_STEPS | SDC_LOG_TESTS | SDC_LOG_CALCS | SDC_LOG_DISTS | SDC_LOG_CALCS2)
#define SDC_LOG_TIMESTAMP 32

/* Convariance determination run in two passes if not all available at first pass */

#define SDC_OPT_TWOPASS_CVR  1

/* Options defining short circuit of covariance calculations.

   SDC_OPT_NO_SHORTCIRCUIT_CVR flags that station variances will not be used to
      as first pass test covariance is within bounds before calculating
      covariance to confirm

   SDC_OPT_STRICT_SHORTCIRCUIT_CVR flags that this will be done strictly, rather
      than assuming that variances will not be negatively correlated.  If 0 assume
      V12 < V1+V2.  Otherwise use V12 < (V1 + 2*sqrt(V1*V2) + V2)

*/

#define SDC_OPT_NO_SHORTCIRCUIT_CVR     2
#define SDC_OPT_STRICT_SHORTCIRCUIT_CVR 4

/* Option to reject mark if no relative accuracy tests available */

#define SDC_OPT_FAIL_NORELACC           8

#define SDC_DEFAULT  -1  /* Passed to SDCTest.pfSetOrder for the default order */

#define SDC_NO_PRIORITY -1 /* Lowest ranking station priority */

/* Return value for covariance unavailable */

#define SDC_COVAR_UNAVAILABLE -1.0

typedef struct
{
    void *env;         /**< Environment passed to function pointers */
    int  nmark;        /**< Number of marks - ids are 0 .. nmark-1  */
    int  norder;           /**< The number of orders in the test */
    int  maxorder;         /**< The number of orders allocated in tests */
    int  options;          /**< Options controlling application of SDC algorithm */
    int  loglevel;         /**< Greater than 0 for logging */
    SDCOrderTest *tests;   /**< The definitions of each test */
    IdType idFailOrder;    /**< The order to apply if all tests fail */
    SysCodeType scFailOrder;  /**< Display string for fail order */
    double dblErrFactor;   /**< Factor by which errors are multiplied for test */

    long (*pfStationId) ( /* Function to get the id of the station */
        void *env,
        int  stn );

    int (*pfStationRole) ( /* Function to get the role of the station in the  tests */
        void *env,         /* Returns one of the above status, or the lowest number */
        int  stn );        /* test to apply */

    int (*pfStationPriority) ( /* Function to get the priority of the station in the  tests */
        void *env,         /* Returns an integer value used to choose potential */
        int  stn );        /* stations to discard when all have failing tests */
                           /* Choose SDC_NO_PRIORITY or a highest numeric priority */

    double (*pfDistance2) ( /* Function to get square of the distance between two marks */
        void *env,
        int stn1,
        int stn2 );

    double (*pfError2) (   /* Get the relative error between two marks */
        void *env,         /* Returns the square of the semi-major axis */
        int stn1,
        int stn2 );

    double (*pfVrtError2) (   /* Get the vertical relative error between two marks */
        void *env,           /* Returns the square of the vertical error */
        int stn1,
        int stn2 );

    void (*pfRequestCovar) (   /* Requests covariance information between */
        void *env,               /* stations */
        int stn1,
        int stn2 );

    int  (*pfCalcRequested) (   /* Calculates the requested covariances */
        void *env );

    void (*pfSetOrder) (   /* Sets the order for a mark */
        void *env,
        int stn,
        int order );

    void (*pfWriteLog) (   /* Writes log information */
        void *env,
        const char *text );
} SDCTest, *hSDCTest;

hSDCTest sdcCreateSDCTest( int maxorder );

StatusType sdcCalcSDCOrders( hSDCTest sdc );

StatusType sdcCalcSDCOrders2( hSDCTest sdc, int minorder );

void sdcDropSDCTest( hSDCTest sdc );

#endif  /* define DBL4_ADC_SDC_H */
