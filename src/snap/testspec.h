#ifndef TESTSPEC_H
#define TESTSPEC_H

/*

  $Log: testspec.h,v $
  Revision 1.2  2003/10/23 01:29:05  ccrook
  Updated to support absolute accuracy tests

  Revision 1.1  1999/05/20 10:41:20  ccrook
  Initial revision


*/


#ifndef TESTSPEC_H_RCSID
#define TESTSPEC_H_RCSID "$Id: testspec.h,v 1.2 2003/10/23 01:29:05 ccrook Exp $"
#endif

typedef struct SpecDef_s
{
    struct SpecDef_s * next;
    char *name;
    double confidence;
    int gothtol;
    double htolabs;
    double htolppm;
    double htolmax;
    double htolfactor;
    int gotvtol;
    double vtolabs;
    double vtolppm;
    double vtolmax;
    double vtolfactor;
    int  testid;
} SpecDef;

int define_spec( char *name, double conf,
                 int goth, double habs, double hppm, double hmax,
                 int gotv, double vabs, double vppm, double vmax );

void set_spec_apriori( int isapriori );

void set_spec_listoption( int option );

int get_spec_testid( char *name, int *testid );

int set_station_spec_testid( int stnid, int testid, int add );

void test_specifications( void );

extern int do_accuracy_tests;

#define SPEC_LIST_NONE 0
#define SPEC_LIST_FAIL 1
#define SPEC_LIST_ALL  2


#endif  /* TESTSPEC_H defined */

