#ifndef _RFTRNDMP_H
#define _RFTRNDMP_H

/*
   $Log: rftrndmp.h,v $
   Revision 1.1  1995/12/22 17:47:38  CHRIS
   Initial revision

*/

#ifndef _BINFILE_H
#include "util/binfile.h"
#endif

void dump_rftransformations( BINARY_FILE *b );
int reload_rftransformations( BINARY_FILE *b );

#endif
