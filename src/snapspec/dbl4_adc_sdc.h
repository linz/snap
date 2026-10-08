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

#include <cstddef>
#include <string_view>
#include <vector>

#include "dbl4_types.h"
#include "util/stringlimited.hpp"

/// The most characters in an order code
inline constexpr size_t ORDER_CODE_LEN = 4;

using OrderCode = StringLimited<ORDER_CODE_LEN>;

struct SDCOrderTest
{
    const IdType idOrder;      ///< Order of the nodes passing the test
    const OrderCode scOrder;   ///< Order display code
    bool    blnAutoRange = false;  ///< Range is calculated based on nearest control

    bool    blnTestHor = false;    ///< Test horizontal accuracy
    double  dblRange = 0.0;        ///< Range used in rel accuracy test - <=0 for no limit
    int     iMinRelAcc = 0;        ///< Minimum number of relative accuracy tests
    double  dblAbsTestAbsMax = 1000.0;  ///< Absolute test fail limit
    double  dblAbsTestDDMax = 1000.0;   ///< Relative to control dist dep m/100m
    double  dblAbsTestDFMax = 1000.0;   ///< Relative to control fixed component
    double  dblRelTestAbsMin = 0.0;     ///< Rel Acc by absolute accuracy limit
    double  dblRelTestDDMax = 0.0;      ///< Rel Acc dist dependent m/100m
    double  dblRelTestDFMax = 1000.0;   ///< Rel Accuracy fixed component

    bool    blnTestVrt = false;         ///< Test vertical accuracy
    double  dblAbsTestAbsMaxV = 1000.0; ///< Absolute test fail limit
    double  dblAbsTestDDMaxV = 1000.0;  ///< Relative to control dist dep m/100m
    double  dblAbsTestDFMaxV = 1000.0;  ///< Relative to control fixed component
    double  dblRelTestAbsMinV = 0.0;    ///< Rel Acc by absolute accuracy limit
    double  dblRelTestDDMaxV = 0.0;     ///< Rel Acc dist dependent m/100m
    double  dblRelTestDFMaxV = 1000.0;  ///< Rel Accuracy fixed component

    /// Ratio of vert/horizontal accuracies when determining station with
    /// maximum error to reject
    double  dblVertHorRatio = 0.0;

    SDCOrderTest( const IdType order,                ///< Order of the nodes passing the test
                  const std::string_view code );     ///< Order display code
};

#define SDC_IGNORE_MARK    -1
#define SDC_CONTROL_MARK   -2

#define SDC_LOG_STEPS 1
#define SDC_LOG_TESTS 2
#define SDC_LOG_CALCS 4
#define SDC_LOG_DISTS 8
#define SDC_LOG_CALCS2 16
#define SDC_LOG_TIMESTAMP 32
#define SDC_LOG_SUMMARY 64
#define SDC_LOG_COMPACT 256 
#define SDC_LOG_ALL (SDC_LOG_SUMMARY | SDC_LOG_STEPS | SDC_LOG_TESTS | SDC_LOG_CALCS | SDC_LOG_DISTS | SDC_LOG_CALCS2)

/* Covariance determination run in two passes if not all available at first pass */

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

#define SDC_DEFAULT  -1  /* Passed to SDCTest.pfSetOrder for the default order */

#define SDC_NO_PRIORITY -1 /* Lowest ranking station priority */

/* Return value for covariance unavailable */

#define SDC_COVAR_UNAVAILABLE -1.0

struct SDCTest
{
    void *env = nullptr;   ///< Environment passed to function pointers
    int  nmark = 0;        ///< Number of marks - ids are 0 .. nmark-1
    const int maxorder;    ///< The most orders that tests can hold
    int  options = 0;      ///< Options controlling application of SDC algorithm
    int  useKDTree = 0;    ///< Non-zero to build KD-tree spatial indices (also requires range limits)
    int  loglevel = 0;     ///< Greater than 0 for logging
    std::vector<SDCOrderTest> tests;  ///< The definitions of each test, one per order
    const OrderCode scFailOrder;      ///< Display string for fail order
    double dblErrFactor = 3.0;        ///< Factor by which errors are multiplied for test

    /// Creates a test with no orders defined
    explicit SDCTest( const int orderCapacity );  ///< The most orders that can be defined

    /// The number of orders in the test
    int norder() const;

    long (*pfStationId) ( /* Function to get the id of the station */
        void *env,
        int  stn ) = nullptr;

    int (*pfStationRole) ( /* Function to get the role of the station in the  tests */
        void *env,         /* Returns one of the above status, or the lowest number */
        int  stn ) = nullptr;  /* test to apply */

    int (*pfStationPriority) ( /* Function to get the priority of the station in the  tests */
        void *env,         /* Returns an integer value used to choose potential */
        int  stn ) = nullptr;  /* stations to discard when all have failing tests */
                           /* Choose SDC_NO_PRIORITY or a highest numeric priority */

    double (*pfDistance2) ( /* Function to get square of the distance between two marks */
        void *env,
        int stn1,
        int stn2 ) = nullptr;

    double (*pfError2) (   /* Get the relative error between two marks */
        void *env,         /* Returns the square of the semi-major axis */
        int stn1,
        int stn2 ) = nullptr;

    double (*pfVrtError2) (   /* Get the vertical relative error between two marks */
        void *env,           /* Returns the square of the vertical error */
        int stn1,
        int stn2 ) = nullptr;

    void (*pfRequestCovar) (   /* Requests covariance information between */
        void *env,               /* stations */
        int stn1,
        int stn2 ) = nullptr;

    int  (*pfCalcRequested) (   /* Calculates the requested covariances */
        void *env ) = nullptr;

    void (*pfSetOrder) (   /* Sets the order for a mark */
        void *env,
        int stn,
        int order ) = nullptr;

    void (*pfWriteLog) (   /* Writes log information */
        void *env,
        std::string_view text ) = nullptr;

    void (*pfWriteCompact) (   /* Writes compact log information */
        void *env,
        std::string_view text ) = nullptr;
};

StatusType sdcCalcSDCOrders( SDCTest *sdc );

StatusType sdcCalcSDCOrders2( SDCTest *sdc, int minorder );

#endif  /* define DBL4_ADC_SDC_H */
