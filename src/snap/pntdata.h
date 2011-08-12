
/*
   $Log: pntdata.h,v $
   Revision 1.2  2003/11/25 01:29:59  ccrook
   Updated SNAP to allow calculation of projection bearings in coordinate
   systems other than that of the coordinate file

   Revision 1.1  1996/01/03 22:04:30  CHRIS
   Initial revision

*/

#ifndef PNTDATA_H_RCSID
#define PNTDATA_H_RCSID "$Id: pntdata.h,v 1.2 2003/11/25 01:29:59 ccrook Exp $"
#endif
/* pntdata.h: routines for handling point related data, lat, long,
   doppler, etc... */

#ifndef _PNTDATA_H
#define _PNTDATA_H

#ifndef _LOADDATA_H
#include "snapdata/loaddata.h"
#endif

#ifndef _BINDATA2_h
#include "bindata2.h"
#endif

int pntdata_obseq( survdata *p, void *hA, int nextra );
void list_pntdata( FILE *out, survdata *p );
void list_pntdata_residuals( FILE *out, survdata *o, double semult );
void calc_pntdata_residuals( survdata *o, lsdata *l );


#endif



