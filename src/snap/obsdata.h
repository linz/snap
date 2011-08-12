#ifndef _OBSDATA_H
#define _OBSDATA_H

/*
   $Log: obsdata.h,v $
   Revision 1.2  2003/11/25 01:29:58  ccrook
   Updated SNAP to allow calculation of projection bearings in coordinate
   systems other than that of the coordinate file

   Revision 1.1  1996/01/03 22:03:12  CHRIS
   Initial revision

*/

#ifndef OBSDATA_H_RCSID
#define OBSDATA_H_RCSID "$Id: obsdata.h,v 1.2 2003/11/25 01:29:58 ccrook Exp $"
#endif

#ifndef _LOADDATA_H
#include "snapdata/loaddata.h"  /* For definitions of data structures */
#endif

#ifndef _BINDATA2_H
#include "bindata2.h"  /* For definition of lsdata */
#endif

void list_obsdata( FILE *out, survdata *o ) ;
int obsdata_obseq( survdata *o, void *hA, int nextra ) ;
void calc_obsdata_residuals( survdata *o, lsdata *l );
void list_obsdata_residuals( FILE *out, survdata *o, double semult);

#endif

