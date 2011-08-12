#ifndef _VECDATA_H
#define _VECDATA_H

/*
   $Log: vecdata.h,v $
   Revision 1.2  2003/11/25 01:30:00  ccrook
   Updated SNAP to allow calculation of projection bearings in coordinate
   systems other than that of the coordinate file

   Revision 1.1  1996/01/03 22:13:59  CHRIS
   Initial revision

*/

#ifndef VECDATA_H_RCSID
#define VECDATA_H_RCSID "$Id: vecdata.h,v 1.2 2003/11/25 01:30:00 ccrook Exp $"
#endif

#ifndef _LOADDATA_H
#include "snapdata/loaddata.h"
#endif

#ifndef _BINDATA2_H
#include "bindata2.h"
#endif

/* Define whether midpoint used to define gps vertical, or topocentre used */

void list_vecdata( FILE *out, survdata  *v ) ;
int vecdata_obseq( survdata  *v, void *hA, int nextra ) ;
void calc_vecdata_residuals( survdata  *v, lsdata *l );
void list_vecdata_residuals( FILE *out, survdata  *v, double semult);

#endif
