#ifndef TESTSPEC_H
#define TESTSPEC_H

#include <string>
#include <string_view>
#include <boost/algorithm/string/case_conv.hpp>

/*

  $Log: testspec.h,v $
  Revision 1.2  2003/10/23 01:29:05  ccrook
  Updated to support absolute accuracy tests

  Revision 1.1  1999/05/20 10:41:20  ccrook
  Initial revision


*/


struct SpecDef
{
    /// name is upper-cased, matching the original's _strupr(), since spec
    /// names are matched case-insensitively but stored upper-case.
    SpecDef( std::string_view name, double conf,
             int goth, double habs, double hppm, double hmax,
             int gotv, double vabs, double vppm, double vmax ) :
        next(nullptr), name(boost::algorithm::to_upper_copy(std::string(name))), confidence(conf),
        gothtol(goth), htolabs(habs), htolppm(hppm), htolmax(hmax), htolfactor(0.0),
        gotvtol(gotv), vtolabs(vabs), vtolppm(vppm), vtolmax(vmax), vtolfactor(0.0),
        testid(0)
    {
    }

    struct SpecDef * next;          ///< mutated later, appending a new node
    const std::string name;
    const double confidence;
    const int gothtol;
    const double htolabs;
    const double htolppm;
    const double htolmax;
    double htolfactor;              ///< computed later, at test time
    const int gotvtol;
    const double vtolabs;
    const double vtolppm;
    const double vtolmax;
    double vtolfactor;              ///< computed later, at test time
    int testid;                     ///< lazily assigned on first use
};

int define_spec( std::string_view name, double conf,
                 int goth, double habs, double hppm, double hmax,
                 int gotv, double vabs, double vppm, double vmax );

void set_spec_apriori( int isapriori );

void set_spec_listoption( int option );

int get_spec_testid( std::string_view name, int *testid );

int set_station_spec_testid( int stnid, int testid, int add );

void test_specifications( void );

extern int do_accuracy_tests;

#define SPEC_LIST_NONE 0
#define SPEC_LIST_FAIL 1
#define SPEC_LIST_ALL  2


#endif  /* TESTSPEC_H defined */

