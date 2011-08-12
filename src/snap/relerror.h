#ifndef RELERROR_H
#define RELERROR_H

/*
   $Log: relerror.h,v $
   Revision 1.4  2003/05/16 01:20:41  ccrook
   Added option to store all relative covariances in binary file.

   Revision 1.3  1999/05/20 12:20:59  ccrook
   Fixed up handling of aposteriori tests

   Revision 1.2  1999/05/20 10:41:44  ccrook
   Added function for testing relative accuracies of stations against
   specifications.

   Revision 1.1  1996/01/03 22:05:15  CHRIS
   Initial revision

*/

#ifndef RELERROR_H_RCSID
#define RELERROR_H_RCSID "$Id: relerror.h,v 1.4 2003/05/16 01:20:41 ccrook Exp $"
#endif

#ifndef _BINFILE_H
#include "util/binfile.h"
#endif

#ifndef TESTSPEC_H
#include "testspec.h"
#endif

void dump_relative_covariances( BINARY_FILE *b, int dumpall );

void test_relative_accuracy_specs( SpecDef *spec, int apriori, int *stn_testids, int listopts );

#endif
